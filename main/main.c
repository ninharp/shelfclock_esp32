#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_sntp.h"
#include <time.h>
#include <string.h>
#include <sys/time.h>

#include "storage.h"
#include "led_display.h"
#include "sensors.h"
#include "clock_modes.h"
#include "lightshow.h"
#include "rtttl_player.h"
#include "web_server.h"
#include "wifi_manager.h"

static const char *TAG = "main";

// ── NTP-Sync ──────────────────────────────────────────────────────────────────
static void ntp_sync_start(void) {
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_init();

    int gmt_h = g_config.gmt_offset_sec / 3600;
    char tz[32];
    snprintf(tz, sizeof(tz), "UTC%d", -gmt_h);
    setenv("TZ", tz, 1);
    tzset();

    int retry = 0;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        retry++;
    }
    if (sntp_get_sync_status() != SNTP_SYNC_STATUS_RESET) {
        ESP_LOGI(TAG, "NTP synchronized");
        time_t now = time(NULL);
        struct tm ti; localtime_r(&now, &ti);
        ds3231_set_time(&ti);
    } else {
        ESP_LOGW(TAG, "NTP sync failed — RTC fallback");
        struct tm rtc_time;
        if (ds3231_get_time(&rtc_time) && !ds3231_lost_power()) {
            time_t t = mktime(&rtc_time);
            struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
            settimeofday(&tv, NULL);
            ESP_LOGI(TAG, "RTC time applied");
        }
    }
}

// ── Sensor-Update ─────────────────────────────────────────────────────────────
static void update_sensors(void) {
    static int64_t s_dht_last = 0;
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms - s_dht_last >= 2000) {
        float tc = 0, h = 0;
        if (dht11_read(&tc, &h)) {
            g_sensors.temperature_c = tc;
            g_sensors.temperature_f = tc * 1.8f + 32.0f;
            g_sensors.humidity = h;
        }
        s_dht_last = now_ms;
    }
    g_sensors.light_level = adc_light_read();
    if (g_config.clock_mode != 9) {
        g_sensors.audio_level = i2s_mic_get_level();
    }
}

// ── Helligkeit ────────────────────────────────────────────────────────────────
static void apply_brightness(void) {
    uint8_t bright;
    if (g_config.brightness == 10) {
        bright = adc_light_to_brightness(g_sensors.light_level);
    } else {
        bright = g_config.brightness;
    }
    xSemaphoreTake(g_led_mutex, portMAX_DELAY);
    for (int i = 0; i < NUM_LEDS; i++) {
        g_leds[i].r = (uint8_t)((uint16_t)g_leds[i].r * bright / 255);
        g_leds[i].g = (uint8_t)((uint16_t)g_leds[i].g * bright / 255);
        g_leds[i].b = (uint8_t)((uint16_t)g_leds[i].b * bright / 255);
    }
    xSemaphoreGive(g_led_mutex);
}

// ── Scroll-Overlay prüfen ─────────────────────────────────────────────────────
static void check_scroll_overlay(struct tm *ti, int secs) {
    if (!g_config.scroll_override) return;
    int m1 = ti->tm_min / 10, m2 = ti->tm_min % 10;
    int cm = g_config.clock_mode;
    if (cm == 11 || cm == 1 || cm == 4) return;
    bool do_scroll = false;
    switch (g_config.scroll_frequency) {
        case 1: do_scroll = (secs == 0); break;
        case 2: do_scroll = ((m2 == 0 || m2 == 5) && secs == 0); break;
        case 3: do_scroll = (m2 == 0 && secs == 0); break;
        case 4: do_scroll = (((m1==0&&m2==0)||(m1==1&&m2==5)||(m1==3&&m2==0)||(m1==4&&m2==5)) && secs == 0); break;
        case 5: do_scroll = (((m1==0&&m2==0)||(m1==3&&m2==0)) && secs == 0); break;
        case 6: do_scroll = (g_flag_hour && secs == 0); break;
        default: break;
    }
    if (do_scroll) mode_scroll_overlay();
}

