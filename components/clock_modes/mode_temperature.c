#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"
#include "esp_random.h"
#include <math.h>

void mode_temperature_update(void) {
    float temp = g_sensors.temperature_c + g_config.temperature_correction;
    if (g_config.temperature_symbol == 39) {
        temp = (g_sensors.temperature_c * 1.8f + 32.0f) + g_config.temperature_correction;
    }
    uint8_t cs = g_config.temp_color_settings;
    bool pastel = g_config.pastel_colors;
    bool trigger = (g_config.color_change_frequency == 0) ||
                   (g_config.color_change_frequency == 1 && g_flag_min) ||
                   (g_config.color_change_frequency == 2 && g_flag_hour);
    crgb_t tc = (cs >= 2 && trigger) ? random_color(pastel) : CRGB(g_config.r[7], g_config.g[7], g_config.b[7]);
    crgb_t vc = (cs == 1 || (cs == 3 && trigger)) ? tc :
                ((cs >= 2 && trigger) ? random_color(pastel) : CRGB(g_config.r[8], g_config.g[8], g_config.b[8]));
    crgb_t dc = (cs == 3 && trigger) ? tc :
                ((cs >= 2 && trigger) ? random_color(pastel) : CRGB(g_config.r[9], g_config.g[9], g_config.b[9]));

    int temp_dec = (int)(temp * 10);
    uint8_t t1 = 0, t2 = 0, t3, t4;
    if (temp >= 100.0f) {
        int th = (int)temp / 10;
        t1 = th / 10;
        t2 = th % 10;
    } else {
        t2 = (int)temp / 10;
    }
    t3 = (int)temp % 10;
    t4 = temp_dec % 10;

    uint8_t sym = g_config.temperature_symbol;

    switch (g_config.temp_display_type) {
    case 0:
        if (temp < 100.0f) {
            display_number(t2, 6, tc); display_number(t3, 4, tc);
            display_number(26, 2, dc); display_number(sym, 0, vc);
        } else {
            display_number(t1, 6, tc); display_number(t2, 4, tc);
            display_number(t3, 2, tc); display_number(sym, 0, vc);
        }
        break;
    case 1:
        if (temp < 100.0f) {
            display_number(t2, 5, tc); display_number(t3, 3, tc); display_number(sym, 1, vc);
        } else {
            display_number(t1, 6, tc); display_number(t2, 4, tc);
            display_number(t3, 2, tc); display_number(sym, 0, vc);
        }
        break;
    case 2:
        if (temp < 100.0f) {
            display_number(t2, 5, tc); display_number(t3, 3, tc); display_number(26, 1, dc);
        } else {
            display_number(t1, 6, tc); display_number(t2, 4, tc);
            display_number(t3, 2, tc); display_number(26, 0, dc);
        }
        break;
    case 3:
        if (temp < 100.0f) {
            display_number(t2, 6, tc); display_number(t3, 4, tc); display_number(t4, 1, vc);
            crgb_t pc = (cs == 4) ? random_color(pastel) : dc;
            for (int i = 12 * LEDS_PER_SEGMENT; i < 13 * LEDS_PER_SEGMENT; i++) {
                if (i < NUM_LEDS) g_leds[i] = pc;
            }
        } else {
            display_number(t2, 5, tc); display_number(t3, 3, tc); display_number(t4, 0, vc);
            crgb_t tth = (cs == 4) ? random_color(pastel) : tc;
            for (int i = 32 * LEDS_PER_SEGMENT; i < 34 * LEDS_PER_SEGMENT; i++) {
                if (i < NUM_LEDS) g_leds[i] = tth;
            }
            crgb_t pc = (cs == 4) ? random_color(pastel) : dc;
            for (int i = 10 * LEDS_PER_SEGMENT; i < 11 * LEDS_PER_SEGMENT; i++) {
                if (i < NUM_LEDS) g_leds[i] = pc;
            }
        }
        break;
    case 4:
        if (temp < 100.0f) {
            display_number(t2, 4, tc); display_number(t3, 2, vc);
        } else {
            display_number(t1, 5, dc); display_number(t2, 3, tc); display_number(t3, 1, vc);
        }
        break;
    }
}
