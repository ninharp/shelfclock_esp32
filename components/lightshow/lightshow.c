#include "lightshow.h"
#include "storage.h"
#include "led_display.h"
#include "esp_timer.h"

#define EVERY_N_MS(ms, last_ms, body) do { \
    int64_t _now = esp_timer_get_time()/1000; \
    if (_now - (last_ms) >= (ms)) { (last_ms) = _now; body } \
} while(0)

static int64_t s_twinkle_t = 0, s_matrix_t = 0;
static int64_t s_fire_t    = 0, s_cylon_t  = 0;

void lightshow_dispatch(void) {
    uint8_t mode = g_config.lightshow_mode;
    if (mode == 0) { lightshow_chase();  led_refresh(); }
    else if (mode == 1) { EVERY_N_MS(30, s_twinkle_t, { lightshow_twinkles(); led_refresh(); }); }
    else if (mode == 2) { lightshow_rainbow(); led_refresh(); }
    else if (mode == 3) { EVERY_N_MS(100, s_matrix_t, { lightshow_green_matrix(); }); }
    else if (mode == 4) { lightshow_rain(); led_refresh(); }
    else if (mode == 5) { EVERY_N_MS(60, s_fire_t, { lightshow_fire(); led_refresh(); }); }
    else if (mode == 6) {
        static int64_t s_snake2_t = 0;
        EVERY_N_MS(180, s_snake2_t, { lightshow_snake(); led_refresh(); });
    }
    else if (mode == 7) { EVERY_N_MS(150, s_cylon_t, { lightshow_cylon(); led_refresh(); }); }
}
