#include "lightshow.h"
#include "storage.h"
#include "led_display.h"
#include "esp_timer.h"
#include "esp_random.h"

/* speed 1-5: 1=sehr langsam (×4), 3=normal (×1), 5=sehr schnell (÷4) */
static int speed_ms(int base) {
    uint8_t s = g_config.lightshow_speed;
    if (s < 1 || s > 5) s = 3;
    int ms = (s <= 3) ? (base << (3 - s)) : (base >> (s - 3));
    return ms < 10 ? 10 : ms;
}
static int speed_ms_spot(int base) {
    uint8_t s = g_config.spotlight_speed;
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

    if (!g_config.use_spotlights) {
        for (int i = SEGMENTS_LEDS; i < NUM_LEDS; i++) g_leds[i] = CRGB_BLACK;
        return;
    }

    int64_t now = esp_timer_get_time() / 1000;
    if (now - s_t < speed_ms_spot(30)) return;
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
    } else if (mode == 5) {
        /* Sequentielles Twinkle: ein zufälliges LED blendet einzeln ein/aus */
        static int     s5_led    = -1;
        static uint8_t s5_bright = 0;
        static int8_t  s5_dir    = 1;
        static uint8_t s5_hue    = 0;
        static uint8_t s5_hold   = 0;

        for (int i = 0; i < n; i++) g_leds[SEGMENTS_LEDS + i] = CRGB_BLACK;

        if (s5_led < 0) {
            s5_led    = (int)(esp_random() % (uint32_t)n);
            s5_bright = 0;
            s5_dir    = 1;
            s5_hue    = (uint8_t)(esp_random() & 0xFF);
            s5_hold   = 0;
        }

        if (s5_dir > 0) {
            s5_bright = (s5_bright > 235) ? 255 : s5_bright + 20;
            if (s5_bright == 255) { s5_dir = 0; s5_hold = 10; }
        } else if (s5_dir == 0) {
            if (s5_hold > 0) s5_hold--;
            else s5_dir = -1;
        } else {
            if (s5_bright < 20) { s5_bright = 0; s5_led = -1; }
            else s5_bright -= 20;
        }

        if (s5_led >= 0) {
            g_leds[SEGMENTS_LEDS + s5_led] = hsv_to_rgb(s5_hue, 255, s5_bright);
        }
    } else if (mode == 6) {
        /* Firefly: jedes LED hat unabhängige Phase und Farbe – asynchrones Aufblitzen */
        static uint8_t s6_phase[SPOT_LEDS];
        static uint8_t s6_hue[SPOT_LEDS];
        static bool    s6_inited = false;

        if (!s6_inited) {
            for (int i = 0; i < n; i++) {
                s6_phase[i] = (uint8_t)(esp_random() & 0xFF);
                s6_hue[i]   = (uint8_t)(esp_random() & 0xFF);
            }
            s6_inited = true;
        }

        for (int i = 0; i < n; i++) {
            uint8_t ph = s6_phase[i];
            uint8_t v;
            if      (ph < 64)  v = ph * 4;               /* einblenden  */
            else if (ph < 128) v = 255;                   /* halten      */
            else if (ph < 192) v = (uint8_t)((192 - ph) * 4); /* ausblenden */
            else               v = 0;                     /* Pause       */
            if (ph == 0) s6_hue[i] = (uint8_t)(esp_random() & 0xFF);
            g_leds[SEGMENTS_LEDS + i] = (v > 0)
                ? hsv_to_rgb(s6_hue[i], 220, v)
                : CRGB_BLACK;
            /* Individuelle Geschwindigkeit: nicht alle gleichzeitig */
            if ((esp_random() & 3) != 0) s6_phase[i]++;
        }
    } else if (mode == 7) {
        /* Multi-Twinkle: bis zu 4 LEDs gleichzeitig, jede unabhängig */
        #define MT_MAX 4
        static struct {
            int     led;
            uint8_t bright;
            int8_t  dir;    /* 1=ein, 0=halten, -1=aus, -2=inaktiv */
            uint8_t hold;
            uint8_t hue;
        } s7[MT_MAX];
        static bool s7_inited = false;
        if (!s7_inited) {
            for (int k = 0; k < MT_MAX; k++) s7[k].dir = -2;
            s7_inited = true;
        }

        for (int i = 0; i < n; i++) g_leds[SEGMENTS_LEDS + i] = CRGB_BLACK;

        for (int k = 0; k < MT_MAX; k++) {
            if (s7[k].dir == -2) {
                /* Zufällig entscheiden ob dieses Slot jetzt startet */
                if ((esp_random() & 7) == 0) {
                    s7[k].led    = (int)(esp_random() % (uint32_t)n);
                    s7[k].bright = 0;
                    s7[k].dir    = 1;
                    s7[k].hold   = (uint8_t)(5 + (esp_random() & 7));
                    s7[k].hue    = (uint8_t)(esp_random() & 0xFF);
                }
                continue;
            }
            if (s7[k].dir == 1) {
                s7[k].bright = (s7[k].bright > 235) ? 255 : s7[k].bright + 20;
                if (s7[k].bright == 255) { s7[k].dir = 0; }
            } else if (s7[k].dir == 0) {
                if (s7[k].hold > 0) s7[k].hold--;
                else s7[k].dir = -1;
            } else {
                if (s7[k].bright < 20) { s7[k].bright = 0; s7[k].dir = -2; }
                else s7[k].bright -= 20;
            }
            if (s7[k].dir != -2 && s7[k].led >= 0) {
                g_leds[SEGMENTS_LEDS + s7[k].led] = hsv_to_rgb(s7[k].hue, 255, s7[k].bright);
            }
        }
    } else if (mode == 8) {
        /* Comet: heller Kopf mit Schweif, prallt an Enden ab */
        static int     s8_pos   = 0;
        static int8_t  s8_dir   = 1;
        static uint8_t s8_hue   = 0;

        for (int i = 0; i < n; i++) {
            int dist = s8_pos - i;
            if (dist < 0) dist = -dist;
            /* Schweif nur hinter Bewegungsrichtung */
            int trail = (s8_dir > 0) ? (s8_pos - i) : (i - s8_pos);
            uint8_t v;
            if      (trail == 0) v = 255;
            else if (trail == 1) v = 160;
            else if (trail == 2) v = 80;
            else if (trail == 3) v = 30;
            else                 v = 0;
            g_leds[SEGMENTS_LEDS + i] = (v > 0)
                ? hsv_to_rgb((s8_hue + i * 8) & 0xFF, 240, v)
                : CRGB_BLACK;
        }
        s8_pos += s8_dir;
        if (s8_pos >= n - 1) { s8_pos = n - 1; s8_dir = -1; s8_hue += 30; }
        if (s8_pos <= 0)     { s8_pos = 0;      s8_dir =  1; s8_hue += 30; }
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
