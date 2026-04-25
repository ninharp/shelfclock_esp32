#include "storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <time.h>

static const char *NVS_NS = "shelfclock";

clock_config_t g_config;
sensor_data_t  g_sensors;
SemaphoreHandle_t g_config_mutex;

void storage_defaults(void) {
    memset(&g_config, 0, sizeof(g_config));
    g_config.clock_mode              = 0;
    g_config.clock_display_type      = 3;
    g_config.date_display_type       = 5;
    g_config.temp_display_type       = 0;
    g_config.humi_display_type       = 0;
    g_config.brightness              = 10;
    g_config.gmt_offset_sec          = -28800;
    g_config.ds_time                 = false;
    g_config.temperature_symbol      = 39;
    g_config.temperature_correction  = 0;
    g_config.colon_type              = 0;
    g_config.color_change_frequency  = 0;
    g_config.pastel_colors           = 0;
    g_config.color_change_cd         = true;
    g_config.use_audible_alarm       = false;
    g_config.use_spotlights          = true;
    g_config.spectrum_mode           = 0;
    g_config.spectrum_color_settings = 2;
    g_config.spectrum_background_settings = 0;
    g_config.realtime_mode           = 0;
    g_config.spotlights_color_settings = 0;
    g_config.scroll_color_settings   = 0;
    g_config.scroll_frequency        = 1;
    g_config.random_spectrum_mode    = false;
    g_config.scroll_override         = true;
    g_config.lightshow_mode          = 0;
    g_config.suspend_frequency       = 1;
    g_config.suspend_type            = 0;
    g_config.scoreboard_left         = 0;
    g_config.scoreboard_right        = 0;
    strncpy(g_config.scroll_text, "dAdS ArE tHE bESt", sizeof(g_config.scroll_text) - 1);
    for (int i = 0; i < NUM_COLOR_CHANNELS; i++) {
        g_config.r[i] = 255; g_config.g[i] = 0; g_config.b[i] = 0;
    }
    g_config.r[0] = 193; g_config.g[0] = 204; g_config.b[0] = 78;
    g_config.r[16] = 255; g_config.g[16] = 255; g_config.b[16] = 255;
    g_config.r[17] = 0; g_config.g[17] = 0; g_config.b[17] = 0;
    g_config.cd_r = 0; g_config.cd_g = 255; g_config.cd_b = 0;
}

