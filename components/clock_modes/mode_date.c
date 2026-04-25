#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"
#include "esp_random.h"
#include <time.h>
#include <string.h>

static crgb_t pick(uint8_t cs, int ri) {
    bool pastel = g_config.pastel_colors;
    uint8_t ccf = g_config.color_change_frequency;
    bool trigger = (ccf == 0) || (ccf == 1 && g_flag_min) || (ccf == 2 && g_flag_hour) ||
                   (ccf == 3 && g_flag_day) || (ccf == 4 && g_flag_week) || (ccf == 5 && g_flag_month);
    if (cs == 0 || cs == 1) return CRGB(g_config.r[ri], g_config.g[ri], g_config.b[ri]);
    if ((cs == 2 || cs == 3) && trigger) return random_color(pastel);
    return CRGB(g_config.r[ri], g_config.g[ri], g_config.b[ri]);
}

void mode_date_update(void) {
    time_t now = time(NULL);
    struct tm ti;
    localtime_r(&now, &ti);
    int mday = ti.tm_mday, mont = ti.tm_mon + 1, year = (ti.tm_year + 1900) - 2000;
    uint8_t d1 = mday / 10, d2 = mday % 10;
    uint8_t m1 = mont / 10, m2 = mont % 10;
    uint8_t y1 = year / 10, y2 = year % 10;
    uint8_t cs = g_config.date_color_settings;
    crgb_t dc = pick(cs, 4);
    crgb_t mc = (cs == 1 || cs == 3) ? dc : pick(cs, 5);
    crgb_t sc = (cs == 3) ? dc : pick(cs, 6);

    switch (g_config.date_display_type) {
    case 0:
        display_number(m1 < 1 ? 0 : m1, 6, mc);
        display_number(m2, 4, mc);
        display_number(d1, 2, dc);
        display_number(d2, 0, dc);
        break;
    case 1:
        display_number(m1 < 1 ? 10 : m1, 6, mc);
        display_number(m2, 4, mc);
        display_number(d1, 2, dc);
        display_number(d2, 0, dc);
        break;
    case 2:
        if (m1 > 0) {
            crgb_t tm = (cs == 4) ? random_color(g_config.pastel_colors) : mc;
            for (int i = 32 * LEDS_PER_SEGMENT; i < 33 * LEDS_PER_SEGMENT; i++) {
                if (i < NUM_LEDS) g_leds[i] = tm;
            }
            tm = (cs == 4) ? random_color(g_config.pastel_colors) : mc;
            for (int i = 33 * LEDS_PER_SEGMENT; i < 34 * LEDS_PER_SEGMENT; i++) {
                if (i < NUM_LEDS) g_leds[i] = tm;
            }
        } else {
            for (int i = 32 * LEDS_PER_SEGMENT; i < 34 * LEDS_PER_SEGMENT; i++) {
                if (i < NUM_LEDS) g_leds[i] = CRGB_BLACK;
            }
        }
        display_number(m2, 5, mc);
        display_number(d1, 3, dc);
        display_number(d2, 1, dc);
        break;
    case 3:
        switch (ti.tm_wday) {
            case 1: display_number(78, 5, dc); display_number(80, 3, dc); display_number(79, 1, dc); break;
            case 2: display_number(85, 6, dc); display_number(54, 4, dc); display_number(38, 2, dc); display_number(52, 0, dc); break;
            case 3: display_number(88, 5, dc); display_number(38, 3, dc); display_number(69, 1, dc); break;
            case 4: display_number(85, 6, dc); display_number(73, 4, dc); display_number(86, 2, dc); display_number(83, 0, dc); break;
            case 5: display_number(39, 5, dc); display_number(83, 3, dc); display_number(42, 1, dc); break;
            case 6: display_number(52, 5, dc); display_number(34, 3, dc); display_number(85, 1, dc); break;
            case 0: display_number(52, 5, dc); display_number(86, 3, dc); display_number(79, 1, dc); break;
        }
        break;
    case 4:
        if (d1 < 1) display_number(d2, 3, dc);
        else { display_number(d1, 4, dc); display_number(d2, 2, dc); }
        break;
    case 5:
        if (m1 > 0) {
            crgb_t tm = (cs == 4) ? random_color(g_config.pastel_colors) : mc;
            for (int i = 32 * LEDS_PER_SEGMENT; i < 33 * LEDS_PER_SEGMENT; i++) {
                if (i < NUM_LEDS) g_leds[i] = tm;
            }
            tm = (cs == 4) ? random_color(g_config.pastel_colors) : mc;
            for (int i = 33 * LEDS_PER_SEGMENT; i < 34 * LEDS_PER_SEGMENT; i++) {
                if (i < NUM_LEDS) g_leds[i] = tm;
            }
        } else {
            for (int i = 32 * LEDS_PER_SEGMENT; i < 34 * LEDS_PER_SEGMENT; i++) {
                if (i < NUM_LEDS) g_leds[i] = CRGB_BLACK;
            }
        }
        display_number(m2, 5, mc);
        display_number(d1, 2, dc);
        display_number(d2, 0, dc);
        for (int i = 20 * LEDS_PER_SEGMENT; i < 21 * LEDS_PER_SEGMENT; i++) {
            if (i < NUM_LEDS) g_leds[i] = sc;
        }
        break;
    case 6:
        display_number(2, 6, mc);
        display_number(0, 4, mc);
        display_number(y1, 2, dc);
        display_number(y2, 0, dc);
        break;
    }
}
