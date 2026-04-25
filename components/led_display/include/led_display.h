#pragma once
#include <stdint.h>
typedef struct { uint8_t r, g, b; } crgb_t;
#define NUM_LEDS 273
extern crgb_t g_leds[NUM_LEDS];
esp_err_t led_display_init(void);
void led_display_refresh(void);