static void load_from_nvs(nvs_handle_t h) {
    int32_t v32; uint8_t u8; int8_t i8; size_t len;

    if (nvs_get_i32(h, "gmtOffset_sec",  &v32) == ESP_OK) g_config.gmt_offset_sec = v32;
    if (nvs_get_u8(h,  "DSTime",         &u8)  == ESP_OK) g_config.ds_time = (bool)u8;
    if (nvs_get_u8(h,  "clockMode",      &u8)  == ESP_OK) g_config.clock_mode = u8;
    if (nvs_get_u8(h,  "clockDispType",  &u8)  == ESP_OK) g_config.clock_display_type = u8;
    if (nvs_get_u8(h,  "dateDisplayType",&u8)  == ESP_OK) g_config.date_display_type = u8;
    if (nvs_get_u8(h,  "tempDisplayType",&u8)  == ESP_OK) g_config.temp_display_type = u8;
    if (nvs_get_u8(h,  "humiDisplayType",&u8)  == ESP_OK) g_config.humi_display_type = u8;
    if (nvs_get_u8(h,  "brightness",     &u8)  == ESP_OK) g_config.brightness = u8;
    if (nvs_get_u8(h,  "ClockColorSet",  &u8)  == ESP_OK) g_config.clock_color_settings = u8;
    if (nvs_get_u8(h,  "DateColorSet",   &u8)  == ESP_OK) g_config.date_color_settings = u8;
    if (nvs_get_u8(h,  "tempColorSet",   &u8)  == ESP_OK) g_config.temp_color_settings = u8;
    if (nvs_get_u8(h,  "humiColorSet",   &u8)  == ESP_OK) g_config.humi_color_settings = u8;
    if (nvs_get_u8(h,  "temperatureSym", &u8)  == ESP_OK) g_config.temperature_symbol = u8;
    if (nvs_get_u8(h,  "pastelColors",   &u8)  == ESP_OK) g_config.pastel_colors = u8;
    if (nvs_get_u8(h,  "colonType",      &u8)  == ESP_OK) g_config.colon_type = u8;
    if (nvs_get_u8(h,  "ColorChangeFreq",&u8)  == ESP_OK) g_config.color_change_frequency = u8;
    if (nvs_get_u8(h,  "spectrumMode",   &u8)  == ESP_OK) g_config.spectrum_mode = u8;
    if (nvs_get_u8(h,  "realtimeMode",   &u8)  == ESP_OK) g_config.realtime_mode = u8;
    if (nvs_get_u8(h,  "spectrumColor",  &u8)  == ESP_OK) g_config.spectrum_color_settings = u8;
    if (nvs_get_u8(h,  "spectrumBkgd",  &u8)  == ESP_OK) g_config.spectrum_background_settings = u8;
    if (nvs_get_u8(h,  "spotlightsCoSe",&u8)  == ESP_OK) g_config.spotlights_color_settings = u8;
    if (nvs_get_u8(h,  "scrollColorSet", &u8)  == ESP_OK) g_config.scroll_color_settings = u8;
    if (nvs_get_u8(h,  "scrollFreq",     &u8)  == ESP_OK) g_config.scroll_frequency = u8;
    if (nvs_get_u8(h,  "lightshowMode",  &u8)  == ESP_OK) g_config.lightshow_mode = u8;
    if (nvs_get_u8(h,  "suspendFreq",    &u8)  == ESP_OK) g_config.suspend_frequency = u8;
    if (nvs_get_u8(h,  "suspendType",    &u8)  == ESP_OK) g_config.suspend_type = u8;
    if (nvs_get_u8(h,  "alarmCD",        &u8)  == ESP_OK) g_config.use_audible_alarm = (bool)u8;
    if (nvs_get_u8(h,  "colorchangeCD",  &u8)  == ESP_OK) g_config.color_change_cd = (bool)u8;
    if (nvs_get_u8(h,  "useSpotlights",  &u8)  == ESP_OK) g_config.use_spotlights = (bool)u8;
    if (nvs_get_u8(h,  "randSpecMode",   &u8)  == ESP_OK) g_config.random_spectrum_mode = (bool)u8;
    if (nvs_get_u8(h,  "scrollOverride", &u8)  == ESP_OK) g_config.scroll_override = (bool)u8;
    if (nvs_get_i8(h,  "tempCorrection", &i8)  == ESP_OK) g_config.temperature_correction = i8;

    for (int i = 0; i < 8; i++) {
        char key[16]; snprintf(key, sizeof(key), "scrollOpts%d", i + 1);
        if (nvs_get_u8(h, key, &u8) == ESP_OK) g_config.scroll_options[i] = (bool)u8;
    }
    for (int i = 0; i < NUM_COLOR_CHANNELS; i++) {
        char kr[8], kg[8], kb[8];
        snprintf(kr, sizeof(kr), "r%d_val", i);
        snprintf(kg, sizeof(kg), "g%d_val", i);
        snprintf(kb, sizeof(kb), "b%d_val", i);
        if (nvs_get_u8(h, kr, &u8) == ESP_OK) g_config.r[i] = u8;
        if (nvs_get_u8(h, kg, &u8) == ESP_OK) g_config.g[i] = u8;
        if (nvs_get_u8(h, kb, &u8) == ESP_OK) g_config.b[i] = u8;
    }
    if (nvs_get_u8(h, "cd_r_val", &u8) == ESP_OK) g_config.cd_r = u8;
    if (nvs_get_u8(h, "cd_g_val", &u8) == ESP_OK) g_config.cd_g = u8;
    if (nvs_get_u8(h, "cd_b_val", &u8) == ESP_OK) g_config.cd_b = u8;

    len = sizeof(g_config.scroll_text);
    nvs_get_str(h, "scrollText", g_config.scroll_text, &len);
}

void storage_init(void) {
    g_config_mutex = xSemaphoreCreateMutex();
    storage_defaults();
}

void storage_load(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        load_from_nvs(h);
        nvs_close(h);
    }
}

