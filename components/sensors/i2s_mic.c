#include "sensors.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#define I2S_SAMPLE_RATE  16000
#define I2S_BUF_SAMPLES  64

static const char *TAG = "i2s_mic";
static i2s_chan_handle_t s_rx;

void i2s_mic_init_static(void) {
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &s_rx));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                        I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk  = I2S_GPIO_UNUSED,
            .bclk  = I2S_BCK_GPIO_NUM,
            .ws    = I2S_WS_GPIO_NUM,
            .dout  = I2S_GPIO_UNUSED,
            .din   = I2S_DIN_GPIO_NUM,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));
    ESP_LOGI(TAG, "INMP441 I2S initialized");
}

void i2s_mic_read_samples(int32_t *buf, size_t len) {
    size_t bytes_read = 0;
    i2s_channel_read(s_rx, buf, len * sizeof(int32_t), &bytes_read, pdMS_TO_TICKS(100));
    for (size_t i = 0; i < bytes_read / sizeof(int32_t); i++) buf[i] >>= 8;
}

int32_t i2s_mic_get_level(void) {
    int32_t buf[I2S_BUF_SAMPLES];
    i2s_mic_read_samples(buf, I2S_BUF_SAMPLES);
    int32_t peak = 0;
    for (int i = 0; i < I2S_BUF_SAMPLES; i++) {
        int32_t v = buf[i] < 0 ? -buf[i] : buf[i];
        if (v > peak) peak = v;
    }
    return peak;
}
