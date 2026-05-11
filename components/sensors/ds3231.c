#include "sensors.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <string.h>

extern i2c_master_bus_handle_t g_i2c_bus;
static i2c_master_dev_handle_t s_ds3231;
static bool s_initialized = false;
static bool s_present = true;

static uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
static uint8_t dec2bcd(uint8_t d) { return ((d / 10) << 4) | (d % 10); }

static void ds3231_ensure_init(void) {
    if (s_initialized) return;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = DS3231_I2C_ADDR,
        .scl_speed_hz    = 400000,
    };
    i2c_master_bus_add_device(g_i2c_bus, &cfg, &s_ds3231);
    s_initialized = true;
}

bool ds3231_get_time(struct tm *t) {
    ds3231_ensure_init();
    if (!s_present) return false;
    uint8_t reg = 0x00, buf[7];
    if (i2c_master_transmit(s_ds3231, &reg, 1, 100) != ESP_OK) { s_present = false; return false; }
    if (i2c_master_receive(s_ds3231, buf, 7, 100) != ESP_OK) { s_present = false; return false; }
    memset(t, 0, sizeof(*t));
    t->tm_sec  = bcd2dec(buf[0] & 0x7F);
    t->tm_min  = bcd2dec(buf[1] & 0x7F);
    t->tm_hour = bcd2dec(buf[2] & 0x3F);
    t->tm_wday = buf[3] - 1;
    t->tm_mday = bcd2dec(buf[4]);
    t->tm_mon  = bcd2dec(buf[5] & 0x1F) - 1;
    t->tm_year = bcd2dec(buf[6]) + 100;
    return true;
}

bool ds3231_set_time(const struct tm *t) {
    ds3231_ensure_init();
    if (!s_present) return false;
    uint8_t buf[8] = {
        0x00,
        dec2bcd(t->tm_sec),
        dec2bcd(t->tm_min),
        dec2bcd(t->tm_hour),
        (uint8_t)(t->tm_wday + 1),
        dec2bcd(t->tm_mday),
        dec2bcd(t->tm_mon + 1),
        dec2bcd(t->tm_year - 100),
    };
    return i2c_master_transmit(s_ds3231, buf, 8, 100) == ESP_OK;
}

bool ds3231_lost_power(void) {
    ds3231_ensure_init();
    if (!s_present) return true;
    uint8_t reg = 0x0F, status;
    if (i2c_master_transmit(s_ds3231, &reg, 1, 100) != ESP_OK) { s_present = false; return true; }
    if (i2c_master_receive(s_ds3231, &status, 1, 100) != ESP_OK) { s_present = false; return true; }
    return (status & 0x80) != 0;
}