void storage_save_all(void) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;

    nvs_set_i32(h, "gmtOffset_sec",  g_config.gmt_offset_sec);
    nvs_set_u8(h,  "DSTime",         (uint8_t)g_config.ds_time);
    nvs_set_u8(h,  "clockMode",      g_config.clock_mode);
    nvs_set_u8(h,  "clockDispType",  g_config.clock_display_type);
    nvs_set_u8(h,  "dateDisplayType",g_config.date_display_type);
    nvs_set_u8(h,  "tempDisplayType",g_config.temp_display_type);
    nvs_set_u8(h,  "humiDisplayType",g_config.humi_display_type);
    nvs_set_u8(h,  "brightness",     g_config.brightness);
    nvs_set_u8(h,  "ClockColorSet",  g_config.clock_color_settings);
    nvs_set_u8(h,  "DateColorSet",   g_config.date_color_settings);
    nvs_set_u8(h,  "tempColorSet",   g_config.temp_color_settings);
    nvs_set_u8(h,  "humiColorSet",   g_config.humi_color_settings);
    nvs_set_u8(h,  "temperatureSym", g_config.temperature_symbol);
    nvs_set_u8(h,  "pastelColors",   g_config.pastel_colors);
    nvs_set_u8(h,  "colonType",      g_config.colon_type);
    nvs_set_u8(h,  "ColorChangeFreq",g_config.color_change_frequency);
    nvs_set_u8(h,  "spectrumMode",   g_config.spectrum_mode);
    nvs_set_u8(h,  "realtimeMode",   g_config.realtime_mode);
    nvs_set_u8(h,  "spectrumColor",  g_config.spectrum_color_settings);
    nvs_set_u8(h,  "spectrumBkgd",  g_config.spectrum_background_settings);
    nvs_set_u8(h,  "spotlightsCoSe",g_config.spotlights_color_settings);
    nvs_set_u8(h,  "scrollColorSet", g_config.scroll_color_settings);
    nvs_set_u8(h,  "scrollFreq",     g_config.scroll_frequency);
    nvs_set_u8(h,  "lightshowMode",  g_config.lightshow_mode);
    nvs_set_u8(h,  "suspendFreq",    g_config.suspend_frequency);
    nvs_set_u8(h,  "suspendType",    g_config.suspend_type);
    nvs_set_u8(h,  "alarmCD",        (uint8_t)g_config.use_audible_alarm);
    nvs_set_u8(h,  "colorchangeCD",  (uint8_t)g_config.color_change_cd);
    nvs_set_u8(h,  "useSpotlights",  (uint8_t)g_config.use_spotlights);
    nvs_set_u8(h,  "randSpecMode",   (uint8_t)g_config.random_spectrum_mode);
    nvs_set_u8(h,  "scrollOverride", (uint8_t)g_config.scroll_override);
    nvs_set_i8(h,  "tempCorrection", g_config.temperature_correction);

    for (int i = 0; i < 8; i++) {
        char key[16]; snprintf(key, sizeof(key), "scrollOpts%d", i + 1);
        nvs_set_u8(h, key, (uint8_t)g_config.scroll_options[i]);
    }
    for (int i = 0; i < NUM_COLOR_CHANNELS; i++) {
        char kr[8], kg[8], kb[8];
        snprintf(kr, sizeof(kr), "r%d_val", i);
        snprintf(kg, sizeof(kg), "g%d_val", i);
        snprintf(kb, sizeof(kb), "b%d_val", i);
        nvs_set_u8(h, kr, g_config.r[i]);
        nvs_set_u8(h, kg, g_config.g[i]);
        nvs_set_u8(h, kb, g_config.b[i]);
    }
    nvs_set_u8(h, "cd_r_val", g_config.cd_r);
    nvs_set_u8(h, "cd_g_val", g_config.cd_g);
    nvs_set_u8(h, "cd_b_val", g_config.cd_b);
    nvs_set_str(h, "scrollText", g_config.scroll_text);
    nvs_commit(h);
    nvs_close(h);
}

void storage_load_preset(int n) {
    char ns[16]; snprintf(ns, sizeof(ns), "shelfclock-p%d", n);
    nvs_handle_t h;
    if (nvs_open(ns, NVS_READONLY, &h) == ESP_OK) {
        load_from_nvs(h);
        nvs_close(h);
    }
}

void storage_save_preset(int n) {
    char ns[16]; snprintf(ns, sizeof(ns), "shelfclock-p%d", n);
    nvs_handle_t h;
    if (nvs_open(ns, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "clockMode",      g_config.clock_mode);
    nvs_set_u8(h, "brightness",     g_config.brightness);
    nvs_set_i32(h, "gmtOffset_sec", g_config.gmt_offset_sec);
    nvs_set_u8(h, "DSTime",         (uint8_t)g_config.ds_time);
    for (int i = 0; i < NUM_COLOR_CHANNELS; i++) {
        char kr[8], kg[8], kb[8];
        snprintf(kr, sizeof(kr), "r%d_val", i);
        snprintf(kg, sizeof(kg), "g%d_val", i);
        snprintf(kb, sizeof(kb), "b%d_val", i);
        nvs_set_u8(h, kr, g_config.r[i]);
        nvs_set_u8(h, kg, g_config.g[i]);
        nvs_set_u8(h, kb, g_config.b[i]);
    }
    nvs_set_str(h, "scrollText", g_config.scroll_text);
    nvs_commit(h);
    nvs_close(h);
}
