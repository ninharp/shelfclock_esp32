#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define NUM_COLOR_CHANNELS 18

typedef struct {
    uint8_t  clock_mode;
    uint8_t  clock_display_type;
    uint8_t  date_display_type;
    uint8_t  temp_display_type;
    uint8_t  humi_display_type;
    uint8_t  brightness;           // 10 = Auto
    int32_t  gmt_offset_sec;
    bool     ds_time;
    uint8_t  clock_color_settings;
    uint8_t  date_color_settings;
    uint8_t  temp_color_settings;
    uint8_t  humi_color_settings;
    uint8_t  r[NUM_COLOR_CHANNELS];
    uint8_t  g[NUM_COLOR_CHANNELS];
    uint8_t  b[NUM_COLOR_CHANNELS];
    uint8_t  cd_r, cd_g, cd_b;    // countdown color
    uint8_t  temperature_symbol;   // 36=C, 39=F
    int8_t   temperature_correction;
    uint8_t  colon_type;
    uint8_t  color_change_frequency;
    uint8_t  pastel_colors;
    bool     color_change_cd;
    bool     use_audible_alarm;
    bool     use_spotlights;
    uint8_t  spectrum_mode;
    uint8_t  spectrum_color_settings;
    uint8_t  spectrum_background_settings;
    uint8_t  realtime_mode;
    uint8_t  spotlights_color_settings;
    uint8_t  scroll_color_settings;
    uint8_t  scroll_frequency;
    bool     random_spectrum_mode;
    bool     scroll_override;
    bool     scroll_options[8];
    uint8_t  lightshow_mode;
    uint8_t  lightshow_speed;       // 1=sehr langsam … 3=normal … 5=sehr schnell
    uint8_t  spotlight_brightness;  // 1-10, 10=Auto
    uint8_t  spotlight_anim_mode;   // 0=static, 1=rainbow, 2=pulse, 3=chase, 4=color-cycle
    uint8_t  suspend_frequency;
    uint8_t  suspend_type;
    char     scroll_text[257];
    int32_t  scoreboard_left;
    int32_t  scoreboard_right;
} clock_config_t;

typedef struct {
    float    temperature_c;
    float    temperature_f;
    float    humidity;
    uint16_t light_level;
    int32_t  audio_level;
    struct tm rtc_time;
    bool     rtc_valid;
} sensor_data_t;

extern clock_config_t g_config;
extern sensor_data_t  g_sensors;
extern SemaphoreHandle_t g_config_mutex;

void     storage_init(void);
void     storage_load(void);
void     storage_save_all(void);
void     storage_load_preset(int n);
void     storage_save_preset(int n);
void     storage_defaults(void);
