#pragma once
#include <stdint.h>
#include <time.h>
typedef struct {
    float temperature_c, temperature_f, humidity;
    uint16_t light_level;
    int32_t audio_level;
    struct tm rtc_time;
} sensor_data_t;
extern sensor_data_t g_sensors;
esp_err_t sensors_init(void);
