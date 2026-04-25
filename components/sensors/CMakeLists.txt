idf_component_register(
    SRCS "sensors.c" "dht11.c" "ds3231.c" "adc_light.c" "i2s_mic.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_adc esp_timer freertos
)
