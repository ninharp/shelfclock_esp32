#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"
#include "rtttl_player.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <time.h>

int64_t g_countdown_end_us = 0;
static int64_t s_duration_us = 0;

void mode_countdown_start(int32_t duration_ms) {
    s_duration_us      = (int64_t)duration_ms * 1000;
    g_countdown_end_us = esp_timer_get_time() + s_duration_us;
}

void end_countdown(void) {
    all_blank();
    if (g_config.use_audible_alarm) {
        time_t now = time(NULL);
        struct tm ti;
        localtime_r(&now, &ti);
        int mday = ti.tm_mday, mont = ti.tm_mon + 1;
        int idx = esp_random() % 11;
        if (mday == 22 && mont == 10) idx = RTTTL_SONG_BIRTHDAY;
        else if (mday == 25 && mont == 12) idx = RTTTL_SONG_XMAS;
        else if (mday == 4  && mont == 5)  idx = RTTTL_SONG_STARWARS;
        else if (mday == 8  && mont == 9)  idx = 9;
        rtttl_play_song(idx);
    }
    int cw = 0;
    for (int i = 0; i < 300 && !atomic_load(&g_rtttl_breakout); i++) {
        crgb_t c = color_wheel(cw++ & 0xFF);
        display_number(38, 5, c); display_number(79, 3, c); display_number(69, 1, c);
        led_refresh();
        vTaskDelay(pdMS_TO_TICKS(16));
    }
    scroll("tIMEr Ended      tIMEr Ended");
    all_blank();
    g_countdown_end_us = 0;
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    g_config.clock_mode    = 0;
    g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    storage_save_all();
}

void mode_countdown_update(void) {
    if (g_countdown_end_us == 0) return;
    int64_t now_us = esp_timer_get_time();
    if (now_us >= g_countdown_end_us) { end_countdown(); return; }
    int64_t rest_ms = (g_countdown_end_us - now_us) / 1000;
    uint32_t hours   = (uint32_t)((rest_ms / 1000) / 3600);
    uint32_t minutes = (uint32_t)((rest_ms / 1000) / 60);
    uint32_t seconds = (uint32_t)(rest_ms / 1000);
    uint32_t rem_sec = seconds - minutes * 60;
    uint32_t rem_min = minutes - hours * 60;
    uint8_t h1 = hours / 10,   h2 = hours % 10;
    uint8_t m1 = rem_min / 10, m2 = rem_min % 10;
    uint8_t s1 = rem_sec / 10, s2 = rem_sec % 10;
    crgb_t col = CRGB(g_config.cd_r, g_config.cd_g, g_config.cd_b);
    if (rest_ms <= 10000 && g_config.color_change_cd) col = CRGB_RED;
    if (hours > 0) {
        display_number(h1, 6, col); display_number(h2, 4, col);
        display_number(m1, 2, col); display_number(m2, 0, col);
    } else {
        display_number(m1, 6, col); display_number(m2, 4, col);
        display_number(s1, 2, col); display_number(s2, 0, col);
    }
}
