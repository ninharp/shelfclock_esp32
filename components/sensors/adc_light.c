#include "sensors.h"
#include "esp_adc/adc_oneshot.h"

#define PHOTO_SAMPLES 15

static adc_oneshot_unit_handle_t s_adc;
static int s_readings[PHOTO_SAMPLES];
static int s_read_idx = 0;

void adc_light_init_static(void) {
    adc_oneshot_unit_init_cfg_t init_cfg = { .unit_id = ADC_UNIT_1 };
    adc_oneshot_new_unit(&init_cfg, &s_adc);
    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten    = ADC_ATTEN_DB_12,
    };
    adc_oneshot_config_channel(s_adc, ADC_LIGHT_CHANNEL, &chan_cfg);
}

uint16_t adc_light_read(void) {
    int raw = 0;
    adc_oneshot_read(s_adc, ADC_LIGHT_CHANNEL, &raw);
    s_readings[s_read_idx] = raw;
    s_read_idx = (s_read_idx + 1) % PHOTO_SAMPLES;
    int sum = 0;
    for (int i = 0; i < PHOTO_SAMPLES; i++) sum += s_readings[i];
    return (uint16_t)(sum / PHOTO_SAMPLES);
}

uint8_t adc_light_to_brightness(uint16_t raw) {
    int mapped = (raw * 254 / 4095) + 1;
    if (mapped < 1) mapped = 1;
    if (mapped > 255) mapped = 255;
    return (uint8_t)mapped;
}
