#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"
#include "esp_timer.h"

int64_t g_countup_start_us = 0;
int64_t g_countup_end_us   = 0;

void mode_stopwatch_start(int32_t duration_ms) {
    g_countup_start_us = esp_timer_get_time();
    g_countup_end_us   = g_countup_start_us + (int64_t)duration_ms * 1000;
}

void mode_stopwatch_update(void) {
    int64_t now_us = esp_timer_get_time();
    if (g_countup_end_us > 0 && now_us >= g_countup_end_us) {
        end_countdown();
        g_countup_start_us = 0;
        g_countup_end_us   = 0;
        return;
    }
    int64_t elapsed_ms = (now_us - g_countup_start_us) / 1000;
    uint32_t hours   = (uint32_t)(elapsed_ms / 3600000);
    uint32_t minutes = (uint32_t)(elapsed_ms / 60000);
    uint32_t seconds = (uint32_t)(elapsed_ms / 1000);
    uint32_t rem_sec = seconds - minutes * 60;
    uint32_t rem_min = minutes - hours * 60;
    uint8_t h1 = hours / 10,   h2 = hours % 10;
    uint8_t m1 = rem_min / 10, m2 = rem_min % 10;
    uint8_t s1 = rem_sec / 10, s2 = rem_sec % 10;
    crgb_t col = CRGB(g_config.cd_r, g_config.cd_g, g_config.cd_b);
    int64_t remaining_us = g_countup_end_us - now_us;
    if (remaining_us > 0 && remaining_us <= 10000000LL && g_config.color_change_cd) col = CRGB_RED;
    if (hours > 0) {
        display_number(h1, 6, col); display_number(h2, 4, col);
        display_number(m1, 2, col); display_number(m2, 0, col);
    } else {
        display_number(m1, 6, col); display_number(m2, 4, col);
        display_number(s1, 2, col); display_number(s2, 0, col);
    }
}
