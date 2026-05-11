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

extern bool g_digit_anim_active;

int64_t g_countdown_end_us = 0;
static int64_t s_duration_us = 0;
static bool s_countdown_ended = false;

static digit_anim_t s_anims[7] = {
    DIGIT_ANIM_INIT, DIGIT_ANIM_INIT, DIGIT_ANIM_INIT, DIGIT_ANIM_INIT,
    DIGIT_ANIM_INIT, DIGIT_ANIM_INIT, DIGIT_ANIM_INIT
};

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

bool mode_countdown_check_ended(void) {
    if (s_countdown_ended) { s_countdown_ended = false; return true; }
    return false;
}

void mode_countdown_update(void) {
    if (g_countdown_end_us == 0) return;
    int64_t now_us = esp_timer_get_time();
    if (now_us >= g_countdown_end_us) {
        s_countdown_ended = true;
        g_countdown_end_us = 0;
        display_number(38, 5, CRGB_WHITE);  /* E */
        display_number(79, 3, CRGB_WHITE);  /* n */
        display_number(69, 1, CRGB_WHITE);  /* d */
        return;
    }
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
    bool any = false;
    if (hours > 0) {
        any |= display_number_animated(h1, 6, col, &s_anims[6]);
        any |= display_number_animated(h2, 4, col, &s_anims[4]);
        any |= display_number_animated(m1, 2, col, &s_anims[2]);
        any |= display_number_animated(m2, 0, col, &s_anims[0]);
    } else {
        any |= display_number_animated(m1, 6, col, &s_anims[6]);
        any |= display_number_animated(m2, 4, col, &s_anims[4]);
        any |= display_number_animated(s1, 2, col, &s_anims[2]);
        any |= display_number_animated(s2, 0, col, &s_anims[0]);
    }
    g_digit_anim_active = any;
}
