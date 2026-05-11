#include "lightshow.h"
#include "storage.h"
#include "led_display.h"
#include "esp_timer.h"

/* lightshow_speed 1-5: 1=sehr langsam (×4), 3=normal (×1), 5=sehr schnell (÷4) */
static int speed_ms(int base) {
    uint8_t s = g_config.lightshow_speed;
    if (s < 1 || s > 5) s = 3;
    int ms = (s <= 3) ? (base << (3 - s)) : (base >> (s - 3));
    return ms < 10 ? 10 : ms;
}

#define EVERY_N_MS(ms, last_ms, body) do { \
    int64_t _now = esp_timer_get_time()/1000; \
    if (_now - (last_ms) >= (ms)) { (last_ms) = _now; body } \
} while(0)

/* Kopiert die unterste LED jeder Digit-Spalte auf die zugehörigen Spotlight-LEDs */
static void sync_spotlights(void) {
    if (!g_config.use_spotlights) {
        for (int i = SEGMENTS_LEDS; i < NUM_LEDS; i++) g_leds[i] = CRGB_BLACK;
        return;
    }
    for (int d = 0; d < NUMBER_OF_DIGITS; d++) {
        int col = (d * SPECTRUM_PIXELS) / NUMBER_OF_DIGITS;
        crgb_t c = g_leds[FAKE_LEDs_C_BMUP[col * LEDS_PER_SEGMENT]];
        g_leds[SEGMENTS_LEDS + d * 2]     = c;
        g_leds[SEGMENTS_LEDS + d * 2 + 1] = c;
    }
}

static int64_t s_twinkle_t = 0, s_matrix_t = 0;
static int64_t s_fire_t    = 0, s_cylon_t  = 0;

void spotlight_anim_update(void) {
    static int64_t s_t = 0;
    static uint8_t s_hue = 0;
    static uint8_t s_phase = 0;
    static int     s_pos = 0;
    uint8_t mode = g_config.spotlight_anim_mode;
    if (mode == 0) return;

    int64_t now = esp_timer_get_time() / 1000;
    if (now - s_t < 30) return;
    s_t = now;

    int n = SPOT_LEDS;
    if (mode == 1) {
        /* Rainbow: hue rotiert gleichmäßig über alle 14 LEDs */
        for (int i = 0; i < n; i++) {
            g_leds[SEGMENTS_LEDS + i] = hsv_to_rgb((s_hue + i * 255 / n) & 0xFF, 255, 200);
        }
        s_hue++;
    } else if (mode == 2) {
        /* Pulse: alle LEDs atmen in der statischen Farbe */
        /* Sinus-Näherung via quadratischem Dreieck (kein math.h nötig) */
        uint8_t ph = s_phase & 0xFF;
        int sv = ph < 128 ? (int)ph * 2 : (int)(255 - ph) * 2;
        uint8_t v = (uint8_t)(sv);
        crgb_t base = CRGB(g_config.r[0], g_config.g[0], g_config.b[0]);
        crgb_t col = CRGB(
            (uint8_t)((uint16_t)base.r * v / 255),
            (uint8_t)((uint16_t)base.g * v / 255),
            (uint8_t)((uint16_t)base.b * v / 255));
        for (int i = 0; i < n; i++) g_leds[SEGMENTS_LEDS + i] = col;
        s_phase++;
    } else if (mode == 3) {
        /* Chase: ein Lichtpunkt läuft durch alle 14 Spotlight-LEDs */
        for (int i = 0; i < n; i++) {
            int dist = (i - s_pos + n) % n;
            uint8_t v = dist == 0 ? 255 : dist == 1 ? 120 : dist == 2 ? 40 : 0;
            g_leds[SEGMENTS_LEDS + i] = hsv_to_rgb(s_hue, 255, v);
        }
        s_pos = (s_pos + 1) % n;
        s_hue += 3;
    } else if (mode == 4) {
        /* Color-Cycle: alle LEDs in einer Farbe, die sich langsam ändert */
        crgb_t col = hsv_to_rgb(s_hue, 255, 200);
        for (int i = 0; i < n; i++) g_leds[SEGMENTS_LEDS + i] = col;
        s_hue++;
    }
}

void lightshow_dispatch(void) {
    uint8_t mode = g_config.lightshow_mode;
    bool updated = false;
    if (mode == 0) { lightshow_chase();                                          updated = true; }
    else if (mode == 1) { EVERY_N_MS(speed_ms(30),  s_twinkle_t, { lightshow_twinkles(); updated = true; }); }
    else if (mode == 2) { lightshow_rainbow();                                   updated = true; }
    else if (mode == 3) { EVERY_N_MS(speed_ms(100), s_matrix_t,  { lightshow_green_matrix(); updated = true; }); }
    else if (mode == 4) { lightshow_rain();                                      updated = true; }
    else if (mode == 5) { EVERY_N_MS(speed_ms(60),  s_fire_t,    { lightshow_fire();  updated = true; }); }
    else if (mode == 6) {
        static int64_t s_snake2_t = 0;
        EVERY_N_MS(speed_ms(180), s_snake2_t, { lightshow_snake(); updated = true; });
    }
    else if (mode == 7) { EVERY_N_MS(speed_ms(150), s_cylon_t,   { lightshow_cylon(); updated = true; }); }

    if (updated) {
        sync_spotlights();
        led_refresh();
    }
}
