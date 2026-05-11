#include "lightshow.h"
#include "storage.h"
#include "led_display.h"
#include "esp_random.h"
#include <string.h>
#include <stdlib.h>

// ── Chase ─────────────────────────────────────────────────────────────────────
void lightshow_chase(void) {
    static int pos = 0;
    static int cw_pos = 0;
    for (int i = 0; i < FAKE_NUM_LEDS; i++) g_leds[FAKE_LEDs[i]] = CRGB_BLACK;
    for (int j = 0; j < 5; j++) {
        int idx = (pos + j * 7) % FAKE_NUM_LEDS;
        g_leds[FAKE_LEDs[idx]] = color_wheel((cw_pos + j * 20) & 0xFF);
    }
    pos = (pos + 1) % FAKE_NUM_LEDS;
    cw_pos = (cw_pos + 1) & 0xFF;
}

// ── Twinkles ──────────────────────────────────────────────────────────────────
void lightshow_twinkles(void) {
    for (int i = 0; i < FAKE_NUM_LEDS; i++) fade_to_black_by(&g_leds[FAKE_LEDs[i]], 30);
    for (int i = 0; i < 10; i++) {
        int idx = esp_random() % FAKE_NUM_LEDS;
        g_leds[FAKE_LEDs[idx]] = random_color(g_config.pastel_colors);
    }
}

// ── Rainbow ───────────────────────────────────────────────────────────────────
void lightshow_rainbow(void) {
    static int hue = 0;
    for (int i = 0; i < FAKE_NUM_LEDS; i++) {
        g_leds[FAKE_LEDs[i]] = hsv_to_rgb((hue + i * 256/FAKE_NUM_LEDS) & 0xFF, 255, 200);
    }
    hue = (hue + 2) & 0xFF;
}

// ── Rain ──────────────────────────────────────────────────────────────────────
#define RAIN_COLS SPECTRUM_PIXELS
static uint8_t s_rain_heat[RAIN_COLS];
static bool    s_rain_inited = false;
void lightshow_rain(void) {
    if (!s_rain_inited) {
        memset(s_rain_heat, 0, sizeof(s_rain_heat));
        s_rain_inited = true;
    }
    for (int c = 0; c < RAIN_COLS; c++) {
        if (esp_random() % 10 == 0) s_rain_heat[c] = 200 + (esp_random() & 55);
        if (s_rain_heat[c] > 20) s_rain_heat[c] -= 20;
        else s_rain_heat[c] = 0;
        crgb_t col = s_rain_heat[c] > 0 ? CRGB(0, 0, s_rain_heat[c]) : CRGB_BLACK;
        int base = c * LEDS_PER_SEGMENT;
        for (int j = 0; j < LEDS_PER_SEGMENT; j++) {
            int real = FAKE_LEDs_C_RAIN[base + j];
            if (real < NUM_LEDS) g_leds[real] = col;
        }
    }
}

// ── Fire ──────────────────────────────────────────────────────────────────────
#define FIRE_COLS SPECTRUM_PIXELS
static uint8_t s_fire_heat[FIRE_COLS];
void lightshow_fire(void) {
    for (int c = 0; c < FIRE_COLS; c++) {
        if (esp_random() % 5 == 0) s_fire_heat[c] = 180 + (esp_random() & 75);
        if (s_fire_heat[c] > 15) s_fire_heat[c] -= 15;
        else s_fire_heat[c] = 0;
        uint8_t h = s_fire_heat[c];
        crgb_t col;
        if      (h > 200) col = CRGB(255, 255, h-200);
        else if (h > 100) col = CRGB(255, h-100, 0);
        else if (h >   0) col = CRGB(h*2, 0, 0);
        else              col = CRGB_BLACK;
        int base = c * LEDS_PER_SEGMENT;
        for (int j = 0; j < LEDS_PER_SEGMENT; j++) {
            int real = FAKE_LEDs_C_FIRE[base + j];
            if (real < NUM_LEDS) g_leds[real] = col;
        }
    }
}

// ── Snake ─────────────────────────────────────────────────────────────────────
void lightshow_snake(void) {
    static int pos = 0;
    static int dir = 1;
    static int cw  = 0;
    for (int i = 0; i < FAKE_NUM_LEDS; i++) fade_to_black_by(&g_leds[FAKE_LEDs[i]], 50);
    for (int j = 0; j < 14; j++) {
        int idx = (pos + j) % FAKE_NUM_LEDS;
        g_leds[FAKE_LEDs[idx]] = color_wheel((cw + j*8) & 0xFF);
    }
    pos += dir;
    if (pos >= FAKE_NUM_LEDS || pos < 0) { dir = -dir; pos += dir; }
    cw = (cw + 3) & 0xFF;
}

// ── Cylon ─────────────────────────────────────────────────────────────────────
void lightshow_cylon(void) {
    static int pos = 0;
    static int dir = 1;
    static int cw  = 0;
    #define CYLON_COLS (FAKE_NUM_LEDS / LEDS_PER_SEGMENT)
    for (int i = 0; i < FAKE_NUM_LEDS; i++) fade_to_black_by(&g_leds[FAKE_LEDs[i]], 40);
    int base = pos * LEDS_PER_SEGMENT;
    for (int j = 0; j < LEDS_PER_SEGMENT; j++) {
        if (base+j < FAKE_NUM_LEDS) g_leds[FAKE_LEDs[base+j]] = color_wheel(cw & 0xFF);
    }
    pos += dir;
    if (pos >= CYLON_COLS || pos < 0) { dir = -dir; pos += dir; }
    cw = (cw + 4) & 0xFF;
}

// ── GreenMatrix ───────────────────────────────────────────────────────────────
#define GM_COLS SPECTRUM_PIXELS
static uint8_t s_gm_pos[GM_COLS];
static bool    s_gm_inited = false;
void lightshow_green_matrix(void) {
    if (!s_gm_inited) {
        for (int i = 0; i < GM_COLS; i++) s_gm_pos[i] = esp_random() % LEDS_PER_SEGMENT;
        s_gm_inited = true;
    }
    for (int c = 0; c < GM_COLS; c++) {
        int base = c * LEDS_PER_SEGMENT;
        for (int j = 0; j < LEDS_PER_SEGMENT; j++) {
            int real = FAKE_LEDs_C_BMUP[base + j];
            if (real < NUM_LEDS) {
                if (j == s_gm_pos[c]) g_leds[real] = CRGB(0, 255, 0);
                else fade_to_black_by(&g_leds[real], 60);
            }
        }
        if (esp_random() % 3 == 0) s_gm_pos[c] = (s_gm_pos[c] + 1) % LEDS_PER_SEGMENT;
    }
}
