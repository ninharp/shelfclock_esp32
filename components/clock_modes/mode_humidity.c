#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"
#include "esp_random.h"

void mode_humidity_update(void) {
    float humi = g_sensors.humidity;
    uint8_t cs = g_config.humi_color_settings;
    bool pastel = g_config.pastel_colors;
    bool trigger = (g_config.color_change_frequency == 0) ||
                   (g_config.color_change_frequency == 1 && g_flag_min) ||
                   (g_config.color_change_frequency == 2 && g_flag_hour);
    crgb_t hc = (cs >= 2 && trigger) ? random_color(pastel) :
                CRGB(g_config.r[10], g_config.g[10], g_config.b[10]);
    crgb_t sc = (cs == 1 || (cs == 3 && trigger)) ? hc :
                ((cs >= 2 && trigger) ? random_color(pastel) :
                CRGB(g_config.r[11], g_config.g[11], g_config.b[11]));
    crgb_t dc = (cs == 3 && trigger) ? hc :
                ((cs >= 2 && trigger) ? random_color(pastel) :
                CRGB(g_config.r[12], g_config.g[12], g_config.b[12]));

    int humi_dec = (int)(humi * 10);
    uint8_t t1 = 0, t2 = 0;
    if (humi >= 100.0f) {
        int th = (int)humi / 10;
        t1 = th / 10;
        t2 = th % 10;
    } else {
        t2 = (int)humi / 10;
    }
    uint8_t t3 = (int)humi % 10;
    uint8_t t4 = humi_dec % 10;

    switch (g_config.humi_display_type) {
    case 0:
        if (humi < 100.0f) {
            display_number(t2, 5, hc); display_number(t3, 3, hc); display_number(41, 1, sc);
        } else {
            display_number(t1, 6, hc); display_number(t2, 4, hc);
            display_number(t3, 2, hc); display_number(41, 0, sc);
        }
        break;
    case 1:
        if (humi < 100.0f) {
            display_number(t2, 6, hc); display_number(t3, 4, hc); display_number(t4, 1, sc);
            crgb_t pc = (cs == 4) ? random_color(pastel) : dc;
            for (int i = 12 * LEDS_PER_SEGMENT; i < 13 * LEDS_PER_SEGMENT; i++) {
                if (i < NUM_LEDS) g_leds[i] = pc;
            }
        } else {
            display_number(t2, 5, hc); display_number(t3, 3, hc); display_number(t4, 0, sc);
            crgb_t th = (cs == 4) ? random_color(pastel) : hc;
            for (int i = 32 * LEDS_PER_SEGMENT; i < 34 * LEDS_PER_SEGMENT; i++) {
                if (i < NUM_LEDS) g_leds[i] = th;
            }
            crgb_t pc = (cs == 4) ? random_color(pastel) : dc;
            for (int i = 10 * LEDS_PER_SEGMENT; i < 11 * LEDS_PER_SEGMENT; i++) {
                if (i < NUM_LEDS) g_leds[i] = pc;
            }
        }
        break;
    case 2:
        if (humi < 100.0f) {
            display_number(t2, 4, hc); display_number(t3, 2, sc);
        } else {
            display_number(t1, 5, dc); display_number(t2, 3, hc); display_number(t3, 1, sc);
        }
        break;
    }
}
