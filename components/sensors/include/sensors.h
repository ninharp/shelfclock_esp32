#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define DHT11_GPIO_NUM      10
#define I2C_SDA_GPIO_NUM    18
#define I2C_SCL_GPIO_NUM    19
#define ADC_LIGHT_CHANNEL   1
#define I2S_BCK_GPIO_NUM    6
#define I2S_WS_GPIO_NUM     7
#define I2S_DIN_GPIO_NUM    3
#define DS3231_I2C_ADDR     0x68

void     sensors_init(void);
bool     dht11_read(float *temp_c, float *humidity);
bool     ds3231_get_time(struct tm *t);
bool     ds3231_set_time(const struct tm *t);
bool     ds3231_lost_power(void);
uint16_t adc_light_read(void);
uint8_t  adc_light_to_brightness(uint16_t raw);
int32_t  i2s_mic_get_level(void);
void     i2s_mic_read_samples(int32_t *buf, size_t len);
