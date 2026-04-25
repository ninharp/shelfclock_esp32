#include "sensors.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static bool dht11_wait_level(int gpio, int level, int timeout_us) {
    int64_t start = esp_timer_get_time();
    while (gpio_get_level(gpio) != level) {
        if (esp_timer_get_time() - start > timeout_us) return false;
    }
    return true;
}

bool dht11_read(float *temp_c, float *humidity) {
    uint8_t data[5] = {0};

    gpio_set_direction(DHT11_GPIO_NUM, GPIO_MODE_OUTPUT);
    gpio_set_level(DHT11_GPIO_NUM, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(DHT11_GPIO_NUM, 1);
    esp_rom_delay_us(30);
    gpio_set_direction(DHT11_GPIO_NUM, GPIO_MODE_INPUT);

    if (!dht11_wait_level(DHT11_GPIO_NUM, 0, 100)) return false;
    if (!dht11_wait_level(DHT11_GPIO_NUM, 1, 100)) return false;
    if (!dht11_wait_level(DHT11_GPIO_NUM, 0, 100)) return false;

    for (int i = 0; i < 40; i++) {
        if (!dht11_wait_level(DHT11_GPIO_NUM, 1, 100)) return false;
        int64_t t = esp_timer_get_time();
        if (!dht11_wait_level(DHT11_GPIO_NUM, 0, 100)) return false;
        if (esp_timer_get_time() - t > 40) data[i / 8] |= (1 << (7 - i % 8));
    }

    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) return false;

    *humidity = data[0] + data[1] * 0.1f;
    *temp_c   = data[2] + data[3] * 0.1f;
    return true;
}
