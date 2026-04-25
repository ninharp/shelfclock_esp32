#include "lightshow.h"
#include "storage.h"
#include "led_display.h"
#include "sensors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
#include <string.h>

static int  s_react     = 0;
static int  s_decay_cnt = 0;
static const int DECAY  = 3;
static int  s_cw_pos    = 0;
static int  s_cw_speed  = 2;

static const uint32_t FIRE_PAL[16] = {
    0x110000,0x330000,0x660000,0x990000,0xCC0000,0xFF0000,
    0xFF3300,0xFF6600,0xFF9900,0xFFCC00,0xFFFF00,0xFFFF33,
    0xFFFF66,0xFFFF99,0xFFFFCC,0xFFFFFF
};
static crgb_t fire_color(int i) {
    int idx = (i * 15) / (SPECTRUM_PIXELS - 1);
    uint32_t c = FIRE_PAL[idx];
    return CRGB((c>>16)&0xFF,(c>>8)&0xFF,c&0xFF);
}

void spectrum_task_fn(void *arg) {
    while (1) {
        if (g_config.clock_mode != 9 || !g_config.realtime_mode) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        int32_t level = i2s_mic_get_level();
        int pre = (int)((long)SPECTRUM_PIXELS * (long)(level >> 8)) / 8192;
        if (pre > s_react) s_react = pre;
        if (s_react > SPECTRUM_PIXELS) s_react = SPECTRUM_PIXELS;

        xSemaphoreTake(g_led_mutex, portMAX_DELAY);
        uint8_t sm = g_config.spectrum_mode;
        for (int i = SPECTRUM_PIXELS-1; i >= 0; i--) {
            int fake_base = i * LEDS_PER_SEGMENT;
            for (int s = 0; s < LEDS_PER_SEGMENT; s++) {
                int real;
                switch (sm) {
                    case  0: real=FAKE_LEDs_C_BMUP[fake_base+s]; break;
                    case  1: real=FAKE_LEDs_C_CMOT[fake_base+s]; break;
                    case  2: real=FAKE_LEDs_C_BLTR[fake_base+s]; break;
                    case  3: real=FAKE_LEDs_C_TLBR[fake_base+s]; break;
                    case  4: real=FAKE_LEDs_C_VERT[fake_base+s]; break;
                    case  5: real=FAKE_LEDs_C_TMDN[fake_base+s]; break;
                    case  6: real=FAKE_LEDs_C_CSIN[fake_base+s]; break;
                    case  7: real=FAKE_LEDs_C_BRTL[fake_base+s]; break;
                    case  8: real=FAKE_LEDs_C_TRBL[fake_base+s]; break;
                    case  9: real=FAKE_LEDs_C_OUTS[fake_base+s]; break;
                    case 10: real=FAKE_LEDs_C_VERT2[fake_base+s]; break;
                    default: real=FAKE_LEDs_C_OUTS2[fake_base+s]; break;
                }
                if (real >= NUM_LEDS) continue;
                crgb_t col;
                if (i < s_react) {
                    uint8_t cs = g_config.spectrum_color_settings;
                    if (cs == 0) col = CRGB(g_config.r[15],g_config.g[15],g_config.b[15]);
                    else if (cs == 1) col = hsv_to_rgb((uint8_t)((255/SPECTRUM_PIXELS)*i),255,255);
                    else if (cs == 2) col = color_wheel((i*256/SPECTRUM_PIXELS + s_cw_pos) & 0xFF);
                    else if (cs == 3) col = fire_color(i);
                    else col = color_wheel((i*256/SPECTRUM_PIXELS + s_cw_pos) & 0xFF);
                } else {
                    uint8_t bs = g_config.spectrum_background_settings;
                    if (bs == 0) col = CRGB(g_config.r[17],g_config.g[17],g_config.b[17]);
                    else if (bs == 1) col = color_wheel((i*256/SPECTRUM_PIXELS + s_cw_pos) & 0xFF);
                    else col = color_wheel2(s_cw_pos & 0xFF);
                }
                g_leds[real] = col;
            }
        }
        led_refresh();
        xSemaphoreGive(g_led_mutex);

        s_cw_pos = (s_cw_pos - s_cw_speed) & 0xFF;
        s_decay_cnt++;
        if (s_decay_cnt >= DECAY) {
            s_decay_cnt = 0;
            if (s_react > 0) s_react--;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
