#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define LEDS_PER_SEGMENT   7
#define SEGMENTS_PER_NUMBER 7
#define NUMBER_OF_DIGITS   7
#define SPECTRUM_PIXELS    37
#define SEGMENTS_LEDS      (SPECTRUM_PIXELS * LEDS_PER_SEGMENT)
#define SPOT_LEDS          (NUMBER_OF_DIGITS * 2)
#define NUM_LEDS           (SEGMENTS_LEDS + SPOT_LEDS)
#define FAKE_NUM_LEDS      (NUMBER_OF_DIGITS * SEGMENTS_PER_NUMBER * LEDS_PER_SEGMENT)

#define LED_GPIO_NUM       4

typedef struct { uint8_t r, g, b; } crgb_t;

#define CRGB_BLACK   ((crgb_t){0,   0,   0  })
#define CRGB_RED     ((crgb_t){255, 0,   0  })
#define CRGB_GREEN   ((crgb_t){0,   255, 0  })
#define CRGB_BLUE    ((crgb_t){0,   0,   255})
#define CRGB_WHITE   ((crgb_t){255, 255, 255})
#define CRGB(r,g,b)  ((crgb_t){(r),(g),(b)})

extern crgb_t g_leds[NUM_LEDS];
extern SemaphoreHandle_t g_led_mutex;

extern const uint16_t FAKE_LEDs[FAKE_NUM_LEDS];
extern const uint16_t FAKE_LEDs_C_BMUP[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_CMOT[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_BLTR[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_TLBR[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_TMDN[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_CSIN[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_BRTL[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_TRBL[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_OUTS[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_OUTS2[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_VERT[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_VERT2[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_FIRE[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_RAIN[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_SNAKE[SEGMENTS_LEDS];
extern const uint8_t  g_numbers[97];

/* ── Digit-Übergangsanimation ────────────────────────────────────────── */
typedef struct {
    uint8_t from;   /* vorheriger Wert (10=leer, 255=nicht initialisiert) */
    uint8_t to;     /* Zielwert                                           */
    int16_t frame;  /* -1=keine Animation, 0..g_digit_anim_frames-1=aktiv */
} digit_anim_t;

#define DIGIT_ANIM_INIT { .from = 10, .to = 255, .frame = -1 }

extern bool g_digit_anim_active;  /* true während mindestens ein Übergang läuft */
extern int  g_digit_anim_frames;  /* Anzahl Frames (Geschwindigkeit), default 10 */

/* Gibt true zurück wenn Animation noch läuft */
bool display_number_animated(uint8_t num, int digit_pos, crgb_t color,
                              digit_anim_t *anim);

void led_display_init(void);
void led_refresh(void);
void all_blank(void);
void display_number(uint8_t num, int digit_pos, crgb_t color);
void shelf_down_lights(void);
void blink_dots(bool *dots_on);
void set_brightness(uint8_t brightness);

crgb_t color_wheel(int pos);
crgb_t color_wheel2(int pos);
crgb_t hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v);
crgb_t random_color(bool pastel);
void   fade_to_black_by(crgb_t *c, uint8_t amount);