// ── Main Task ─────────────────────────────────────────────────────────────────
static void main_task(void *arg) {
    int prev_min = -1, prev_hour = -1, prev_day = -1, prev_month = -1, prev_week = -1;

    if (wifi_manager_is_connected()) {
        ntp_sync_start();
    } else {
        struct tm rtc_time;
        if (ds3231_get_time(&rtc_time) && !ds3231_lost_power()) {
            time_t t = mktime(&rtc_time);
            struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
            settimeofday(&tv, NULL);
        }
    }

    rtttl_play_song(RTTTL_SONG_SMB);

    bool wish_done_today = false;

    while (1) {
        int64_t tick_start = esp_timer_get_time();

        update_sensors();

        time_t now = time(NULL);
        struct tm ti; localtime_r(&now, &ti);
        int secs = ti.tm_sec;

        g_flag_min   = (ti.tm_min  != prev_min);
        g_flag_hour  = (ti.tm_hour != prev_hour);
        g_flag_day   = (ti.tm_mday != prev_day);
        g_flag_month = (ti.tm_mon  != prev_month);
        int cur_week = (ti.tm_yday + 7 - (ti.tm_wday ? ti.tm_wday - 1 : 6)) / 7;
        g_flag_week  = (cur_week   != prev_week);

        if (g_flag_min)   prev_min   = ti.tm_min;
        if (g_flag_hour)  prev_hour  = ti.tm_hour;
        if (g_flag_day)   prev_day   = ti.tm_mday;
        if (g_flag_month) prev_month = ti.tm_mon;
        if (g_flag_week)  prev_week  = cur_week;

        if (g_flag_day && wifi_manager_is_connected()) {
            sntp_restart();
            vTaskDelay(pdMS_TO_TICKS(2000));
            time_t t2 = time(NULL);
            struct tm ti2; localtime_r(&t2, &ti2);
            ds3231_set_time(&ti2);
        }

        if ((ti.tm_hour == 11 || ti.tm_hour == 23) && ti.tm_min == 11 && secs == 0 && !wish_done_today) {
            wish_done_today = true;
            scroll("MAkE A WISH");
        }
        if (ti.tm_hour == 0 && ti.tm_min == 0) wish_done_today = false;

        check_scroll_overlay(&ti, secs);

        if (g_config.random_spectrum_mode && g_config.clock_mode == 9 && g_flag_min) {
            xSemaphoreTake(g_config_mutex, portMAX_DELAY);
            g_config.spectrum_mode = (uint8_t)(esp_random() % 12);
            xSemaphoreGive(g_config_mutex);
        }

        xSemaphoreTake(g_led_mutex, portMAX_DELAY);
        all_blank();
        switch (g_config.clock_mode) {
            case 0:  mode_time_update();        break;
            case 1:  mode_countdown_update();   break;
            case 2:  mode_temperature_update(); break;
            case 3:  mode_scoreboard_update();  break;
            case 4:  mode_stopwatch_update();   break;
            case 5:  lightshow_dispatch();      break;
            case 7:  mode_date_update();        break;
            case 8:  mode_humidity_update();    break;
            case 9:  /* spectrum: eigenständiger Task */ break;
            case 10: /* display off */          break;
            case 11: mode_scroll_update();      break;
            default: break;
        }
        if (g_config.clock_mode != 9) {
            shelf_down_lights();
        }
        xSemaphoreGive(g_led_mutex);

        if (g_config.clock_mode != 9) {
            apply_brightness();
            xSemaphoreTake(g_led_mutex, portMAX_DELAY);
            led_refresh();
            xSemaphoreGive(g_led_mutex);
        }

        int64_t elapsed = (esp_timer_get_time() - tick_start) / 1000;
        int32_t delay_ms = 1000 - (int32_t)elapsed;
        if (delay_ms > 10) vTaskDelay(pdMS_TO_TICKS(delay_ms));
        else               vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ── Einstiegspunkt ────────────────────────────────────────────────────────────
void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    storage_init();
    storage_load();
    ESP_LOGI(TAG, "Config loaded, mode=%d", g_config.clock_mode);

    led_display_init();
    sensors_init();
    rtttl_player_init();

    bool wifi_ok = wifi_manager_init();

    if (wifi_ok) {
        web_server_init();
        ESP_LOGI(TAG, "Web server ready at http://shelfclock.local");
    }

    xTaskCreate(spectrum_task_fn, "spectrum", 4096, NULL, 6, NULL);
    xTaskCreate(main_task, "main_task", 8192, NULL, 5, NULL);
}
