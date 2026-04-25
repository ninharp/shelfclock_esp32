#include "sensors.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

static const char *TAG = "sensors";
i2c_master_bus_handle_t g_i2c_bus;

void adc_light_init_static(void);
void i2s_mic_init_static(void);

void sensors_init(void) {
    i2c_master_bus_config_t cfg = {
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .i2c_port          = I2C_NUM_0,
        .scl_io_num        = I2C_SCL_GPIO_NUM,
        .sda_io_num        = I2C_SDA_GPIO_NUM,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &g_i2c_bus));

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DHT11_GPIO_NUM),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);

    adc_light_init_static();
    i2s_mic_init_static();
    ESP_LOGI(TAG, "All sensors initialized");
}
