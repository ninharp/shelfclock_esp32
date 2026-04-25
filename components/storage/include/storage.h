#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    uint8_t clock_mode;
    uint8_t brightness;
    uint8_t r[18], g[18], b[18];
    char scroll_text[257];
    int32_t gmt_offset_sec;
    /* weitere Felder folgen in Task 2 */
} clock_config_t;

extern clock_config_t g_config;
extern SemaphoreHandle_t g_config_mutex;
extern SemaphoreHandle_t g_led_mutex;

esp_err_t storage_init(void);
esp_err_t storage_load(void);
esp_err_t storage_save_all(void);
