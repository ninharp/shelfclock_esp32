# ShelfClock ESP-IDF 5.4.3 Port — Implementierungsplan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Ziel:** Vollständige Portierung des ShelfClock Arduino-Projekts auf ESP-IDF 5.4.3 für den ESP32-C3 mit allen Original-Funktionen.

**Architektur:** 8 ESP-IDF-Komponenten (`storage`, `led_display`, `sensors`, `clock_modes`, `lightshow`, `rtttl_player`, `web_server`, `wifi_manager`). Globaler Zustand in `clock_config_t` (NVS-persistent), Mutex-gesichert. FreeRTOS-Tasks für HTTP-Server, Spectrum-Analyzer und RTTTL-Player.

**Tech Stack:** ESP-IDF 5.4.3, ESP32-C3, led_strip (RMT), i2s_std (INMP441), i2c_master (DS3231), adc_oneshot (Fotowiderstand), ledc (Buzzer), esp_http_server, esp_spiffs, mdns, nvs_flash, esp_sntp, esp_ota_ops

---

## Vollständige Dateistruktur

```
ShelfClock/
├── old/ShelfClock.ino                    (verschoben)
├── CMakeLists.txt
├── sdkconfig.defaults
├── partitions.csv
├── data/                                  (unverändert, für SPIFFS)
├── main/
│   ├── CMakeLists.txt
│   └── main.c
└── components/
    ├── storage/
    │   ├── CMakeLists.txt
    │   ├── include/storage.h
    │   └── storage.c
    ├── led_display/
    │   ├── CMakeLists.txt
    │   ├── include/led_display.h
    │   └── led_display.c
    ├── sensors/
    │   ├── CMakeLists.txt
    │   ├── include/sensors.h
    │   ├── sensors.c
    │   ├── dht11.c
    │   ├── ds3231.c
    │   ├── adc_light.c
    │   └── i2s_mic.c
    ├── clock_modes/
    │   ├── CMakeLists.txt
    │   ├── include/clock_modes.h
    │   ├── mode_time.c
    │   ├── mode_date.c
    │   ├── mode_temperature.c
    │   ├── mode_humidity.c
    │   ├── mode_scoreboard.c
    │   ├── mode_countdown.c
    │   ├── mode_stopwatch.c
    │   └── mode_scroll.c
    ├── lightshow/
    │   ├── CMakeLists.txt
    │   ├── include/lightshow.h
    │   ├── lightshow.c
    │   ├── lightshow_fx.c
    │   └── spectrum.c
    ├── rtttl_player/
    │   ├── CMakeLists.txt
    │   ├── include/rtttl_player.h
    │   ├── rtttl_songs.h
    │   └── rtttl_player.c
    ├── web_server/
    │   ├── CMakeLists.txt
    │   ├── include/web_server.h
    │   ├── web_server.c
    │   ├── handlers_clock.c
    │   ├── handlers_date_temp_humi.c
    │   ├── handlers_scores_cd_ls.c
    │   ├── handlers_scroll_spec.c
    │   └── handlers_system.c
    └── wifi_manager/
        ├── CMakeLists.txt
        ├── include/wifi_manager.h
        ├── wifi_manager.c
        └── captive_portal.c
```

---

## Task 1: Repo-Umstrukturierung & Projekt-Gerüst

**Dateien:**
- Erstellen: `old/` (Verzeichnis)
- Verschieben: `ShelfClock.ino` → `old/ShelfClock.ino`
- Erstellen: `CMakeLists.txt`, `sdkconfig.defaults`, `partitions.csv`
- Erstellen: `main/CMakeLists.txt`, `main/main.c` (Stub)
- Erstellen: Alle `components/*/CMakeLists.txt` (Stubs)

- [ ] **Schritt 1: Alten Code verschieben**

```bash
mkdir old
mv ShelfClock.ino old/ShelfClock.ino
```

- [ ] **Schritt 2: Top-Level CMakeLists.txt erstellen**

`CMakeLists.txt`:
```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(shelfclock)
```

- [ ] **Schritt 3: sdkconfig.defaults erstellen**

`sdkconfig.defaults`:
```
CONFIG_IDF_TARGET="esp32c3"
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
CONFIG_SPIFFS_MAX_PARTITIONS=1
CONFIG_BT_ENABLED=n
CONFIG_MDNS_ENABLED=y
CONFIG_LWIP_LOCAL_HOSTNAME="shelfclock"
CONFIG_ADC_CALI_LINE_FITTING_EFUSE_VREF_ENABLE=y
CONFIG_ESP_TASK_WDT_TIMEOUT_S=10
CONFIG_FREERTOS_HZ=1000
```

- [ ] **Schritt 4: partitions.csv erstellen**

`partitions.csv`:
```
# Name,   Type, SubType,  Offset,   Size
nvs,      data, nvs,      0x9000,   0x7000,
otadata,  data, ota,      0x10000,  0x2000,
app0,     app,  ota_0,    0x20000,  0x180000,
app1,     app,  ota_1,    0x1A0000, 0x180000,
spiffs,   data, spiffs,   0x320000, 0xE0000,
```

- [ ] **Schritt 5: main/ erstellen**

`main/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES storage led_display sensors clock_modes lightshow
             rtttl_player web_server wifi_manager
             nvs_flash esp_wifi esp_event esp_netif
             freertos esp_timer
)
```

`main/main.c` (Stub):
```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "main";

void app_main(void) {
    ESP_LOGI(TAG, "ShelfClock starting...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
}
```

- [ ] **Schritt 6: Alle Komponenten-CMakeLists.txt Stubs erstellen**

`components/storage/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "storage.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash
)
```

`components/led_display/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "led_display.c"
    INCLUDE_DIRS "include"
    REQUIRES led_strip driver freertos
)
```

`components/sensors/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "sensors.c" "dht11.c" "ds3231.c" "adc_light.c" "i2s_mic.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_adc esp_timer freertos
)
```

`components/clock_modes/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "mode_time.c" "mode_date.c" "mode_temperature.c" "mode_humidity.c"
         "mode_scoreboard.c" "mode_countdown.c" "mode_stopwatch.c" "mode_scroll.c"
    INCLUDE_DIRS "include"
    REQUIRES storage led_display sensors rtttl_player esp_timer
)
```

`components/lightshow/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "lightshow.c" "lightshow_fx.c" "spectrum.c"
    INCLUDE_DIRS "include"
    REQUIRES storage led_display sensors freertos
)
```

`components/rtttl_player/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "rtttl_player.c"
    INCLUDE_DIRS "include"
    REQUIRES driver freertos
)
```

`components/web_server/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "web_server.c" "handlers_clock.c" "handlers_date_temp_humi.c"
         "handlers_scores_cd_ls.c" "handlers_scroll_spec.c" "handlers_system.c"
    INCLUDE_DIRS "include"
    REQUIRES storage led_display sensors clock_modes lightshow rtttl_player
             esp_http_server esp_spiffs mdns esp_ota freertos esp_timer
             esp_wifi esp_netif
)
```

`components/wifi_manager/CMakeLists.txt`:
```cmake
idf_component_register(
    SRCS "wifi_manager.c" "captive_portal.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_wifi esp_event esp_netif nvs_flash
             esp_http_server freertos esp_timer
)
```

Jeweils leere `.c`-Dateien und `.h`-Stubs anlegen damit der Build durchläuft:
```bash
for dir in storage led_display sensors clock_modes lightshow rtttl_player web_server wifi_manager; do
  mkdir -p components/$dir/include
  touch components/$dir/include/${dir}.h
done
touch components/storage/storage.c
touch components/led_display/led_display.c
touch components/sensors/sensors.c components/sensors/dht11.c
touch components/sensors/ds3231.c components/sensors/adc_light.c components/sensors/i2s_mic.c
touch components/clock_modes/mode_time.c components/clock_modes/mode_date.c
touch components/clock_modes/mode_temperature.c components/clock_modes/mode_humidity.c
touch components/clock_modes/mode_scoreboard.c components/clock_modes/mode_countdown.c
touch components/clock_modes/mode_stopwatch.c components/clock_modes/mode_scroll.c
touch components/lightshow/lightshow.c components/lightshow/lightshow_fx.c components/lightshow/spectrum.c
touch components/rtttl_player/rtttl_player.c components/rtttl_player/include/rtttl_player.h
touch components/rtttl_player/rtttl_songs.h
touch components/web_server/web_server.c components/web_server/handlers_clock.c
touch components/web_server/handlers_date_temp_humi.c components/web_server/handlers_scores_cd_ls.c
touch components/web_server/handlers_scroll_spec.c components/web_server/handlers_system.c
touch components/wifi_manager/wifi_manager.c components/wifi_manager/captive_portal.c
```

- [ ] **Schritt 7: Build verifizieren**

```bash
cd /Users/michael/projects/ShelfClock
idf.py set-target esp32c3
idf.py build
```

Erwartete Ausgabe: `Project build complete.` (oder nur Linker-Warnungen wegen leerer Dateien)

- [ ] **Schritt 8: Commit**

```bash
git add CMakeLists.txt sdkconfig.defaults partitions.csv main/ components/ old/
git commit -m "feat: add ESP-IDF project scaffold and move Arduino code to old/"
```

---

## Task 2: Storage-Komponente

**Dateien:**
- Erstellen: `components/storage/include/storage.h`
- Erstellen: `components/storage/storage.c`

- [ ] **Schritt 1: Header schreiben**

`components/storage/include/storage.h`:
```c
#pragma once
#include <stdint.h>
#include <stdbool.h>
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
```

- [ ] **Schritt 2: Implementation schreiben**

`components/storage/storage.c`:
```c
#include "storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "storage";
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
    // Standardfarben
    for (int i = 0; i < NUM_COLOR_CHANNELS; i++) {
        g_config.r[i] = 255; g_config.g[i] = 0; g_config.b[i] = 0;
    }
    // Spotlight: gelblich
    g_config.r[0] = 193; g_config.g[0] = 204; g_config.b[0] = 78;
    // Scroll-Text: weiß
    g_config.r[16] = 255; g_config.g[16] = 255; g_config.b[16] = 255;
    // Spectrum-Hintergrund: schwarz
    g_config.r[17] = 0; g_config.g[17] = 0; g_config.b[17] = 0;
    // Countdown-Farbe: grün
    g_config.cd_r = 0; g_config.cd_g = 255; g_config.cd_b = 0;
}

static void load_from_nvs(nvs_handle_t h) {
    int32_t v32; uint8_t u8; bool b; size_t len;

    if (nvs_get_i32(h, "gmtOffset_sec",  &v32) == ESP_OK) g_config.gmt_offset_sec = v32;
    if (nvs_get_u8(h,  "DSTime",         &u8)  == ESP_OK) g_config.ds_time = u8;
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
    if (nvs_get_u8(h,  "alarmCD",        &u8)  == ESP_OK) g_config.use_audible_alarm = u8;
    if (nvs_get_u8(h,  "colorchangeCD",  &u8)  == ESP_OK) g_config.color_change_cd = u8;
    if (nvs_get_u8(h,  "useSpotlights",  &u8)  == ESP_OK) g_config.use_spotlights = u8;
    if (nvs_get_u8(h,  "randSpecMode",   &u8)  == ESP_OK) g_config.random_spectrum_mode = u8;
    if (nvs_get_u8(h,  "scrollOverride", &u8)  == ESP_OK) g_config.scroll_override = u8;
    int8_t i8;
    if (nvs_get_i8(h,  "tempCorrection", &i8)  == ESP_OK) g_config.temperature_correction = i8;

    for (int i = 0; i < 8; i++) {
        char key[16]; snprintf(key, sizeof(key), "scrollOpts%d", i + 1);
        if (nvs_get_u8(h, key, &u8) == ESP_OK) g_config.scroll_options[i] = u8;
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
    nvs_set_u8(h,  "DSTime",         g_config.ds_time);
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
    nvs_set_u8(h,  "alarmCD",        g_config.use_audible_alarm);
    nvs_set_u8(h,  "colorchangeCD",  g_config.color_change_cd);
    nvs_set_u8(h,  "useSpotlights",  g_config.use_spotlights);
    nvs_set_u8(h,  "randSpecMode",   g_config.random_spectrum_mode);
    nvs_set_u8(h,  "scrollOverride", g_config.scroll_override);
    nvs_set_i8(h,  "tempCorrection", g_config.temperature_correction);

    for (int i = 0; i < 8; i++) {
        char key[16]; snprintf(key, sizeof(key), "scrollOpts%d", i + 1);
        nvs_set_u8(h, key, g_config.scroll_options[i]);
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

// Preset laden/speichern mit Namespace "shelfclock-pN"
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
    // Gleiche Keys wie storage_save_all, aber in anderem Namespace
    nvs_set_u8(h, "clockMode",      g_config.clock_mode);
    nvs_set_u8(h, "brightness",     g_config.brightness);
    nvs_set_i32(h, "gmtOffset_sec", g_config.gmt_offset_sec);
    nvs_set_u8(h, "DSTime",         g_config.ds_time);
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
```

- [ ] **Schritt 3: Build verifizieren**

```bash
idf.py build
```
Erwartet: Kompiliert fehlerfrei.

- [ ] **Schritt 4: Commit**

```bash
git add components/storage/
git commit -m "feat: add storage component with NVS config persistence"
```

---

## Task 3: LED-Display-Komponente

**Dateien:**
- Erstellen: `components/led_display/include/led_display.h`
- Erstellen: `components/led_display/led_display.c`

- [ ] **Schritt 1: Header schreiben**

`components/led_display/include/led_display.h`:
```c
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// Konstanten (ident mit Original)
#define LEDS_PER_SEGMENT   7
#define SEGMENTS_PER_NUMBER 7
#define NUMBER_OF_DIGITS   7
#define SPECTRUM_PIXELS    37
#define SEGMENTS_LEDS      (SPECTRUM_PIXELS * LEDS_PER_SEGMENT)   // 259
#define SPOT_LEDS          (NUMBER_OF_DIGITS * 2)                  // 14
#define NUM_LEDS           (SEGMENTS_LEDS + SPOT_LEDS)             // 273
#define FAKE_NUM_LEDS      (NUMBER_OF_DIGITS * SEGMENTS_PER_NUMBER * LEDS_PER_SEGMENT) // 343

#define LED_GPIO_NUM       4

typedef struct { uint8_t r, g, b; } crgb_t;

#define CRGB_BLACK   ((crgb_t){0,   0,   0  })
#define CRGB_RED     ((crgb_t){255, 0,   0  })
#define CRGB_GREEN   ((crgb_t){0,   255, 0  })
#define CRGB_BLUE    ((crgb_t){0,   0,   255})
#define CRGB_WHITE   ((crgb_t){255, 255, 255})
#define CRGB(r,g,b)  ((crgb_t){(r),(g),(b)})

extern crgb_t g_leds[NUM_LEDS];
extern SemaphoreHandle_t g_led_mutex;

// Externe LED-Layout-Arrays (für Lightshow/Spectrum)
extern const uint16_t FAKE_LEDs[FAKE_NUM_LEDS];
extern const uint16_t FAKE_LEDs_C_BMUP[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_CMOT[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_BLTR[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_TLBR[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_TMDN[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_CSIN[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_BRTL[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_TRBL[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_OUTS[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_OUTS2[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_VERT[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_VERT2[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_FIRE[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_C_RAIN[SEGMENTS_LEDS];
extern const uint16_t FAKE_LEDs_SNAKE[SEGMENTS_LEDS];
extern const uint8_t  g_numbers[97];

void led_display_init(void);
void led_refresh(void);
void all_blank(void);
void display_number(uint8_t num, int digit_pos, crgb_t color);
void shelf_down_lights(void);
void blink_dots(bool *dots_on);
void set_brightness(uint8_t brightness);

// Farb-Hilfsfunktionen
crgb_t color_wheel(int pos);
crgb_t color_wheel2(int pos);
crgb_t hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v);
crgb_t random_color(bool pastel);
void   fade_to_black_by(crgb_t *c, uint8_t amount);
```

- [ ] **Schritt 2: Implementation schreiben**

`components/led_display/led_display.c`:
```c
#include "led_display.h"
#include "storage.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>

static const char *TAG = "led_display";
static led_strip_handle_t s_strip;

crgb_t g_leds[NUM_LEDS];
SemaphoreHandle_t g_led_mutex;

// Segment-Bitmuster: bit 0=unten-rechts, 1=unten, 2=links-unten,
//                    3=links-oben, 4=oben, 5=rechts-oben, 6=mitte
const uint8_t g_numbers[97] = {
    0b0111111, 0b0100001, 0b1110110, 0b1110011, 0b1101001,
    0b1011011, 0b1011111, 0b0110001, 0b1111111, 0b1111011,
    0b0000000, 0b0100001, 0b0101000, 0b1101111, 0b1011011,
    0b1100100, 0b1100001, 0b0001000, 0b0011010, 0b0110010,
    0b0011000, 0b1001100, 0b0000100, 0b1000000, 0b0000010,
    0b1100100, 0b1111000, 0b0010010, 0b0010011, 0b1011000,
    0b1000010, 0b1110000, 0b1110100, 0b1110111, 0b1111101,
    0b1001111, 0b0011110, 0b1100111, 0b1011110, 0b1011100,
    0b0011111, 0b1101101, 0b0000100, 0b0100111, 0b1011101,
    0b0001110, 0b0010101, 0b0111101, 0b0111111, 0b1111100,
    0b1111010, 0b0111100, 0b1011011, 0b1001110, 0b0101111,
    0b0101111, 0b0101010, 0b1101101, 0b1101011, 0b1110110,
    0b0011110, 0b1001001, 0b0110011, 0b0111000, 0b0000010,
    0b0100000, 0b1110111, 0b1001111, 0b1000110, 0b1100111,
    0b1111110, 0b1011100, 0b1111011, 0b1001101, 0b0000100,
    0b0000011, 0b1011101, 0b0001100, 0b0000101, 0b1000101,
    0b1000111, 0b1111100, 0b1111001, 0b1000100, 0b1011011,
    0b1001110, 0b0000111, 0b0000111, 0b0000101, 0b1101101,
    0b1101011, 0b1110110, 0b1100001, 0b0001100, 0b1001100,
    0b0010000, 0b1010010
};

// Makros wie im Original
#define seg(n) \
    (n)*LEDS_PER_SEGMENT+0,(n)*LEDS_PER_SEGMENT+1,(n)*LEDS_PER_SEGMENT+2, \
    (n)*LEDS_PER_SEGMENT+3,(n)*LEDS_PER_SEGMENT+4,(n)*LEDS_PER_SEGMENT+5, \
    (n)*LEDS_PER_SEGMENT+6

#define digit0  seg(0),  seg(1),  seg(2),  seg(3),  seg(4),  seg(5),  seg(6)
#define fdigit1 seg(2),  seg(7),  seg(10), seg(15), seg(8),  seg(3),  seg(9)
#define digit2  seg(10), seg(11), seg(12), seg(13), seg(14), seg(15), seg(16)
#define fdigit3 seg(12), seg(17), seg(20), seg(25), seg(18), seg(13), seg(19)
#define digit4  seg(20), seg(21), seg(22), seg(23), seg(24), seg(25), seg(26)
#define fdigit5 seg(22), seg(27), seg(30), seg(35), seg(28), seg(23), seg(29)
#define digit6  seg(30), seg(31), seg(32), seg(33), seg(34), seg(35), seg(36)

const uint16_t FAKE_LEDs[FAKE_NUM_LEDS] = {
    digit0, fdigit1, digit2, fdigit3, digit4, fdigit5, digit6
};

const uint16_t FAKE_LEDs_C_BMUP[SEGMENTS_LEDS] = {
    seg(17),seg(11),seg(21),seg(12),seg(20),seg(19),seg(27),seg(7),seg(22),
    seg(10),seg(26),seg(16),seg(25),seg(13),seg(18),seg(1),seg(31),seg(2),
    seg(30),seg(9),seg(29),seg(15),seg(23),seg(14),seg(24),seg(0),seg(32),
    seg(6),seg(36),seg(3),seg(35),seg(8),seg(28),seg(5),seg(33),seg(4),seg(34)
};
const uint16_t FAKE_LEDs_C_CMOT[SEGMENTS_LEDS] = {
    seg(19),seg(26),seg(16),seg(20),seg(13),seg(25),seg(12),seg(17),seg(18),
    seg(21),seg(14),seg(24),seg(11),seg(29),seg(9),seg(22),seg(15),seg(23),
    seg(10),seg(28),seg(7),seg(27),seg(8),seg(36),seg(6),seg(30),seg(3),
    seg(35),seg(2),seg(34),seg(1),seg(31),seg(4),seg(32),seg(5),seg(33),seg(0)
};
const uint16_t FAKE_LEDs_C_BLTR[SEGMENTS_LEDS] = {
    seg(32),seg(31),seg(27),seg(30),seg(36),seg(33),seg(34),seg(35),seg(29),
    seg(22),seg(21),seg(17),seg(20),seg(26),seg(23),seg(28),seg(24),seg(25),
    seg(19),seg(12),seg(11),seg(7),seg(10),seg(16),seg(13),seg(18),seg(14),
    seg(15),seg(9),seg(2),seg(1),seg(0),seg(6),seg(3),seg(8),seg(4),seg(5)
};
const uint16_t FAKE_LEDs_C_TLBR[SEGMENTS_LEDS] = {
    seg(34),seg(33),seg(32),seg(36),seg(35),seg(28),seg(24),seg(23),seg(29),
    seg(30),seg(31),seg(27),seg(22),seg(26),seg(25),seg(18),seg(14),seg(13),
    seg(19),seg(20),seg(21),seg(17),seg(12),seg(16),seg(15),seg(8),seg(4),
    seg(3),seg(9),seg(10),seg(11),seg(7),seg(2),seg(6),seg(5),seg(0),seg(1)
};
const uint16_t FAKE_LEDs_C_TMDN[SEGMENTS_LEDS] = {
    seg(18),seg(14),seg(24),seg(13),seg(25),seg(19),seg(8),seg(28),seg(15),
    seg(23),seg(16),seg(26),seg(12),seg(20),seg(17),seg(4),seg(34),seg(3),
    seg(35),seg(9),seg(29),seg(10),seg(22),seg(11),seg(21),seg(5),seg(33),
    seg(6),seg(36),seg(2),seg(30),seg(7),seg(27),seg(0),seg(32),seg(1),seg(31)
};
const uint16_t FAKE_LEDs_C_CSIN[SEGMENTS_LEDS] = {
    seg(5),seg(32),seg(0),seg(33),seg(6),seg(36),seg(4),seg(31),seg(1),
    seg(34),seg(3),seg(30),seg(2),seg(35),seg(9),seg(29),seg(8),seg(27),
    seg(7),seg(28),seg(15),seg(22),seg(10),seg(23),seg(16),seg(26),seg(14),
    seg(21),seg(11),seg(24),seg(13),seg(20),seg(12),seg(25),seg(19),seg(18),seg(17)
};
const uint16_t FAKE_LEDs_C_BRTL[SEGMENTS_LEDS] = {
    seg(0),seg(1),seg(7),seg(2),seg(6),seg(5),seg(4),seg(3),seg(9),
    seg(10),seg(11),seg(17),seg(12),seg(16),seg(15),seg(8),seg(14),seg(13),
    seg(19),seg(20),seg(21),seg(27),seg(22),seg(26),seg(25),seg(18),seg(24),
    seg(23),seg(29),seg(30),seg(31),seg(32),seg(36),seg(35),seg(28),seg(34),seg(33)
};
const uint16_t FAKE_LEDs_C_TRBL[SEGMENTS_LEDS] = {
    seg(4),seg(5),seg(0),seg(6),seg(3),seg(8),seg(14),seg(15),seg(9),
    seg(2),seg(1),seg(7),seg(10),seg(16),seg(13),seg(18),seg(24),seg(25),
    seg(19),seg(12),seg(11),seg(17),seg(20),seg(26),seg(23),seg(28),seg(34),
    seg(35),seg(29),seg(22),seg(21),seg(27),seg(30),seg(36),seg(33),seg(32),seg(31)
};
const uint16_t FAKE_LEDs_C_OUTS[SEGMENTS_LEDS] = {
    seg(31),seg(36),seg(36),seg(36),seg(34),seg(36),seg(27),seg(36),seg(29),
    seg(36),seg(28),seg(36),seg(21),seg(36),seg(26),seg(36),seg(24),seg(36),
    seg(17),seg(36),seg(19),seg(36),seg(18),seg(36),seg(11),seg(36),seg(16),
    seg(36),seg(14),seg(36),seg(7),seg(36),seg(9),seg(8),seg(1),seg(6),seg(4)
};
const uint16_t FAKE_LEDs_C_OUTS2[SEGMENTS_LEDS] = {
    seg(1),seg(36),seg(6),seg(36),seg(4),seg(36),seg(7),seg(36),seg(9),
    seg(36),seg(8),seg(36),seg(11),seg(36),seg(16),seg(36),seg(14),seg(36),
    seg(17),seg(36),seg(19),seg(36),seg(18),seg(36),seg(21),seg(36),seg(26),
    seg(36),seg(24),seg(36),seg(27),seg(36),seg(29),seg(28),seg(31),seg(36),seg(34)
};
const uint16_t FAKE_LEDs_C_VERT[SEGMENTS_LEDS] = {
    seg(32),seg(36),seg(33),seg(36),seg(30),seg(36),seg(35),seg(36),seg(36),
    seg(22),seg(36),seg(23),seg(36),seg(36),seg(20),seg(36),seg(25),seg(36),
    seg(36),seg(12),seg(36),seg(13),seg(36),seg(36),seg(10),seg(36),seg(15),
    seg(36),seg(36),seg(2),seg(36),seg(3),seg(36),seg(0),seg(36),seg(5),seg(36)
};
const uint16_t FAKE_LEDs_C_VERT2[SEGMENTS_LEDS] = {
    seg(0),seg(36),seg(5),seg(36),seg(36),seg(2),seg(36),seg(3),seg(36),
    seg(36),seg(10),seg(36),seg(15),seg(36),seg(36),seg(12),seg(36),seg(13),
    seg(36),seg(36),seg(20),seg(36),seg(25),seg(36),seg(36),seg(22),seg(36),
    seg(23),seg(36),seg(30),seg(36),seg(35),seg(36),seg(32),seg(36),seg(33),seg(36)
};
const uint16_t FAKE_LEDs_C_FIRE[SEGMENTS_LEDS] = {
    seg(17),seg(11),seg(21),seg(12),seg(20),seg(19),seg(27),seg(7),seg(22),
    seg(10),seg(26),seg(16),seg(25),seg(13),seg(18),seg(1),seg(31),seg(2),
    seg(30),seg(9),seg(29),seg(15),seg(23),seg(14),seg(24),seg(0),seg(32),
    seg(6),seg(36),seg(3),seg(35),seg(8),seg(28),seg(5),seg(33),seg(4),seg(34)
};
const uint16_t FAKE_LEDs_C_RAIN[SEGMENTS_LEDS] = {
    seg(30),seg(35),seg(1),seg(6),seg(4),seg(22),seg(23),seg(31),seg(36),
    seg(34),seg(12),seg(13),seg(7),seg(9),seg(8),seg(0),seg(5),seg(17),
    seg(19),seg(18),seg(10),seg(15),seg(21),seg(26),seg(24),seg(2),seg(3),
    seg(27),seg(29),seg(28),seg(20),seg(25),seg(11),seg(16),seg(14),seg(32),seg(33)
};
const uint16_t FAKE_LEDs_SNAKE[SEGMENTS_LEDS] = {
    seg(0),seg(1),seg(7),seg(2),seg(6),seg(5),seg(4),seg(3),seg(9),
    seg(10),seg(11),seg(17),seg(12),seg(16),seg(15),seg(8),seg(14),seg(13),
    seg(19),seg(20),seg(21),seg(27),seg(22),seg(26),seg(25),seg(18),seg(24),
    seg(23),seg(29),seg(30),seg(31),seg(32),seg(36),seg(35),seg(28),seg(34),seg(33)
};

void led_display_init(void) {
    g_led_mutex = xSemaphoreCreateMutex();
    memset(g_leds, 0, sizeof(g_leds));

    led_strip_config_t strip_cfg = {
        .strip_gpio_num = LED_GPIO_NUM,
        .max_leds       = NUM_LEDS,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model      = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src       = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip));
    led_strip_clear(s_strip);
}

void led_refresh(void) {
    for (int i = 0; i < NUM_LEDS; i++) {
        led_strip_set_pixel(s_strip, i, g_leds[i].r, g_leds[i].g, g_leds[i].b);
    }
    led_strip_refresh(s_strip);
}

void all_blank(void) {
    memset(g_leds, 0, sizeof(g_leds));
}

void display_number(uint8_t num, int digit_pos, crgb_t color) {
    if (num >= 97) return;
    for (int seg = 0; seg < SEGMENTS_PER_NUMBER; seg++) {
        bool seg_on = (g_numbers[num] >> seg) & 1;
        crgb_t c = seg_on ? color : CRGB_BLACK;
        for (int led = 0; led < LEDS_PER_SEGMENT; led++) {
            int fake_idx = (digit_pos * SEGMENTS_PER_NUMBER + seg) * LEDS_PER_SEGMENT + led;
            if (fake_idx >= FAKE_NUM_LEDS) continue;
            uint16_t real_idx = FAKE_LEDs[fake_idx];
            if (real_idx < NUM_LEDS) g_leds[real_idx] = c;
        }
    }
}

void shelf_down_lights(void) {
    if (!g_config.use_spotlights) {
        for (int i = SEGMENTS_LEDS; i < NUM_LEDS; i++) g_leds[i] = CRGB_BLACK;
        return;
    }
    crgb_t sc;
    if (g_config.spotlights_color_settings == 0) {
        sc = CRGB(g_config.r[0], g_config.g[0], g_config.b[0]);
    } else {
        sc = random_color(g_config.pastel_colors);
    }
    for (int i = SEGMENTS_LEDS; i < NUM_LEDS; i++) g_leds[i] = sc;
}

void blink_dots(bool *dots_on) {
    crgb_t col = *dots_on ? CRGB(g_config.r[3], g_config.g[3], g_config.b[3])
                           : CRGB_BLACK;
    if (g_config.colon_type == 0) {
        int mid = LEDS_PER_SEGMENT / 2;
        for (int i = 25*LEDS_PER_SEGMENT+mid-1; i <= 25*LEDS_PER_SEGMENT+mid; i++)
            if (i < NUM_LEDS) g_leds[i] = col;
        for (int i = 20*LEDS_PER_SEGMENT+mid-1; i <= 20*LEDS_PER_SEGMENT+mid; i++)
            if (i < NUM_LEDS) g_leds[i] = col;
    } else if (g_config.colon_type == 1) {
        for (int i = 25*LEDS_PER_SEGMENT; i < 26*LEDS_PER_SEGMENT; i++)
            if (i < NUM_LEDS) g_leds[i] = col;
        for (int i = 20*LEDS_PER_SEGMENT; i < 21*LEDS_PER_SEGMENT; i++)
            if (i < NUM_LEDS) g_leds[i] = col;
    } else {
        for (int i = 20*LEDS_PER_SEGMENT; i < 21*LEDS_PER_SEGMENT; i++)
            if (i < NUM_LEDS) g_leds[i] = col;
    }
    *dots_on = !(*dots_on);
}

void set_brightness(uint8_t brightness) {
    led_strip_set_pixel_rgbw(s_strip, 0, 0, 0, 0, 0); // no-op just to silence warning
    // Helligkeit via Software-Skalierung
    // led_strip hat keine direkte Helligkeitskontrolle;
    // brightness wird in main_task vor led_refresh() auf g_leds[] angewendet
}

crgb_t color_wheel(int pos) {
    pos &= 0xFF;
    crgb_t c = {0, 0, 0};
    if (pos < 85) {
        c.r = (pos * 3);
        c.g = 0;
        c.b = (255 - pos * 3);
    } else if (pos < 170) {
        pos -= 85;
        c.r = (255 - pos * 3);
        c.g = (pos * 3);
        c.b = 0;
    } else {
        pos -= 170;
        c.r = 0;
        c.g = (255 - pos * 3);
        c.b = (pos * 3);
    }
    return c;
}

crgb_t color_wheel2(int pos) { return color_wheel(pos); }

crgb_t hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v) {
    if (s == 0) return CRGB(v, v, v);
    uint8_t region = h / 43;
    uint8_t remainder = (h - (region * 43)) * 6;
    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    switch (region) {
        case 0: return CRGB(v, t, p);
        case 1: return CRGB(q, v, p);
        case 2: return CRGB(p, v, t);
        case 3: return CRGB(p, q, v);
        case 4: return CRGB(t, p, v);
        default: return CRGB(v, p, q);
    }
}

crgb_t random_color(bool pastel) {
    if (!pastel) return hsv_to_rgb(esp_random() & 0xFF, 255, 255);
    return CRGB(esp_random() & 0xFF, esp_random() & 0xFF, esp_random() & 0xFF);
}

void fade_to_black_by(crgb_t *c, uint8_t amount) {
    c->r = c->r > amount ? c->r - amount : 0;
    c->g = c->g > amount ? c->g - amount : 0;
    c->b = c->b > amount ? c->b - amount : 0;
}
```

- [ ] **Schritt 3: Build verifizieren**

```bash
idf.py build
```
Erwartet: Kompiliert ohne Fehler.

- [ ] **Schritt 4: Commit**

```bash
git add components/led_display/
git commit -m "feat: add led_display component with RMT driver and all layout arrays"
```

---

## Task 4: Sensors-Komponente

**Dateien:**
- Erstellen: `components/sensors/include/sensors.h`
- Erstellen: `components/sensors/sensors.c`, `dht11.c`, `ds3231.c`, `adc_light.c`, `i2s_mic.c`

- [ ] **Schritt 1: Header schreiben**

`components/sensors/include/sensors.h`:
```c
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define DHT11_GPIO_NUM      10
#define I2C_SDA_GPIO_NUM    18
#define I2C_SCL_GPIO_NUM    19
#define ADC_LIGHT_CHANNEL   ADC_CHANNEL_1  // GPIO1
#define I2S_BCK_GPIO_NUM    6
#define I2S_WS_GPIO_NUM     7
#define I2S_DIN_GPIO_NUM    8
#define DS3231_I2C_ADDR     0x68

void     sensors_init(void);
// DHT11
bool     dht11_read(float *temp_c, float *humidity);
// DS3231
bool     ds3231_get_time(struct tm *t);
bool     ds3231_set_time(const struct tm *t);
bool     ds3231_lost_power(void);
// Fotowiderstand
uint16_t adc_light_read(void);
uint8_t  adc_light_to_brightness(uint16_t raw);
// INMP441
int32_t  i2s_mic_get_level(void);
void     i2s_mic_read_samples(int32_t *buf, size_t len);
```

- [ ] **Schritt 2: sensors.c (Init-Wrapper) schreiben**

`components/sensors/sensors.c`:
```c
#include "sensors.h"
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "sensors";
i2c_master_bus_handle_t g_i2c_bus;

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
    ESP_LOGI(TAG, "I2C bus initialized");
}
```

- [ ] **Schritt 3: dht11.c schreiben**

`components/sensors/dht11.c`:
```c
#include "sensors.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

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
```

- [ ] **Schritt 4: ds3231.c schreiben**

`components/sensors/ds3231.c`:
```c
#include "sensors.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>

extern i2c_master_bus_handle_t g_i2c_bus;
static i2c_master_dev_handle_t s_ds3231;
static bool s_initialized = false;

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
    uint8_t reg = 0x00, buf[7];
    if (i2c_master_transmit(s_ds3231, &reg, 1, 100) != ESP_OK) return false;
    if (i2c_master_receive(s_ds3231, buf, 7, 100) != ESP_OK) return false;
    memset(t, 0, sizeof(*t));
    t->tm_sec  = bcd2dec(buf[0] & 0x7F);
    t->tm_min  = bcd2dec(buf[1] & 0x7F);
    t->tm_hour = bcd2dec(buf[2] & 0x3F);
    t->tm_wday = buf[3] - 1;
    t->tm_mday = bcd2dec(buf[4]);
    t->tm_mon  = bcd2dec(buf[5] & 0x1F) - 1;
    t->tm_year = bcd2dec(buf[6]) + 100; // 2000-based → tm_year from 1900
    return true;
}

bool ds3231_set_time(const struct tm *t) {
    ds3231_ensure_init();
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
    uint8_t reg = 0x0F, status;
    if (i2c_master_transmit(s_ds3231, &reg, 1, 100) != ESP_OK) return true;
    if (i2c_master_receive(s_ds3231, &status, 1, 100) != ESP_OK) return true;
    return (status & 0x80) != 0;
}
```

- [ ] **Schritt 5: adc_light.c schreiben**

`components/sensors/adc_light.c`:
```c
#include "sensors.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

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

// Map ADC 0-4095 → brightness 1-255
uint8_t adc_light_to_brightness(uint16_t raw) {
    int mapped = (raw * 254 / 4095) + 1;
    if (mapped < 1) mapped = 1;
    if (mapped > 255) mapped = 255;
    return (uint8_t)mapped;
}
```

- [ ] **Schritt 6: i2s_mic.c schreiben**

`components/sensors/i2s_mic.c`:
```c
#include "sensors.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include <stdlib.h>
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
            .invert_flags = { false, false, false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));
    ESP_LOGI(TAG, "INMP441 I2S initialized");
}

void i2s_mic_read_samples(int32_t *buf, size_t len) {
    size_t bytes_read = 0;
    i2s_channel_read(s_rx, buf, len * sizeof(int32_t), &bytes_read, pdMS_TO_TICKS(100));
    // INMP441: 24-bit in 32-bit frame, left-justified → shift right 8
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
```

- [ ] **Schritt 7: sensors.c um ADC/I2S Init ergänzen**

`components/sensors/sensors.c` (vollständig):
```c
#include "sensors.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

static const char *TAG = "sensors";
i2c_master_bus_handle_t g_i2c_bus;

// Deklarationen der statischen Init-Funktionen aus den anderen .c Dateien
void adc_light_init_static(void);
void i2s_mic_init_static(void);

void sensors_init(void) {
    // I2C Bus
    i2c_master_bus_config_t cfg = {
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .i2c_port          = I2C_NUM_0,
        .scl_io_num        = I2C_SCL_GPIO_NUM,
        .sda_io_num        = I2C_SDA_GPIO_NUM,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &g_i2c_bus));

    // GPIO für DHT11
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
```

- [ ] **Schritt 8: Build verifizieren**

```bash
idf.py build
```
Erwartet: Kompiliert ohne Fehler.

- [ ] **Schritt 9: Commit**

```bash
git add components/sensors/
git commit -m "feat: add sensors component (DHT11, DS3231, ADC, INMP441 I2S)"
```

---

## Task 5: RTTTL-Player-Komponente

**Dateien:**
- Erstellen: `components/rtttl_player/rtttl_songs.h`
- Erstellen: `components/rtttl_player/include/rtttl_player.h`
- Erstellen: `components/rtttl_player/rtttl_player.c`

- [ ] **Schritt 1: Songs-Header schreiben**

`components/rtttl_player/rtttl_songs.h`:
```c
#pragma once

#define RTTTL_SMB_UNDER  "smb_under:d=4,o=6,b=100:32c,32p,32c7,32p,32a5,32p,32a,32p,32a#5,32p,32a#,2p"
#define RTTTL_STARWARS   "StarWars:d=4,o=5,b=250:8a,8p,8d6,8p,8a,8p,8d6,8p,8a,8d6,8p,8a,8p,8g#,a,8a,8g#,8a,g,8f#,8g,8f#,f.,8d.,16p,p.,8a,8p,8d6,8p,8a,8p,8d6,8p,8a,8d6,8p,8a,8p,8g#,8a,8p,8g,8p,g.,8f#,8g,8p,8c6,a#,a,g"
#define RTTTL_BIRTHDAY   "HappyBir:d=8,o=5,b=100:16c,16c,d,c,f,e.,16p,16c,16c,d,c,g,f.,16p,16c,16c,c6,a,f,e,d.,16p,16a#,16a#,a,f,g,f."
#define RTTTL_MARIO      "mario:d=4,o=5,b=100:16e6,16e6,32p,8e6,16c6,8e6,8g6,8p,8g,8p,8c6,16p,8g,16p,8e,16p,8a,8b,16a#,8a,16g.,16e6,16g6,8a6,16f6,8g6,8e6,16c6,16d6,8b,16p"
#define RTTTL_FINALCOUNT "FinalCou:d=4,o=5,b=125:16c#6,16b,c#6,f#,p.,16d6,16c#6,8d6,8c#6,b,p.,16d6,16c#6,d6,f#,p.,16b,16a,8b,8a,8g#,8b,a.,16c#6,16b,c#6,f#,p.,16d6,16c#6,8d6,8c#6,b,p.,16d6,16c#6,d6,f#,p.,16b,16a,8b,8a,8g#,8b,a."
#define RTTTL_RICKROLL   "Never Gonna:d=4,o=5,b=200:8g,8a,8c6,8a,e6,8p,e6,8p,d6.,p,8p,8g,8a,8c6,8a,d6,8p,d6,8p,c6,8b,a.,8g,8a,8c6,8a,2c6,d6,b,a,g.,8p,g,2d6,2c6.,p,8g,8a,8c6,8a,e6,8p,e6,8p,d6."
#define RTTTL_RICKROLL2  "Together:d=8,o=5,b=225:4d#,f.,c#.,c.6,4a#.,4g.,f.,d#.,c.,4a#,2g#"
#define RTTTL_AULDLANG   "AuldLang:d=4,o=6,b=125:a5,d.,8d,d,f#,e.,8d,e,8f#,8e,d.,8d,f#,a,2b.,b,a.,8f#,f#,d,e.,8d,e,8f#,8e,d.,8b5,b5,a5,2d,16p"
#define RTTTL_STARTREK   "Star Trek:d=4,o=5,b=63:8f.,16a#,d#.6,8d6,16a#.,16g.,16c.6,f6"
#define RTTTL_HALLOWEEN  "Hallowee:d=4,o=5,b=160:8c6,8f,8f,8c6,8f,8f,8c6,8f,8c#6,8f,8c6,8f,8f,8c6,8f,8c6,8f,8c#6,8f,8b,8e,8e,8b,8e,8e,8b,8e,8c6,8e,8b,8e,8e,8b,8e,8e,8b,16e"
#define RTTTL_XMAS       "WeWishYo:d=4,o=5,b=200:d,g,8g,8a,8g,8f#,e,e,e,a,8a,8b,8a,8g,f#,d,d,b,8b,8c6,8b,8a,g,e,d,e,a,f#,2g"
#define RTTTL_MACGYVER   "MacGyver:d=8,o=5,b=160:c6,c6,c6,c6,c6,c6,c6,c6,2b,f#,4a,2g,p,c6,4c6,4b,a,b,a,4g,4e6,2a"
#define RTTTL_MANDY      "Mandy:d=8,o=6,b=120:d#,f,d#,d,4c.,f5,d#,d,c,4d#,f,d,4c,2a#5,f5,d,4c,a#5,d.5,4c"
#define RTTTL_ADAMS      "AddamsFa:d=4,o=6,b=50:32p,32c#,16f#,32a#,16f#,32c#,16c,8g#,32f#,16f,32g#,16f,32c#,16a#5,8f#"
#define RTTTL_MSPACMAN   "mspacman:d=4,o=5,b=100:32d,32e,8f,8a,8g,8a#,16a,16a#,16c6,16a,8g,8a#,16a,16a#,16c6,16a"
#define RTTTL_GALAGA     "Galaga:d=4,o=5,b=125:8g4,32c,32p,8d,32f,32p,8e,32c,32p,8d,32a,32p"
#define RTTTL_XMEN       "xmen:d=4,o=6,b=140:16f#5,16g5,16b5,16d,c#,8b5,8f#5,p,16f#5,16g5,16b5,16d"
#define RTTTL_BEETHOVEN  "Beethoven:d=4,o=5,b=140:8e6,8d#6,8e6,8d#6,8e6,8b,8d6,8c6,a,8p,8c,8e,8a,b,8p,8e,8g#,8b,c6"
#define RTTTL_PUFFS      "Powerpuf:d=4,o=5,b=200:8c,p,8c,8p,8d#,8g,8a#,a.,8g,2p,8c6,p,8c6,8p,8d#6"
#define RTTTL_CINCO      "Macarena:d=16,o=5,b=180:4f6,8f6,8f6,4f6,8f6,8f6,8f6,8f6,8f6,8f6,8f6,8a6"
#define RTTTL_TRON       "tron:d=4,o=5,b=200:8f6,8c6,8g,e,8p,8f6,8c6,8g,8f6,8c6,8g,e."
#define RTTTL_BURGERTIME "Burgertime:d=4,o=6,b=285:8f,8f,8f#,8f#,8g#,8g#,8a,8a,a#,f,a#,f"
#define RTTTL_TAKEONME   "TakeOnMe:d=16,o=5,b=100:8p,a#,a#,a#,8f#,8d#,8g#,8g#,g#,c6,c6,c#6,d#6"
#define RTTTL_NOKIA      "NokiaTun:d=4,o=5,b=225:8e6,8d6,f#,g#,8c#6,8b,d,e,8b,8a,c#,e,2a"

// Song-Index → String (für Alarm-Auswahl)
extern const char *const RTTTL_SONGS[];
extern const int RTTTL_NUM_SONGS;
```

- [ ] **Schritt 2: Player-Header schreiben**

`components/rtttl_player/include/rtttl_player.h`:
```c
#pragma once
#include <stdatomic.h>

#define BUZZER_GPIO_NUM    16
#define RTTTL_SONG_SMB      0
#define RTTTL_SONG_BIRTHDAY 3
#define RTTTL_SONG_STARWARS 1
#define RTTTL_SONG_AULDLANG 7
#define RTTTL_SONG_XMAS     10

extern atomic_bool g_rtttl_breakout;

void rtttl_player_init(void);
void rtttl_play_song(int song_index);
void rtttl_stop(void);
bool rtttl_is_playing(void);
```

- [ ] **Schritt 3: Player-Implementation schreiben**

`components/rtttl_player/rtttl_player.c`:
```c
#include "rtttl_player.h"
#include "rtttl_songs.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

static const char *TAG = "rtttl";
static QueueHandle_t s_queue;
static bool s_playing = false;

atomic_bool g_rtttl_breakout = false;

const char *const RTTTL_SONGS[] = {
    RTTTL_SMB_UNDER, RTTTL_STARWARS, RTTTL_RICKROLL2, RTTTL_MARIO,
    RTTTL_FINALCOUNT, RTTTL_RICKROLL, RTTTL_MSPACMAN, RTTTL_AULDLANG,
    RTTTL_STARTREK,  RTTTL_XMEN,     RTTTL_GALAGA,    RTTTL_BEETHOVEN,
    RTTTL_PUFFS,     RTTTL_ADAMS,    RTTTL_BURGERTIME, RTTTL_TRON,
    RTTTL_HALLOWEEN, RTTTL_MANDY,    RTTTL_MACGYVER,  RTTTL_TAKEONME,
    RTTTL_NOKIA,     RTTTL_BIRTHDAY, RTTTL_XMAS,      RTTTL_CINCO,
    RTTTL_RICKROLL,
};
const int RTTTL_NUM_SONGS = sizeof(RTTTL_SONGS) / sizeof(RTTTL_SONGS[0]);

static void ledc_set_note(uint32_t freq_hz) {
    if (freq_hz == 0) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    } else {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq_hz);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512); // 50% duty
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
}

// Noten-Frequenzen: C4=262, D4=294, E4=330, F4=349, G4=392, A4=440, B4=494
// Vorzeichen: '#'=+1 Halbton, '.'=1.5x Länge, 'p'=Pause
static const uint32_t NOTE_FREQ[7] = {262, 294, 330, 349, 392, 440, 494}; // C D E F G A B

static uint32_t note_to_freq(char note, bool sharp, int octave) {
    if (note == 'p') return 0;
    int idx = -1;
    switch (note) {
        case 'c': idx = 0; break; case 'd': idx = 1; break; case 'e': idx = 2; break;
        case 'f': idx = 3; break; case 'g': idx = 4; break; case 'a': idx = 5; break;
        case 'b': idx = 6; break;
    }
    if (idx < 0) return 0;
    uint32_t f = NOTE_FREQ[idx];
    if (sharp) f = (uint32_t)(f * 1.059463f);
    // Octave shift from octave 4
    int shift = octave - 4;
    if (shift > 0) f <<= shift;
    else if (shift < 0) f >>= (-shift);
    return f;
}

static void play_rtttl(const char *song) {
    // Parse header: name:d=D,o=O,b=B:notes
    const char *p = strchr(song, ':');
    if (!p) return; p++;
    int default_dur = 4, default_oct = 6, bpm = 63;
    if (*p == 'd') { p += 2; default_dur = atoi(p); while (*p && *p != ',') p++; p++; }
    if (*p == 'o') { p += 2; default_oct = atoi(p); while (*p && *p != ',') p++; p++; }
    if (*p == 'b') { p += 2; bpm = atoi(p); while (*p && *p != ':') p++; p++; }

    uint32_t whole_ms = 240000 / bpm; // 4 beats = one whole note

    while (*p && !atomic_load(&g_rtttl_breakout)) {
        int dur = 0;
        if (*p >= '1' && *p <= '9') { dur = atoi(p); while (*p >= '0' && *p <= '9') p++; }
        if (!dur) dur = default_dur;

        char note = 0;
        if ((*p >= 'a' && *p <= 'g') || *p == 'p') note = *p++;

        bool sharp = (*p == '#') ? (p++, true) : false;
        bool dot   = false;
        int oct = default_oct;
        if (*p >= '4' && *p <= '7') oct = *p++ - '0';
        if (*p == '.') { dot = true; p++; }

        uint32_t dur_ms = whole_ms / dur;
        if (dot) dur_ms = dur_ms * 3 / 2;

        ledc_set_note(note_to_freq(note, sharp, oct));
        vTaskDelay(pdMS_TO_TICKS(dur_ms * 85 / 100)); // 85% note, 15% gap
        ledc_set_note(0);
        vTaskDelay(pdMS_TO_TICKS(dur_ms * 15 / 100));

        if (*p == ',') p++;
    }
    ledc_set_note(0);
}

static void rtttl_task(void *arg) {
    int song_idx;
    while (1) {
        if (xQueueReceive(s_queue, &song_idx, portMAX_DELAY)) {
            s_playing = true;
            atomic_store(&g_rtttl_breakout, false);
            if (song_idx >= 0 && song_idx < RTTTL_NUM_SONGS) {
                play_rtttl(RTTTL_SONGS[song_idx]);
            }
            s_playing = false;
        }
    }
}

void rtttl_player_init(void) {
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 2000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);
    ledc_channel_config_t ch = {
        .gpio_num   = BUZZER_GPIO_NUM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&ch);
    s_queue = xQueueCreate(4, sizeof(int));
    xTaskCreate(rtttl_task, "rtttl", 4096, NULL, 3, NULL);
}

void rtttl_play_song(int idx) { xQueueSend(s_queue, &idx, 0); }
void rtttl_stop(void) { atomic_store(&g_rtttl_breakout, true); }
bool rtttl_is_playing(void) { return s_playing; }
```

- [ ] **Schritt 4: Build verifizieren**

```bash
idf.py build
```

- [ ] **Schritt 5: Commit**

```bash
git add components/rtttl_player/
git commit -m "feat: add rtttl_player component with LEDC buzzer and 24 songs"
```

---

## Task 6: WiFi-Manager-Komponente

**Dateien:**
- Erstellen: `components/wifi_manager/include/wifi_manager.h`
- Erstellen: `components/wifi_manager/wifi_manager.c`
- Erstellen: `components/wifi_manager/captive_portal.c`

- [ ] **Schritt 1: Header schreiben**

`components/wifi_manager/include/wifi_manager.h`:
```c
#pragma once
#include <stdbool.h>
#include "esp_netif.h"

bool wifi_manager_init(void);    // true = verbunden, false = SoftAP gestartet
bool wifi_manager_is_connected(void);
void wifi_manager_get_ip(char *buf, size_t len);
void captive_portal_start(void); // interner Aufruf
```

- [ ] **Schritt 2: wifi_manager.c schreiben**

`components/wifi_manager/wifi_manager.c`:
```c
#include "wifi_manager.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MAX_RETRY          5

static const char *TAG = "wifi_manager";
static EventGroupHandle_t s_events;
static int s_retry = 0;
static bool s_connected = false;
static esp_netif_t *s_sta_netif = NULL;

static void event_handler(void *arg, esp_event_base_t base,
                          int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry < MAX_RETRY) {
            esp_wifi_connect(); s_retry++;
        } else {
            xEventGroupSetBits(s_events, WIFI_FAIL_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_retry = 0;
        s_connected = true;
        xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
    }
}

static bool try_connect_from_nvs(void) {
    nvs_handle_t h;
    if (nvs_open("wifi_creds", NVS_READONLY, &h) != ESP_OK) return false;
    char ssid[33] = {0}, pass[65] = {0};
    size_t len = sizeof(ssid);
    if (nvs_get_str(h, "ssid", ssid, &len) != ESP_OK) { nvs_close(h); return false; }
    len = sizeof(pass);
    nvs_get_str(h, "pass", pass, &len);
    nvs_close(h);
    if (strlen(ssid) == 0) return false;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, event_handler, NULL);

    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid,     ssid, sizeof(wc.sta.ssid) - 1);
    strncpy((char *)wc.sta.password, pass, sizeof(wc.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_events,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
        pdMS_TO_TICKS(15000));
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

bool wifi_manager_init(void) {
    s_events = xEventGroupCreate();
    if (try_connect_from_nvs()) {
        ESP_LOGI(TAG, "WiFi connected");
        return true;
    }
    ESP_LOGW(TAG, "No credentials or connect failed — starting SoftAP");
    captive_portal_start();
    return false;
}

bool wifi_manager_is_connected(void) { return s_connected; }

void wifi_manager_get_ip(char *buf, size_t len) {
    if (!s_sta_netif) { strncpy(buf, "0.0.0.0", len); return; }
    esp_netif_ip_info_t info;
    esp_netif_get_ip_info(s_sta_netif, &info);
    snprintf(buf, len, IPSTR, IP2STR(&info.ip));
}
```

- [ ] **Schritt 3: captive_portal.c schreiben**

`components/wifi_manager/captive_portal.c`:
```c
#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "captive_portal";

static const char PORTAL_HTML[] =
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<title>ShelfClock Setup</title>"
    "<style>body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:20px;}"
    "input{width:100%;padding:8px;margin:8px 0;box-sizing:border-box;}"
    "button{width:100%;padding:10px;background:#007bff;color:white;border:none;cursor:pointer;}"
    "</style></head><body>"
    "<h2>ShelfClock WiFi Setup</h2>"
    "<form method='POST' action='/save'>"
    "<label>SSID:</label><input name='ssid' required><br>"
    "<label>Password:</label><input name='pass' type='password'><br>"
    "<button type='submit'>Verbinden</button>"
    "</form></body></html>";

static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req, PORTAL_HTML);
    return ESP_OK;
}

static void url_decode(char *dst, const char *src, size_t max) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < max - 1; i++) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            char hex[3] = {src[i+1], src[i+2], 0};
            dst[j++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
        } else {
            dst[j++] = src[i];
        }
    }
    dst[j] = '\0';
}

static esp_err_t save_handler(httpd_req_t *req) {
    char body[256] = {0};
    int n = httpd_req_recv(req, body, sizeof(body) - 1);
    if (n <= 0) return ESP_FAIL;

    char raw_ssid[128] = {0}, raw_pass[128] = {0};
    char *s = strstr(body, "ssid=");
    if (s) {
        s += 5;
        char *e = strchr(s, '&');
        size_t len = e ? (size_t)(e - s) : strlen(s);
        strncpy(raw_ssid, s, len < sizeof(raw_ssid)-1 ? len : sizeof(raw_ssid)-1);
    }
    char *p = strstr(body, "pass=");
    if (p) {
        p += 5;
        strncpy(raw_pass, p, sizeof(raw_pass) - 1);
    }

    char ssid[64] = {0}, pass[64] = {0};
    url_decode(ssid, raw_ssid, sizeof(ssid));
    url_decode(pass, raw_pass, sizeof(pass));

    nvs_handle_t h;
    if (nvs_open("wifi_creds", NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "ssid", ssid);
        nvs_set_str(h, "pass", pass);
        nvs_commit(h);
        nvs_close(h);
    }

    httpd_resp_sendstr(req, "<html><body><h2>Gespeichert! Neustart...</h2></body></html>");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();
    return ESP_OK;
}

void captive_portal_start(void) {
    esp_netif_init();
    if (!esp_event_loop_create_default()) {}  // may already exist
    esp_netif_t *ap = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid            = "ShelfClock-Setup",
            .ssid_len        = 16,
            .channel         = 1,
            .password        = "",
            .max_connection  = 4,
            .authmode        = WIFI_AUTH_OPEN,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();
    ESP_LOGI(TAG, "SoftAP started: ShelfClock-Setup @ 192.168.4.1");

    httpd_handle_t server = NULL;
    httpd_config_t hcfg = HTTPD_DEFAULT_CONFIG();
    hcfg.lru_purge_enable = true;
    httpd_start(&server, &hcfg);

    httpd_uri_t root = { .uri="/",     .method=HTTP_GET,  .handler=root_handler };
    httpd_uri_t save = { .uri="/save", .method=HTTP_POST, .handler=save_handler };
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &save);
}
```

- [ ] **Schritt 4: Build verifizieren**

```bash
idf.py build
```

- [ ] **Schritt 5: Commit**

```bash
git add components/wifi_manager/
git commit -m "feat: add wifi_manager with STA connection and SoftAP captive portal"
```

---

## Task 7: Web-Server-Kern

**Dateien:**
- Erstellen: `components/web_server/include/web_server.h`
- Erstellen: `components/web_server/web_server.c`

- [ ] **Schritt 1: Header schreiben**

`components/web_server/include/web_server.h`:
```c
#pragma once
#include "esp_http_server.h"

extern httpd_handle_t g_httpd;

void web_server_init(void);

// Handler-Registrierungsfunktionen (aus handler-Dateien)
void register_handlers_clock(httpd_handle_t server);
void register_handlers_date_temp_humi(httpd_handle_t server);
void register_handlers_scores_cd_ls(httpd_handle_t server);
void register_handlers_scroll_spec(httpd_handle_t server);
void register_handlers_system(httpd_handle_t server);

// Hilfsfunktionen für alle Handler
esp_err_t get_body_param(httpd_req_t *req, const char *key,
                         char *val, size_t val_len);
void      parse_hex_color(const char *hex, uint8_t *r, uint8_t *g, uint8_t *b);
```

- [ ] **Schritt 2: web_server.c schreiben**

`components/web_server/web_server.c`:
```c
#include "web_server.h"
#include "storage.h"
#include "esp_spiffs.h"
#include "mdns.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "web_server";
httpd_handle_t g_httpd = NULL;

// ── SPIFFS ────────────────────────────────────────────────────────────────────
static void spiffs_init(void) {
    esp_vfs_spiffs_conf_t cfg = {
        .base_path              = "/spiffs",
        .partition_label        = NULL,
        .max_files              = 8,
        .format_if_mount_failed = false,
    };
    esp_err_t ret = esp_vfs_spiffs_register(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
    } else {
        size_t total = 0, used = 0;
        esp_spiffs_info(NULL, &total, &used);
        ESP_LOGI(TAG, "SPIFFS: %d/%d bytes used", (int)used, (int)total);
    }
}

// ── Static-File-Handler (Wildcard GET) ───────────────────────────────────────
static esp_err_t static_file_handler(httpd_req_t *req) {
    char path[128];
    snprintf(path, sizeof(path), "/spiffs%s",
             strcmp(req->uri, "/") == 0 ? "/index.html" : req->uri);

    // Einfache MIME-Erkennung
    const char *mime = "application/octet-stream";
    if (strstr(path, ".html")) mime = "text/html";
    else if (strstr(path, ".css"))  mime = "text/css";
    else if (strstr(path, ".js"))   mime = "application/javascript";
    else if (strstr(path, ".ico"))  mime = "image/x-icon";
    else if (strstr(path, ".png"))  mime = "image/png";

    FILE *f = fopen(path, "r");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, mime);
    char buf[512];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        httpd_resp_send_chunk(req, buf, (ssize_t)n);
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

// ── OTA-Handler ───────────────────────────────────────────────────────────────
static esp_ota_handle_t s_ota_handle = 0;
static const esp_partition_t *s_ota_part = NULL;

static esp_err_t ota_post_handler(httpd_req_t *req) {
    char buf[1024];
    int  remaining = req->content_len;
    bool started = false;
    esp_err_t err = ESP_OK;

    while (remaining > 0) {
        int recv = httpd_req_recv(req, buf,
                                  remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf));
        if (recv <= 0) { err = ESP_FAIL; break; }
        if (!started) {
            s_ota_part = esp_ota_get_next_update_partition(NULL);
            err = esp_ota_begin(s_ota_part, OTA_WITH_SEQUENTIAL_WRITES, &s_ota_handle);
            if (err != ESP_OK) break;
            started = true;
        }
        err = esp_ota_write(s_ota_handle, buf, recv);
        if (err != ESP_OK) break;
        remaining -= recv;
    }

    if (err == ESP_OK && started) {
        err = esp_ota_end(s_ota_handle);
        if (err == ESP_OK) {
            esp_ota_set_boot_partition(s_ota_part);
            httpd_resp_sendstr(req, "OK");
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
            return ESP_OK;
        }
    }
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA failed");
    return ESP_FAIL;
}

// ── Hilfsfunktionen ───────────────────────────────────────────────────────────
esp_err_t get_body_param(httpd_req_t *req, const char *key,
                          char *val, size_t val_len) {
    char body[512] = {0};
    int n = httpd_req_recv(req, body,
                           req->content_len < sizeof(body)-1
                               ? req->content_len : sizeof(body)-1);
    if (n <= 0) return ESP_FAIL;
    body[n] = '\0';

    char search[64];
    snprintf(search, sizeof(search), "%s=", key);
    char *p = strstr(body, search);
    if (!p) return ESP_FAIL;
    p += strlen(search);
    char *e = strchr(p, '&');
    size_t len = e ? (size_t)(e - p) : strlen(p);
    if (len >= val_len) len = val_len - 1;
    memcpy(val, p, len);
    val[len] = '\0';
    return ESP_OK;
}

void parse_hex_color(const char *hex, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (hex[0] == '#') hex++;
    unsigned int v = (unsigned int)strtoul(hex, NULL, 16);
    *r = (v >> 16) & 0xFF;
    *g = (v >>  8) & 0xFF;
    *b =  v        & 0xFF;
}

// ── Server starten ────────────────────────────────────────────────────────────
void web_server_init(void) {
    spiffs_init();

    // mDNS
    mdns_init();
    mdns_hostname_set("shelfclock");
    mdns_instance_name_set("ShelfClock");
    ESP_LOGI(TAG, "mDNS: http://shelfclock.local");

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers    = 120;
    cfg.stack_size          = 8192;
    cfg.lru_purge_enable    = true;
    cfg.uri_match_fn        = httpd_uri_match_wildcard;

    ESP_ERROR_CHECK(httpd_start(&g_httpd, &cfg));

    // Statische Dateien (Wildcard)
    httpd_uri_t static_get = {
        .uri     = "/*",
        .method  = HTTP_GET,
        .handler = static_file_handler,
    };
    httpd_register_uri_handler(g_httpd, &static_get);

    // OTA
    httpd_uri_t ota = {
        .uri     = "/update",
        .method  = HTTP_POST,
        .handler = ota_post_handler,
    };
    httpd_register_uri_handler(g_httpd, &ota);

    // Alle API-Handler registrieren
    register_handlers_clock(g_httpd);
    register_handlers_date_temp_humi(g_httpd);
    register_handlers_scores_cd_ls(g_httpd);
    register_handlers_scroll_spec(g_httpd);
    register_handlers_system(g_httpd);

    ESP_LOGI(TAG, "HTTP server started");
}
```

- [ ] **Schritt 3: Build verifizieren**

```bash
idf.py build
```

- [ ] **Schritt 4: Commit**

```bash
git add components/web_server/include/web_server.h components/web_server/web_server.c
git commit -m "feat: add web_server core with SPIFFS, mDNS, OTA and wildcard static handler"
```

---

## Task 8: Web-Server-Handler

**Dateien:**
- Erstellen: `components/web_server/handlers_clock.c`
- Erstellen: `components/web_server/handlers_date_temp_humi.c`
- Erstellen: `components/web_server/handlers_scores_cd_ls.c`
- Erstellen: `components/web_server/handlers_scroll_spec.c`
- Erstellen: `components/web_server/handlers_system.c`

Alle Handler folgen demselben Muster. GET-Handler lesen aus `g_config` und senden als `text/plain`. POST-Handler lesen den Body, parsen den Parameter, schreiben in `g_config` und rufen `storage_save_all()` auf.

Hilfsmakro in jeder Datei:
```c
#define SEND_OK(req)  httpd_resp_set_type(req,"text/json"); \
                      httpd_resp_sendstr(req,"{\"result\":\"ok\"}"); \
                      return ESP_OK
#define SEND_INT(req, v) do { char _b[16]; snprintf(_b,sizeof(_b),"%d",(int)(v)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_b); return ESP_OK; } while(0)
#define SEND_HEX(req, r, g, b) do { char _c[8]; \
    snprintf(_c,sizeof(_c),"#%02X%02X%02X",(r),(g),(b)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_c); return ESP_OK; } while(0)
```

- [ ] **Schritt 1: handlers_clock.c schreiben**

`components/web_server/handlers_clock.c`:
```c
#include "web_server.h"
#include "storage.h"
#include "led_display.h"
#include "clock_modes.h"
#include "rtttl_player.h"
#include "sensors.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

#define SEND_OK(req)  do { httpd_resp_set_type(req,"text/json"); \
    httpd_resp_sendstr(req,"{\"result\":\"ok\"}"); return ESP_OK; } while(0)
#define SEND_INT(req, v) do { char _b[16]; snprintf(_b,sizeof(_b),"%d",(int)(v)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_b); return ESP_OK; } while(0)
#define SEND_HEX(req, r, g, b) do { char _c[8]; \
    snprintf(_c,sizeof(_c),"#%02X%02X%02X",(r),(g),(b)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_c); return ESP_OK; } while(0)

// ── Mode switches ─────────────────────────────────────────────────────────────
static esp_err_t h_go_clock(httpd_req_t *r) {
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 0; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    storage_save_all(); SEND_OK(r);
}
static esp_err_t h_go_temperature(httpd_req_t *r) {
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 2; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    storage_save_all(); SEND_OK(r);
}
static esp_err_t h_go_date(httpd_req_t *r) {
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 7; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    storage_save_all(); SEND_OK(r);
}
static esp_err_t h_go_humidity(httpd_req_t *r) {
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 8; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    storage_save_all(); SEND_OK(r);
}
static esp_err_t h_go_display_off(httpd_req_t *r) {
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 10; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    rtttl_stop(); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_go_scroll(httpd_req_t *r) {
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 11; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    storage_save_all(); SEND_OK(r);
}
static esp_err_t h_go_scoreboard(httpd_req_t *r) {
    char lbuf[8]={0}, rbuf[8]={0};
    get_body_param(r, "left",  lbuf, sizeof(lbuf));
    get_body_param(r, "right", rbuf, sizeof(rbuf));
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    g_config.scoreboard_left  = atoi(lbuf);
    g_config.scoreboard_right = atoi(rbuf);
    if (g_config.scoreboard_left  < 0)  g_config.scoreboard_left  = 0;
    if (g_config.scoreboard_left  > 99) g_config.scoreboard_left  = 99;
    if (g_config.scoreboard_right < 0)  g_config.scoreboard_right = 0;
    if (g_config.scoreboard_right > 99) g_config.scoreboard_right = 99;
    all_blank(); g_config.clock_mode = 3; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    storage_save_all(); SEND_OK(r);
}
static esp_err_t h_go_countdown(httpd_req_t *r) {
    char msbuf[16]={0};
    get_body_param(r, "ms", msbuf, sizeof(msbuf));
    int32_t ms = atoi(msbuf);
    if (ms < 1000)     ms = 1000;
    if (ms > 86400000) ms = 86400000;
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 1; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    mode_countdown_start(ms); SEND_OK(r);
}
static esp_err_t h_go_stopwatch(httpd_req_t *r) {
    char msbuf[16]={0};
    get_body_param(r, "ms", msbuf, sizeof(msbuf));
    int32_t ms = atoi(msbuf);
    if (ms < 1000)     ms = 1000;
    if (ms > 86400000) ms = 86400000;
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 4; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    mode_stopwatch_start(ms); SEND_OK(r);
}
static esp_err_t h_go_lightshow(httpd_req_t *r) {
    char buf[8]={0};
    get_body_param(r, "lightshowMode", buf, sizeof(buf));
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 5; g_config.realtime_mode = 1;
    g_config.lightshow_mode = atoi(buf);
    xSemaphoreGive(g_config_mutex);
    storage_save_all(); SEND_OK(r);
}
static esp_err_t h_go_spectrum(httpd_req_t *r) {
    char buf[8]={0};
    get_body_param(r, "spectrumMode", buf, sizeof(buf));
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 9; g_config.realtime_mode = 1;
    g_config.spectrum_mode = atoi(buf);
    xSemaphoreGive(g_config_mutex);
    storage_save_all(); SEND_OK(r);
}
static esp_err_t h_get_preset1(httpd_req_t *r) {
    storage_load_preset(1); SEND_OK(r);
}
static esp_err_t h_get_preset2(httpd_req_t *r) {
    storage_load_preset(2); SEND_OK(r);
}
static esp_err_t h_set_preset1(httpd_req_t *r) {
    storage_save_preset(1); SEND_OK(r);
}
static esp_err_t h_set_preset2(httpd_req_t *r) {
    storage_save_preset(2); SEND_OK(r);
}

// ── setdate ───────────────────────────────────────────────────────────────────
static esp_err_t h_setdate(httpd_req_t *r) {
    char year[8]={0}, month[4]={0}, day[4]={0};
    char hour[4]={0}, min[4]={0}, sec[4]={0};
    get_body_param(r, "year",  year,  sizeof(year));
    get_body_param(r, "month", month, sizeof(month));
    get_body_param(r, "day",   day,   sizeof(day));
    get_body_param(r, "hour",  hour,  sizeof(hour));
    get_body_param(r, "min",   min,   sizeof(min));
    get_body_param(r, "sec",   sec,   sizeof(sec));
    struct tm tm = {0};
    tm.tm_year = atoi(year) - 1900;
    tm.tm_mon  = atoi(month) - 1;
    tm.tm_mday = atoi(day);
    tm.tm_hour = atoi(hour);
    tm.tm_min  = atoi(min);
    tm.tm_sec  = atoi(sec);
    time_t t = mktime(&tm);
    struct timeval tv = { .tv_sec = t };
    settimeofday(&tv, NULL);
    ds3231_set_time(&tm);
    SEND_OK(r);
}

// ── Clock-Einstellungen ───────────────────────────────────────────────────────
static esp_err_t h_get_clock_display_type(httpd_req_t *r) { SEND_INT(r, g_config.clock_display_type); }
static esp_err_t h_get_colon_type(httpd_req_t *r)         { SEND_INT(r, g_config.colon_type); }
static esp_err_t h_get_gmt_offset(httpd_req_t *r)         { SEND_INT(r, g_config.gmt_offset_sec); }
static esp_err_t h_get_ds_time(httpd_req_t *r)            { SEND_INT(r, g_config.ds_time); }
static esp_err_t h_get_clock_color_settings(httpd_req_t *r) { SEND_INT(r, g_config.clock_color_settings); }
static esp_err_t h_get_color_hour(httpd_req_t *r)  { SEND_HEX(r, g_config.r[1], g_config.g[1], g_config.b[1]); }
static esp_err_t h_get_color_mins(httpd_req_t *r)  { SEND_HEX(r, g_config.r[2], g_config.g[2], g_config.b[2]); }
static esp_err_t h_get_color_colon(httpd_req_t *r) { SEND_HEX(r, g_config.r[3], g_config.g[3], g_config.b[3]); }

static esp_err_t h_update_clock_display_type(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r, "clockDisplayType", buf, sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.clock_display_type = atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_colon_type(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r, "colonType", buf, sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.colon_type = atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_timezone(httpd_req_t *r) {
    char gmt[16]={0}, dst[4]={0};
    get_body_param(r, "gmtOffset", gmt, sizeof(gmt));
    get_body_param(r, "DSTime",    dst, sizeof(dst));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.gmt_offset_sec = atoi(gmt);
    g_config.ds_time        = atoi(dst) ? 1 : 0;
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_ds_time(httpd_req_t *r) {
    char buf[4]={0}; get_body_param(r, "DSTime", buf, sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.ds_time = atoi(buf) ? 1 : 0;
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

static esp_err_t h_update_hour_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"hourColor",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[1],&g_config.g[1],&g_config.b[1]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_mins_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"minsColor",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[2],&g_config.g[2],&g_config.b[2]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_colon_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"colonColor",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[3],&g_config.g[3],&g_config.b[3]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_clock_color_settings(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"ClockColorSettings",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.clock_color_settings = atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

// ── Registrierung ─────────────────────────────────────────────────────────────
#define REG(s, m, u, h) do { httpd_uri_t _u={.uri=(u),.method=(m),.handler=(h)}; \
    httpd_register_uri_handler((s),&_u); } while(0)

void register_handlers_clock(httpd_handle_t s) {
    REG(s, HTTP_POST, "/goClockMode",       h_go_clock);
    REG(s, HTTP_POST, "/goTemperatureMode", h_go_temperature);
    REG(s, HTTP_POST, "/goDateMode",        h_go_date);
    REG(s, HTTP_POST, "/goHumidityMode",    h_go_humidity);
    REG(s, HTTP_POST, "/goDisplayOffMode",  h_go_display_off);
    REG(s, HTTP_POST, "/goScrollingMode",   h_go_scroll);
    REG(s, HTTP_POST, "/goScoreboardMode",  h_go_scoreboard);
    REG(s, HTTP_POST, "/goCountdownMode",   h_go_countdown);
    REG(s, HTTP_POST, "/goStopwatchMode",   h_go_stopwatch);
    REG(s, HTTP_POST, "/goLightshowMode",   h_go_lightshow);
    REG(s, HTTP_POST, "/goSpectrumMode",    h_go_spectrum);
    REG(s, HTTP_POST, "/getPreset1",        h_get_preset1);
    REG(s, HTTP_POST, "/getPreset2",        h_get_preset2);
    REG(s, HTTP_POST, "/setpreset1",        h_set_preset1);
    REG(s, HTTP_POST, "/setpreset2",        h_set_preset2);
    REG(s, HTTP_POST, "/setdate",           h_setdate);
    REG(s, HTTP_GET,  "/getClockDisplayType",    h_get_clock_display_type);
    REG(s, HTTP_GET,  "/getcolonType",           h_get_colon_type);
    REG(s, HTTP_GET,  "/getgmtOffset_sec",       h_get_gmt_offset);
    REG(s, HTTP_GET,  "/getDSTime",              h_get_ds_time);
    REG(s, HTTP_GET,  "/getClockColorSettings",  h_get_clock_color_settings);
    REG(s, HTTP_GET,  "/getcolorHour",           h_get_color_hour);
    REG(s, HTTP_GET,  "/getcolorMins",           h_get_color_mins);
    REG(s, HTTP_GET,  "/getcolorColon",          h_get_color_colon);
    REG(s, HTTP_POST, "/updateClockDisplayType", h_update_clock_display_type);
    REG(s, HTTP_POST, "/updateColonType",        h_update_colon_type);
    REG(s, HTTP_POST, "/updateTimezoneSettings", h_update_timezone);
    REG(s, HTTP_POST, "/updateDSTime",           h_update_ds_time);
    REG(s, HTTP_POST, "/updateHourColor",        h_update_hour_color);
    REG(s, HTTP_POST, "/updateMinsColor",        h_update_mins_color);
    REG(s, HTTP_POST, "/updateColonColor",       h_update_colon_color);
    REG(s, HTTP_POST, "/updateClockColorSettings", h_update_clock_color_settings);
}
```

- [ ] **Schritt 2: handlers_date_temp_humi.c schreiben**

`components/web_server/handlers_date_temp_humi.c` — gleiche Struktur, alle Endpoints für Date/Temp/Humi. Farbindizes: Date=r[4..6], Temp=r[7..9], Humi=r[10..12].

```c
#include "web_server.h"
#include "storage.h"
#include <stdlib.h>
#include <string.h>

#define SEND_OK(req)  do { httpd_resp_set_type(req,"text/json"); \
    httpd_resp_sendstr(req,"{\"result\":\"ok\"}"); return ESP_OK; } while(0)
#define SEND_INT(req, v) do { char _b[16]; snprintf(_b,sizeof(_b),"%d",(int)(v)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_b); return ESP_OK; } while(0)
#define SEND_HEX(req, r, g, b) do { char _c[8]; \
    snprintf(_c,sizeof(_c),"#%02X%02X%02X",(r),(g),(b)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_c); return ESP_OK; } while(0)
#define REG(s,m,u,h) do { httpd_uri_t _u={.uri=(u),.method=(m),.handler=(h)}; \
    httpd_register_uri_handler((s),&_u); } while(0)

// ── Date ──────────────────────────────────────────────────────────────────────
static esp_err_t h_get_date_display_type(httpd_req_t *r) { SEND_INT(r, g_config.date_display_type); }
static esp_err_t h_get_date_color_settings(httpd_req_t *r) { SEND_INT(r, g_config.date_color_settings); }
static esp_err_t h_get_day_color(httpd_req_t *r)   { SEND_HEX(r,g_config.r[4],g_config.g[4],g_config.b[4]); }
static esp_err_t h_get_month_color(httpd_req_t *r) { SEND_HEX(r,g_config.r[5],g_config.g[5],g_config.b[5]); }
static esp_err_t h_get_sep_color(httpd_req_t *r)   { SEND_HEX(r,g_config.r[6],g_config.g[6],g_config.b[6]); }

static esp_err_t h_update_date_display_type(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"dateDisplayType",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.date_display_type=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_day_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"dayColor",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[4],&g_config.g[4],&g_config.b[4]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_month_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"monthColor",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[5],&g_config.g[5],&g_config.b[5]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_sep_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"separatorColor",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[6],&g_config.g[6],&g_config.b[6]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_date_color_settings(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"DateColorSettings",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.date_color_settings=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

// ── Temperature ───────────────────────────────────────────────────────────────
static esp_err_t h_get_temp_symbol(httpd_req_t *r)       { SEND_INT(r, g_config.temperature_symbol); }
static esp_err_t h_get_temp_correction(httpd_req_t *r)   { SEND_INT(r, g_config.temperature_correction); }
static esp_err_t h_get_temp_display_type(httpd_req_t *r) { SEND_INT(r, g_config.temp_display_type); }
static esp_err_t h_get_temp_color_settings(httpd_req_t *r) { SEND_INT(r, g_config.temp_color_settings); }
static esp_err_t h_get_temp_color(httpd_req_t *r)   { SEND_HEX(r,g_config.r[7],g_config.g[7],g_config.b[7]); }
static esp_err_t h_get_type_color(httpd_req_t *r)   { SEND_HEX(r,g_config.r[8],g_config.g[8],g_config.b[8]); }
static esp_err_t h_get_degree_color(httpd_req_t *r) { SEND_HEX(r,g_config.r[9],g_config.g[9],g_config.b[9]); }

static esp_err_t h_update_temp_type(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"temperatureSymbol",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.temperature_symbol=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_temp_correction(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"temperatureCorrection",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.temperature_correction=(int8_t)atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_temp_display_type(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"tempDisplayType",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.temp_display_type=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_temp_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"tempColor",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[7],&g_config.g[7],&g_config.b[7]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_type_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"typeColor",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[8],&g_config.g[8],&g_config.b[8]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_degree_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"degreeColor",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[9],&g_config.g[9],&g_config.b[9]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_temp_color_settings(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"tempColorSettings",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.temp_color_settings=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

// ── Humidity ──────────────────────────────────────────────────────────────────
static esp_err_t h_get_humi_display_type(httpd_req_t *r)    { SEND_INT(r, g_config.humi_display_type); }
static esp_err_t h_get_humi_color_settings(httpd_req_t *r)  { SEND_INT(r, g_config.humi_color_settings); }
static esp_err_t h_get_humi_color(httpd_req_t *r)         { SEND_HEX(r,g_config.r[10],g_config.g[10],g_config.b[10]); }
static esp_err_t h_get_symbol_color(httpd_req_t *r)       { SEND_HEX(r,g_config.r[11],g_config.g[11],g_config.b[11]); }
static esp_err_t h_get_humi_decimal_color(httpd_req_t *r) { SEND_HEX(r,g_config.r[12],g_config.g[12],g_config.b[12]); }

static esp_err_t h_update_humi_display_type(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"humiDisplayType",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.humi_display_type=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_humi_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"humiColor",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[10],&g_config.g[10],&g_config.b[10]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_symbol_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"symbolColor",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[11],&g_config.g[11],&g_config.b[11]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_humi_decimal_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"humiDecimalColor",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[12],&g_config.g[12],&g_config.b[12]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_humi_color_settings(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"humiColorSettings",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.humi_color_settings=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

void register_handlers_date_temp_humi(httpd_handle_t s) {
    REG(s, HTTP_GET,  "/getDateDisplayType",    h_get_date_display_type);
    REG(s, HTTP_GET,  "/getDateColorSettings",  h_get_date_color_settings);
    REG(s, HTTP_GET,  "/getdayColor",           h_get_day_color);
    REG(s, HTTP_GET,  "/getmonthColor",         h_get_month_color);
    REG(s, HTTP_GET,  "/getseparatorColor",     h_get_sep_color);
    REG(s, HTTP_POST, "/updateDateDisplayType", h_update_date_display_type);
    REG(s, HTTP_POST, "/updatedayColor",        h_update_day_color);
    REG(s, HTTP_POST, "/updatemonthColor",      h_update_month_color);
    REG(s, HTTP_POST, "/updateseparatorColor",  h_update_sep_color);
    REG(s, HTTP_POST, "/updateDateColorSettings", h_update_date_color_settings);
    REG(s, HTTP_GET,  "/gettemperatureSymbol",     h_get_temp_symbol);
    REG(s, HTTP_GET,  "/gettemperatureCorrection", h_get_temp_correction);
    REG(s, HTTP_GET,  "/gettempDisplayType",       h_get_temp_display_type);
    REG(s, HTTP_GET,  "/gettempColorSettings",     h_get_temp_color_settings);
    REG(s, HTTP_GET,  "/gettempColor",             h_get_temp_color);
    REG(s, HTTP_GET,  "/gettypeColor",             h_get_type_color);
    REG(s, HTTP_GET,  "/getdegreeColor",           h_get_degree_color);
    REG(s, HTTP_POST, "/updateTempType",           h_update_temp_type);
    REG(s, HTTP_POST, "/updateCorrectionSelect",   h_update_temp_correction);
    REG(s, HTTP_POST, "/updateTempDisplayType",    h_update_temp_display_type);
    REG(s, HTTP_POST, "/updateTempColor",          h_update_temp_color);
    REG(s, HTTP_POST, "/updateTypeColor",          h_update_type_color);
    REG(s, HTTP_POST, "/updateDegreeColor",        h_update_degree_color);
    REG(s, HTTP_POST, "/updateTempColorSettings",  h_update_temp_color_settings);
    REG(s, HTTP_GET,  "/gethumiDisplayType",       h_get_humi_display_type);
    REG(s, HTTP_GET,  "/gethumiColorSettings",     h_get_humi_color_settings);
    REG(s, HTTP_GET,  "/gethumiColor",             h_get_humi_color);
    REG(s, HTTP_GET,  "/getsymbolColor",           h_get_symbol_color);
    REG(s, HTTP_GET,  "/gethumiDecimalColor",      h_get_humi_decimal_color);
    REG(s, HTTP_POST, "/updateHumiDisplayType",    h_update_humi_display_type);
    REG(s, HTTP_POST, "/updateHumiColor",          h_update_humi_color);
    REG(s, HTTP_POST, "/updateSymbolColor",        h_update_symbol_color);
    REG(s, HTTP_POST, "/updateHumiDecimalColor",   h_update_humi_decimal_color);
    REG(s, HTTP_POST, "/updateHumiColorSettings",  h_update_humi_color_settings);
}
```

- [ ] **Schritt 3: handlers_scores_cd_ls.c schreiben**

`components/web_server/handlers_scores_cd_ls.c` — Scoreboard, Countdown, Lightshow:

```c
#include "web_server.h"
#include "storage.h"
#include <stdlib.h>
#include <string.h>

#define SEND_OK(req)  do { httpd_resp_set_type(req,"text/json"); \
    httpd_resp_sendstr(req,"{\"result\":\"ok\"}"); return ESP_OK; } while(0)
#define SEND_INT(req, v) do { char _b[16]; snprintf(_b,sizeof(_b),"%d",(int)(v)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_b); return ESP_OK; } while(0)
#define SEND_HEX(req, r, g, b) do { char _c[8]; \
    snprintf(_c,sizeof(_c),"#%02X%02X%02X",(r),(g),(b)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_c); return ESP_OK; } while(0)
#define SEND_RGBA(req, r, g, b) do { char _c[32]; \
    snprintf(_c,sizeof(_c),"rgba(%d,%d,%d,1)",(r),(g),(b)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_c); return ESP_OK; } while(0)
#define REG(s,m,u,h) do { httpd_uri_t _u={.uri=(u),.method=(m),.handler=(h)}; \
    httpd_register_uri_handler((s),&_u); } while(0)

// Scoreboard r[13]=left, r[14]=right
static esp_err_t h_get_sb_left(httpd_req_t *r)      { SEND_HEX(r,g_config.r[13],g_config.g[13],g_config.b[13]); }
static esp_err_t h_get_sb_right(httpd_req_t *r)     { SEND_HEX(r,g_config.r[14],g_config.g[14],g_config.b[14]); }
static esp_err_t h_get_sb_left_rgb(httpd_req_t *r)  { SEND_RGBA(r,g_config.r[13],g_config.g[13],g_config.b[13]); }
static esp_err_t h_get_sb_right_rgb(httpd_req_t *r) { SEND_RGBA(r,g_config.r[14],g_config.g[14],g_config.b[14]); }

static esp_err_t h_update_sb_left(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"scoreboardColorLeft",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[13],&g_config.g[13],&g_config.b[13]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_sb_right(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"scoreboardColorRight",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[14],&g_config.g[14],&g_config.b[14]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

// Countdown: cd_r/g/b, colorchangeCD, alarmCD
static esp_err_t h_get_cd_color(httpd_req_t *r)          { SEND_HEX(r,g_config.cd_r,g_config.cd_g,g_config.cd_b); }
static esp_err_t h_get_cd_colorchange(httpd_req_t *r)    { SEND_INT(r, g_config.color_change_cd); }
static esp_err_t h_get_cd_alarm(httpd_req_t *r)          { SEND_INT(r, g_config.use_audible_alarm); }

static esp_err_t h_update_cd_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"colorCD",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.cd_r,&g_config.cd_g,&g_config.cd_b);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_cd_colorchange(httpd_req_t *r) {
    char buf[4]={0}; get_body_param(r,"colorchangeCD",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.color_change_cd = atoi(buf) ? 1 : 0;
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_cd_alarm(httpd_req_t *r) {
    char buf[4]={0}; get_body_param(r,"alarmCD",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.use_audible_alarm = atoi(buf) ? 1 : 0;
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

void register_handlers_scores_cd_ls(httpd_handle_t s) {
    REG(s, HTTP_GET,  "/getscoreboardColorLeft",     h_get_sb_left);
    REG(s, HTTP_GET,  "/getscoreboardColorRight",    h_get_sb_right);
    REG(s, HTTP_GET,  "/getscoreboardColorLeftRGB",  h_get_sb_left_rgb);
    REG(s, HTTP_GET,  "/getscoreboardColorRightRGB", h_get_sb_right_rgb);
    REG(s, HTTP_POST, "/updatescoreboardColorLeft",  h_update_sb_left);
    REG(s, HTTP_POST, "/updatescoreboardColorRight", h_update_sb_right);
    REG(s, HTTP_GET,  "/getcolorCD",           h_get_cd_color);
    REG(s, HTTP_GET,  "/getcolorchangeCD",     h_get_cd_colorchange);
    REG(s, HTTP_GET,  "/getalarmCD",           h_get_cd_alarm);
    REG(s, HTTP_POST, "/updatecolorCD",        h_update_cd_color);
    REG(s, HTTP_POST, "/updatecolorchangeCD",  h_update_cd_colorchange);
    REG(s, HTTP_POST, "/updatealarmCD",        h_update_cd_alarm);
}
```

- [ ] **Schritt 4: handlers_scroll_spec.c schreiben**

`components/web_server/handlers_scroll_spec.c`:

```c
#include "web_server.h"
#include "storage.h"
#include <stdlib.h>
#include <string.h>

#define SEND_OK(req)  do { httpd_resp_set_type(req,"text/json"); \
    httpd_resp_sendstr(req,"{\"result\":\"ok\"}"); return ESP_OK; } while(0)
#define SEND_INT(req, v) do { char _b[16]; snprintf(_b,sizeof(_b),"%d",(int)(v)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_b); return ESP_OK; } while(0)
#define SEND_HEX(req, r, g, b) do { char _c[8]; \
    snprintf(_c,sizeof(_c),"#%02X%02X%02X",(r),(g),(b)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_c); return ESP_OK; } while(0)
#define REG(s,m,u,h) do { httpd_uri_t _u={.uri=(u),.method=(m),.handler=(h)}; \
    httpd_register_uri_handler((s),&_u); } while(0)

// ── Scroll ────────────────────────────────────────────────────────────────────
static esp_err_t h_get_scroll_freq(httpd_req_t *r)     { SEND_INT(r,g_config.scroll_frequency); }
static esp_err_t h_get_scroll_override(httpd_req_t *r) { SEND_INT(r,g_config.scroll_override); }
static esp_err_t h_get_scroll_color(httpd_req_t *r)    { SEND_HEX(r,g_config.r[16],g_config.g[16],g_config.b[16]); }
static esp_err_t h_get_scroll_color_set(httpd_req_t *r){ SEND_INT(r,g_config.scroll_color_settings); }
static esp_err_t h_get_scroll_text(httpd_req_t *r) {
    httpd_resp_set_type(r,"text/plain");
    httpd_resp_sendstr(r, g_config.scroll_text);
    return ESP_OK;
}

static esp_err_t h_get_scroll_opt(httpd_req_t *r) {
    // URI ends with digit 1-8
    int idx = r->uri[strlen(r->uri)-1] - '1';
    if (idx < 0 || idx > 7) { httpd_resp_send_404(r); return ESP_FAIL; }
    SEND_INT(r, g_config.scroll_options[idx]);
}

static esp_err_t h_update_scroll_freq(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"scrollFrequency",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.scroll_frequency=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_scroll_override(httpd_req_t *r) {
    char buf[4]={0}; get_body_param(r,"scrollOverride",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.scroll_override=atoi(buf)?1:0;
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_scroll_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"scrollColor",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[16],&g_config.g[16],&g_config.b[16]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_scroll_color_set(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"scrollColorSettings",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.scroll_color_settings=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_scroll_opt(httpd_req_t *r) {
    int idx = r->uri[strlen(r->uri)-1] - '1';
    if (idx < 0 || idx > 7) { httpd_resp_send_404(r); return ESP_FAIL; }
    char buf[4]={0};
    char key[16]; snprintf(key,sizeof(key),"scrollOptions%d",idx+1);
    get_body_param(r,key,buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.scroll_options[idx]=atoi(buf)?1:0;
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_scroll_text(httpd_req_t *r) {
    char buf[257]={0}; get_body_param(r,"scrollText",buf,sizeof(buf));
    if (buf[0]=='\0') strncpy(buf,"dAdS ArE tHE bESt",sizeof(buf)-1);
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    strncpy(g_config.scroll_text,buf,sizeof(g_config.scroll_text)-1);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

// ── Spectrum ──────────────────────────────────────────────────────────────────
static esp_err_t h_get_rand_spec(httpd_req_t *r)     { SEND_INT(r,g_config.random_spectrum_mode); }
static esp_err_t h_get_spec_color(httpd_req_t *r)    { SEND_HEX(r,g_config.r[15],g_config.g[15],g_config.b[15]); }
static esp_err_t h_get_spec_bg(httpd_req_t *r)       { SEND_HEX(r,g_config.r[17],g_config.g[17],g_config.b[17]); }
static esp_err_t h_get_spec_color_set(httpd_req_t *r){ SEND_INT(r,g_config.spectrum_color_settings); }
static esp_err_t h_get_spec_bg_set(httpd_req_t *r)   { SEND_INT(r,g_config.spectrum_background_settings); }

static esp_err_t h_update_rand_spec(httpd_req_t *r) {
    char buf[4]={0}; get_body_param(r,"randomSpectrumMode",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.random_spectrum_mode=atoi(buf)?1:0;
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_spec_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"spectrumColor",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[15],&g_config.g[15],&g_config.b[15]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_spec_bg(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"spectrumBackground",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[17],&g_config.g[17],&g_config.b[17]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_spec_color_set(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"spectrumColorSettings",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.spectrum_color_settings=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_spec_bg_set(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"spectrumBackgroundSettings",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.spectrum_background_settings=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

void register_handlers_scroll_spec(httpd_handle_t s) {
    REG(s,HTTP_GET, "/getscrollFrequency",      h_get_scroll_freq);
    REG(s,HTTP_GET, "/getscrollOverride",        h_get_scroll_override);
    REG(s,HTTP_GET, "/getscrollColor",           h_get_scroll_color);
    REG(s,HTTP_GET, "/getscrollColorSettings",   h_get_scroll_color_set);
    REG(s,HTTP_GET, "/getscrollText",            h_get_scroll_text);
    REG(s,HTTP_GET, "/getscrollOptions1",        h_get_scroll_opt);
    REG(s,HTTP_GET, "/getscrollOptions2",        h_get_scroll_opt);
    REG(s,HTTP_GET, "/getscrollOptions3",        h_get_scroll_opt);
    REG(s,HTTP_GET, "/getscrollOptions4",        h_get_scroll_opt);
    REG(s,HTTP_GET, "/getscrollOptions5",        h_get_scroll_opt);
    REG(s,HTTP_GET, "/getscrollOptions6",        h_get_scroll_opt);
    REG(s,HTTP_GET, "/getscrollOptions7",        h_get_scroll_opt);
    REG(s,HTTP_GET, "/getscrollOptions8",        h_get_scroll_opt);
    REG(s,HTTP_POST,"/updatescrollFrequency",    h_update_scroll_freq);
    REG(s,HTTP_POST,"/updatescrollOverride",     h_update_scroll_override);
    REG(s,HTTP_POST,"/updatescrollColor",        h_update_scroll_color);
    REG(s,HTTP_POST,"/updatescrollColorSettings",h_update_scroll_color_set);
    REG(s,HTTP_POST,"/updatescrollText",         h_update_scroll_text);
    REG(s,HTTP_POST,"/updatescrollOptions1",     h_update_scroll_opt);
    REG(s,HTTP_POST,"/updatescrollOptions2",     h_update_scroll_opt);
    REG(s,HTTP_POST,"/updatescrollOptions3",     h_update_scroll_opt);
    REG(s,HTTP_POST,"/updatescrollOptions4",     h_update_scroll_opt);
    REG(s,HTTP_POST,"/updatescrollOptions5",     h_update_scroll_opt);
    REG(s,HTTP_POST,"/updatescrollOptions6",     h_update_scroll_opt);
    REG(s,HTTP_POST,"/updatescrollOptions7",     h_update_scroll_opt);
    REG(s,HTTP_POST,"/updatescrollOptions8",     h_update_scroll_opt);
    REG(s,HTTP_GET, "/getrandomSpectrumMode",          h_get_rand_spec);
    REG(s,HTTP_GET, "/getspectrumColor",               h_get_spec_color);
    REG(s,HTTP_GET, "/getspectrumBackground",          h_get_spec_bg);
    REG(s,HTTP_GET, "/getspectrumColorSettings",       h_get_spec_color_set);
    REG(s,HTTP_GET, "/getspectrumBackgroundSettings",  h_get_spec_bg_set);
    REG(s,HTTP_POST,"/updaterandomSpectrumMode",       h_update_rand_spec);
    REG(s,HTTP_POST,"/updatespectrumColor",            h_update_spec_color);
    REG(s,HTTP_POST,"/updatespectrumBackground",       h_update_spec_bg);
    REG(s,HTTP_POST,"/updatespectrumColorSettings",    h_update_spec_color_set);
    REG(s,HTTP_POST,"/updatespectrumBackgroundSettings", h_update_spec_bg_set);
}
```

- [ ] **Schritt 5: handlers_system.c schreiben**

`components/web_server/handlers_system.c` — Brightness, Spotlights, Pastel, ColorChangeFreq, SuspendType/Freq, Debug-Seite:

```c
#include "web_server.h"
#include "storage.h"
#include "sensors.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SEND_OK(req)  do { httpd_resp_set_type(req,"text/json"); \
    httpd_resp_sendstr(req,"{\"result\":\"ok\"}"); return ESP_OK; } while(0)
#define SEND_INT(req, v) do { char _b[16]; snprintf(_b,sizeof(_b),"%d",(int)(v)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_b); return ESP_OK; } while(0)
#define SEND_HEX(req, r, g, b) do { char _c[8]; \
    snprintf(_c,sizeof(_c),"#%02X%02X%02X",(r),(g),(b)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_c); return ESP_OK; } while(0)
#define REG(s,m,u,h) do { httpd_uri_t _u={.uri=(u),.method=(m),.handler=(h)}; \
    httpd_register_uri_handler((s),&_u); } while(0)

static esp_err_t h_get_brightness(httpd_req_t *r)     { SEND_INT(r,g_config.brightness); }
static esp_err_t h_get_spotlights_color(httpd_req_t *r){ SEND_HEX(r,g_config.r[0],g_config.g[0],g_config.b[0]); }
static esp_err_t h_get_spotlights_set(httpd_req_t *r)  { SEND_INT(r,g_config.spotlights_color_settings); }
static esp_err_t h_get_use_spotlights(httpd_req_t *r)  { SEND_INT(r,g_config.use_spotlights); }
static esp_err_t h_get_pastel(httpd_req_t *r)          { SEND_INT(r,g_config.pastel_colors); }
static esp_err_t h_get_ccfreq(httpd_req_t *r)          { SEND_INT(r,g_config.color_change_frequency); }
static esp_err_t h_get_suspend_type(httpd_req_t *r)    { SEND_INT(r,g_config.suspend_type); }
static esp_err_t h_get_suspend_freq(httpd_req_t *r)    { SEND_INT(r,g_config.suspend_frequency); }

static esp_err_t h_update_brightness(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"rangeBrightness",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.brightness=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_spotlights_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"spotlightsColor",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[0],&g_config.g[0],&g_config.b[0]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_spotlights_set(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"spotlightsColorSettings",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.spotlights_color_settings=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_use_spotlights(httpd_req_t *r) {
    char buf[4]={0}; get_body_param(r,"useSpotlights",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.use_spotlights=atoi(buf)?1:0;
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_pastel(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"pastelColors",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.pastel_colors=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_ccfreq(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"ColorChangeFrequency",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.color_change_frequency=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_suspend_type(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"suspendType",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.suspend_type=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_suspend_freq(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"suspendFrequency",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.suspend_frequency=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

static esp_err_t h_debugpage(httpd_req_t *r) {
    char buf[1024];
    time_t now = time(NULL);
    struct tm ti; localtime_r(&now, &ti);
    struct tm rtci; ds3231_get_time(&rtci);
    snprintf(buf, sizeof(buf),
        "<html><body><pre>"
        "ESP32 Time: %04d-%02d-%02d %02d:%02d:%02d\n"
        "DS3231 Time: %04d-%02d-%02d %02d:%02d:%02d\n"
        "Temp: %.1f C / %.1f F  Humi: %.1f%%\n"
        "Light: %d  Audio: %d\n"
        "Mode: %d  Brightness: %d\n"
        "</pre></body></html>",
        ti.tm_year+1900, ti.tm_mon+1, ti.tm_mday,
        ti.tm_hour, ti.tm_min, ti.tm_sec,
        rtci.tm_year+1900, rtci.tm_mon+1, rtci.tm_mday,
        rtci.tm_hour, rtci.tm_min, rtci.tm_sec,
        g_sensors.temperature_c, g_sensors.temperature_f, g_sensors.humidity,
        (int)g_sensors.light_level, (int)g_sensors.audio_level,
        (int)g_config.clock_mode, (int)g_config.brightness);
    httpd_resp_set_type(r, "text/html");
    httpd_resp_sendstr(r, buf);
    return ESP_OK;
}

void register_handlers_system(httpd_handle_t s) {
    REG(s,HTTP_GET, "/getrangeBrightness",          h_get_brightness);
    REG(s,HTTP_GET, "/getspotlightsColor",          h_get_spotlights_color);
    REG(s,HTTP_GET, "/getspotlightsColorSettings",  h_get_spotlights_set);
    REG(s,HTTP_GET, "/getuseSpotlights",            h_get_use_spotlights);
    REG(s,HTTP_GET, "/getpastelColors",             h_get_pastel);
    REG(s,HTTP_GET, "/getColorChangeFrequency",     h_get_ccfreq);
    REG(s,HTTP_GET, "/getsuspendType",              h_get_suspend_type);
    REG(s,HTTP_GET, "/getsuspendFrequency",         h_get_suspend_freq);
    REG(s,HTTP_POST,"/updaterangeBrightness",       h_update_brightness);
    REG(s,HTTP_POST,"/updatespotlightsColor",       h_update_spotlights_color);
    REG(s,HTTP_POST,"/updatespotlightsColorSettings", h_update_spotlights_set);
    REG(s,HTTP_POST,"/updateuseSpotlights",         h_update_use_spotlights);
    REG(s,HTTP_POST,"/updatePastelColors",          h_update_pastel);
    REG(s,HTTP_POST,"/updateColorChangeFrequency",  h_update_ccfreq);
    REG(s,HTTP_POST,"/updatesuspendType",           h_update_suspend_type);
    REG(s,HTTP_POST,"/updatesuspendFrequency",      h_update_suspend_freq);
    REG(s,HTTP_GET, "/debugpage",                   h_debugpage);
}
```

- [ ] **Schritt 6: Build verifizieren**

```bash
idf.py build
```

- [ ] **Schritt 7: Commit**

```bash
git add components/web_server/
git commit -m "feat: add all web_server handler files (~90 endpoints)"
```

---

## Task 9: Clock-Modi-Komponente

**Dateien:**
- Erstellen: `components/clock_modes/include/clock_modes.h`
- Erstellen: `components/clock_modes/mode_time.c`
- Erstellen: `components/clock_modes/mode_date.c`
- Erstellen: `components/clock_modes/mode_temperature.c`
- Erstellen: `components/clock_modes/mode_humidity.c`
- Erstellen: `components/clock_modes/mode_scoreboard.c`
- Erstellen: `components/clock_modes/mode_countdown.c`
- Erstellen: `components/clock_modes/mode_stopwatch.c`
- Erstellen: `components/clock_modes/mode_scroll.c`

- [ ] **Schritt 1: Header schreiben**

`components/clock_modes/include/clock_modes.h`:
```c
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Countdown/Stopwatch Zeitstempel (esp_timer_get_time() in µs)
extern int64_t g_countdown_end_us;
extern int64_t g_countup_start_us;
extern int64_t g_countup_end_us;

// Geteilte Flags für Farb-Änderungs-Frequenz (gesetzt/gelöscht in main_task)
extern bool g_flag_min;
extern bool g_flag_hour;
extern bool g_flag_day;
extern bool g_flag_week;
extern bool g_flag_month;

void mode_time_update(void);
void mode_date_update(void);
void mode_temperature_update(void);
void mode_humidity_update(void);
void mode_scoreboard_update(void);
void mode_countdown_update(void);
void mode_stopwatch_update(void);
void mode_scroll_update(void);        // standalone-Scroll-Modus (clockMode==11)
void mode_scroll_overlay(void);       // periodischer Overlay-Scroll

void mode_countdown_start(int32_t duration_ms);
void mode_stopwatch_start(int32_t duration_ms);

void scroll(const char *text);        // direkt aufrufbar aus main_task (z.B. "MAkE A WISH")
void end_countdown(void);
```

- [ ] **Schritt 2: mode_time.c schreiben**

`components/clock_modes/mode_time.c`:
```c
#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"
#include "rtttl_player.h"
#include "esp_timer.h"
#include "esp_random.h"
#include <time.h>
#include <math.h>

static bool s_dots_on = true;
static int  s_prev_sec = -1;

static crgb_t pick_color(uint8_t cs, int ridx, bool flag) {
    bool pastel = g_config.pastel_colors;
    uint8_t ccf = g_config.color_change_frequency;
    bool trigger = (ccf == 0) ||
                   (ccf == 1 && g_flag_min)   || (ccf == 2 && g_flag_hour) ||
                   (ccf == 3 && g_flag_day)   || (ccf == 4 && g_flag_week) ||
                   (ccf == 5 && g_flag_month);
    if (cs == 0 || cs == 1) return CRGB(g_config.r[ridx], g_config.g[ridx], g_config.b[ridx]);
    if ((cs == 2 || cs == 3) && trigger) return random_color(pastel);
    return CRGB(g_config.r[ridx], g_config.g[ridx], g_config.b[ridx]);
}

void mode_time_update(void) {
    time_t now = time(NULL);
    struct tm ti; localtime_r(&now, &ti);
    int hour = ti.tm_hour, mins = ti.tm_min, secs = ti.tm_sec;

    // New-Year-Countdown (clockDisplayType==4)
    if (g_config.clock_display_type == 4) {
        int mday = ti.tm_mday, mont = ti.tm_mon + 1, year = ti.tm_year + 1900;
        int days_in_month[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        int days_left = 0;
        bool leap = ((year%4==0&&year%100!=0)||(year%400==0));
        if (leap) days_left++;
        if (mont != 12) for (int i=mont+1;i<13;i++) days_left += days_in_month[i-1];
        days_left += days_in_month[mont-1] - mday;
        int hours_left = days_left*24 + (23 - hour);
        // DST adjustment
        int y = year-2000, x = (y + y/4 + 2) % 7;
        bool dst = false;
        if (mont == 3  && mday == (14-x) && hour >= 2) dst = true;
        if ((mont == 3 && mday >  (14-x)) || mont > 3) dst = true;
        if (mont == 11 && mday == (7-x)  && hour >= 2) dst = false;
        if ((mont == 11 && mday > (7-x)) || mont > 11 || mont < 3) dst = false;
        if (dst) hours_left++;
        int mins_left  = hours_left*60 + (59 - mins);
        int secs_left  = mins_left*60  + (60 - secs);
        int display    = hours_left;
        crgb_t col = CRGB(g_config.r[1], g_config.g[1], g_config.b[1]);
        if (mins_left  <= 9999 && secs_left > 9999) { display = mins_left;  col = CRGB(g_config.r[2],g_config.g[2],g_config.b[2]); }
        if (secs_left  <= 9999 && secs_left > 10)   { display = secs_left;  col = CRGB(g_config.r[3],g_config.g[3],g_config.b[3]); }
        if (secs_left  <= 10)                        { display = secs_left; }

        uint8_t n1=display/1000, n2=(display%1000)/100, n3=(display%100)/10, n4=display%10;
        all_blank();
        if (display >= 1000) {
            display_number(n1,6,col); display_number(n2,4,col);
            display_number(n3,2,col); display_number(n4,0,col);
        } else if (display >= 100) {
            display_number(n2,5,col); display_number(n3,3,col); display_number(n4,1,col);
        } else {
            display_number(n3,4,col); display_number(n4,2,col);
        }
        if (mday==1 && mont==1 && hour==0 && mins==0 && secs<=3) scroll("hAPPy nEW yEAr");
        return;
    }

    // Blinking dots für clockDisplayType 0 und 3
    if (secs != s_prev_sec) { s_dots_on = !s_dots_on; s_prev_sec = secs; }

    uint8_t cs = g_config.clock_color_settings;
    crgb_t hc = pick_color(cs, 1, true);
    crgb_t mc = (cs == 1 || cs == 3) ? hc : pick_color(cs, 2, true);

    int disp_hour = hour;
    if (g_config.clock_display_type != 1) {
        if (disp_hour > 12) disp_hour -= 12;
        if (disp_hour < 1)  disp_hour += 12;
    }
    uint8_t h1 = disp_hour / 10, h2 = disp_hour % 10;
    uint8_t m1 = mins / 10,      m2 = mins % 10;

    // clockDisplayType: 0=center, 1=24h military, 2=space-padded, 3=blink-center
    if (g_config.clock_display_type == 1) {
        // 24-hour: always show h1 (zero-pad)
        display_number(h1 < 1 ? 0 : h1, 6, hc);
        display_number(h2, 4, hc);
        display_number(m1, 2, mc);
        display_number(m2, 0, mc);
    } else if (g_config.clock_display_type == 2) {
        // Space-padded 12h
        display_number(h1 < 1 ? 10 : h1, 6, hc);
        display_number(h2, 4, hc);
        display_number(m1, 2, mc);
        display_number(m2, 0, mc);
    } else {
        // 0 or 3: center-padded, optional blink
        if (h1 > 0) {
            crgb_t th = (cs==4) ? random_color(g_config.pastel_colors) : hc;
            for (int i=32*LEDS_PER_SEGMENT; i<33*LEDS_PER_SEGMENT; i++) if(i<NUM_LEDS) g_leds[i]=th;
            th = (cs==4) ? random_color(g_config.pastel_colors) : hc;
            for (int i=33*LEDS_PER_SEGMENT; i<34*LEDS_PER_SEGMENT; i++) if(i<NUM_LEDS) g_leds[i]=th;
        } else {
            for (int i=32*LEDS_PER_SEGMENT; i<34*LEDS_PER_SEGMENT; i++) if(i<NUM_LEDS) g_leds[i]=CRGB_BLACK;
        }
        display_number(h2, 5, hc);
        display_number(m1, 3, mc);
        display_number(m2, 1, mc);
        if (g_config.clock_display_type == 3 || g_config.clock_display_type == 0) {
            blink_dots(&s_dots_on);
        }
    }
}
```

- [ ] **Schritt 3: mode_date.c schreiben**

`components/clock_modes/mode_date.c`:
```c
#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"
#include "esp_random.h"
#include <time.h>
#include <string.h>

static crgb_t pick(uint8_t cs, int ri) {
    bool pastel = g_config.pastel_colors;
    uint8_t ccf = g_config.color_change_frequency;
    bool trigger = (ccf==0)||(ccf==1&&g_flag_min)||(ccf==2&&g_flag_hour)||
                   (ccf==3&&g_flag_day)||(ccf==4&&g_flag_week)||(ccf==5&&g_flag_month);
    if (cs==0||cs==1) return CRGB(g_config.r[ri],g_config.g[ri],g_config.b[ri]);
    if ((cs==2||cs==3) && trigger) return random_color(pastel);
    return CRGB(g_config.r[ri],g_config.g[ri],g_config.b[ri]);
}

void mode_date_update(void) {
    time_t now = time(NULL);
    struct tm ti; localtime_r(&now, &ti);
    int mday=ti.tm_mday, mont=ti.tm_mon+1, year=(ti.tm_year+1900)-2000;
    uint8_t d1=mday/10, d2=mday%10, m1=mont/10, m2=mont%10, y1=year/10, y2=year%10;
    uint8_t cs = g_config.date_color_settings;
    crgb_t dc = pick(cs, 4);   // day
    crgb_t mc = (cs==1||cs==3) ? dc : pick(cs, 5); // month
    crgb_t sc = (cs==3) ? dc : pick(cs, 6);          // separator

    switch (g_config.date_display_type) {
    case 0: // Zero-padded MMDD
        display_number(m1<1?0:m1, 6, mc);
        display_number(m2, 4, mc); display_number(d1, 2, dc); display_number(d2, 0, dc);
        break;
    case 1: // Space-padded MMDD
        display_number(m1<1?10:m1, 6, mc);
        display_number(m2, 4, mc); display_number(d1, 2, dc); display_number(d2, 0, dc);
        break;
    case 2: // Center 1MDD
        if (m1>0) {
            crgb_t tm = (cs==4)?random_color(g_config.pastel_colors):mc;
            for(int i=32*LEDS_PER_SEGMENT;i<33*LEDS_PER_SEGMENT;i++) if(i<NUM_LEDS) g_leds[i]=tm;
            tm = (cs==4)?random_color(g_config.pastel_colors):mc;
            for(int i=33*LEDS_PER_SEGMENT;i<34*LEDS_PER_SEGMENT;i++) if(i<NUM_LEDS) g_leds[i]=tm;
        } else {
            for(int i=32*LEDS_PER_SEGMENT;i<34*LEDS_PER_SEGMENT;i++) if(i<NUM_LEDS) g_leds[i]=CRGB_BLACK;
        }
        display_number(m2,5,mc); display_number(d1,3,dc); display_number(d2,1,dc);
        break;
    case 3: // Day-of-week
        switch(ti.tm_wday) {
            case 1: display_number(78,5,dc);display_number(80,3,dc);display_number(79,1,dc); break; // mon
            case 2: display_number(85,6,dc);display_number(54,4,dc);display_number(38,2,dc);display_number(52,0,dc); break; // tUES
            case 3: display_number(88,5,dc);display_number(38,3,dc);display_number(69,1,dc); break; // wEd
            case 4: display_number(85,6,dc);display_number(73,4,dc);display_number(86,2,dc);display_number(83,0,dc); break; // thur
            case 5: display_number(39,5,dc);display_number(83,3,dc);display_number(42,1,dc); break; // FrI
            case 6: display_number(52,5,dc);display_number(34,3,dc);display_number(85,1,dc); break; // SAt
            case 0: display_number(52,5,dc);display_number(86,3,dc);display_number(79,1,dc); break; // Sun
        }
        break;
    case 4: // Just day DD
        if (d1<1) display_number(d2,3,dc);
        else { display_number(d1,4,dc); display_number(d2,2,dc); }
        break;
    case 5: // MM.DD with separator
        if (m1>0) {
            crgb_t tm=(cs==4)?random_color(g_config.pastel_colors):mc;
            for(int i=32*LEDS_PER_SEGMENT;i<33*LEDS_PER_SEGMENT;i++) if(i<NUM_LEDS) g_leds[i]=tm;
            tm=(cs==4)?random_color(g_config.pastel_colors):mc;
            for(int i=33*LEDS_PER_SEGMENT;i<34*LEDS_PER_SEGMENT;i++) if(i<NUM_LEDS) g_leds[i]=tm;
        } else {
            for(int i=32*LEDS_PER_SEGMENT;i<34*LEDS_PER_SEGMENT;i++) if(i<NUM_LEDS) g_leds[i]=CRGB_BLACK;
        }
        display_number(m2,5,mc); display_number(d1,2,dc); display_number(d2,0,dc);
        for(int i=20*LEDS_PER_SEGMENT;i<21*LEDS_PER_SEGMENT;i++) if(i<NUM_LEDS) g_leds[i]=sc;
        break;
    case 6: // Just year 20YY
        display_number(2,6,mc); display_number(0,4,mc);
        display_number(y1,2,dc); display_number(y2,0,dc);
        break;
    }
}
```

- [ ] **Schritt 4: mode_temperature.c schreiben**

`components/clock_modes/mode_temperature.c`:
```c
#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"
#include "sensors.h"
#include "esp_random.h"
#include <math.h>

void mode_temperature_update(void) {
    float temp = g_sensors.temperature_c + g_config.temperature_correction;
    if (g_config.temperature_symbol == 39) {  // Fahrenheit
        temp = (g_sensors.temperature_c * 1.8f + 32.0f) + g_config.temperature_correction;
    }
    uint8_t cs = g_config.temp_color_settings;
    bool pastel = g_config.pastel_colors;
    bool trigger = (g_config.color_change_frequency==0) ||
                   (g_config.color_change_frequency==1&&g_flag_min) ||
                   (g_config.color_change_frequency==2&&g_flag_hour);
    crgb_t tc = (cs>=2&&trigger) ? random_color(pastel) : CRGB(g_config.r[7],g_config.g[7],g_config.b[7]);
    crgb_t vc = (cs==1||(cs==3&&trigger)) ? tc : ((cs>=2&&trigger)?random_color(pastel):CRGB(g_config.r[8],g_config.g[8],g_config.b[8]));
    crgb_t dc = (cs==3&&trigger) ? tc : ((cs>=2&&trigger)?random_color(pastel):CRGB(g_config.r[9],g_config.g[9],g_config.b[9]));

    int temp_dec = (int)(temp * 10);
    uint8_t t1=0, t2=0, t3, t4;
    if (temp >= 100.0f) {
        int th = (int)temp / 10;
        t1 = th / 10; t2 = th % 10;
    } else {
        t2 = (int)temp / 10;
    }
    t3 = (int)temp % 10;
    t4 = temp_dec % 10;

    uint8_t sym = g_config.temperature_symbol; // 36=C, 39=F

    switch (g_config.temp_display_type) {
    case 0: // 79°F (under 100)
        if (temp < 100.0f) {
            display_number(t2,6,tc); display_number(t3,4,tc);
            display_number(26,2,dc); display_number(sym,0,vc);
        } else {
            display_number(t1,6,tc); display_number(t2,4,tc);
            display_number(t3,2,tc); display_number(sym,0,vc);
        }
        break;
    case 1: // 79 F
        if (temp < 100.0f) {
            display_number(t2,5,tc); display_number(t3,3,tc); display_number(sym,1,vc);
        } else {
            display_number(t1,6,tc); display_number(t2,4,tc);
            display_number(t3,2,tc); display_number(sym,0,vc);
        }
        break;
    case 2: // 79°
        if (temp < 100.0f) {
            display_number(t2,5,tc); display_number(t3,3,tc); display_number(26,1,dc);
        } else {
            display_number(t1,6,tc); display_number(t2,4,tc);
            display_number(t3,2,tc); display_number(26,0,dc);
        }
        break;
    case 3: // 79.9
        if (temp < 100.0f) {
            display_number(t2,6,tc); display_number(t3,4,tc); display_number(t4,1,vc);
            crgb_t pc=(cs==4)?random_color(pastel):dc;
            for(int i=12*LEDS_PER_SEGMENT;i<13*LEDS_PER_SEGMENT;i++) if(i<NUM_LEDS) g_leds[i]=pc;
        } else {
            display_number(t2,5,tc); display_number(t3,3,tc); display_number(t4,0,vc);
            crgb_t tth=(cs==4)?random_color(pastel):tc;
            for(int i=32*LEDS_PER_SEGMENT;i<34*LEDS_PER_SEGMENT;i++) if(i<NUM_LEDS) g_leds[i]=tth;
            crgb_t pc=(cs==4)?random_color(pastel):dc;
            for(int i=10*LEDS_PER_SEGMENT;i<11*LEDS_PER_SEGMENT;i++) if(i<NUM_LEDS) g_leds[i]=pc;
        }
        break;
    case 4: // just temp
        if (temp < 100.0f) {
            display_number(t2,4,tc); display_number(t3,2,vc);
        } else {
            display_number(t1,5,dc); display_number(t2,3,tc); display_number(t3,1,vc);
        }
        break;
    }
}
```

- [ ] **Schritt 5: mode_humidity.c schreiben**

`components/clock_modes/mode_humidity.c` — analog zu mode_temperature.c, verwendet `g_sensors.humidity`, Farbindizes r[10..12], Symbol-Zeichen 41 (H), display-Typen 0-2.

```c
#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"
#include "sensors.h"
#include "esp_random.h"

void mode_humidity_update(void) {
    float humi = g_sensors.humidity;
    uint8_t cs = g_config.humi_color_settings;
    bool pastel = g_config.pastel_colors;
    bool trigger = (g_config.color_change_frequency==0) ||
                   (g_config.color_change_frequency==1&&g_flag_min) ||
                   (g_config.color_change_frequency==2&&g_flag_hour);
    crgb_t hc = (cs>=2&&trigger)?random_color(pastel):CRGB(g_config.r[10],g_config.g[10],g_config.b[10]);
    crgb_t sc = (cs==1||(cs==3&&trigger))?hc:((cs>=2&&trigger)?random_color(pastel):CRGB(g_config.r[11],g_config.g[11],g_config.b[11]));
    crgb_t dc = (cs==3&&trigger)?hc:((cs>=2&&trigger)?random_color(pastel):CRGB(g_config.r[12],g_config.g[12],g_config.b[12]));

    int humi_dec = (int)(humi * 10);
    uint8_t t1=0, t2=0;
    if (humi >= 100.0f) { int th=(int)humi/10; t1=th/10; t2=th%10; }
    else { t2=(int)humi/10; }
    uint8_t t3=(int)humi%10, t4=humi_dec%10;

    switch (g_config.humi_display_type) {
    case 0: // 34 H
        if (humi < 100.0f) {
            display_number(t2,5,hc); display_number(t3,3,hc); display_number(41,1,sc);
        } else {
            display_number(t1,6,hc); display_number(t2,4,hc);
            display_number(t3,2,hc); display_number(41,0,sc);
        }
        break;
    case 1: // 34.9
        if (humi < 100.0f) {
            display_number(t2,6,hc); display_number(t3,4,hc); display_number(t4,1,sc);
            crgb_t pc=(cs==4)?random_color(pastel):dc;
            for(int i=12*LEDS_PER_SEGMENT;i<13*LEDS_PER_SEGMENT;i++) if(i<NUM_LEDS) g_leds[i]=pc;
        } else {
            display_number(t2,5,hc); display_number(t3,3,hc); display_number(t4,0,sc);
            crgb_t th=(cs==4)?random_color(pastel):hc;
            for(int i=32*LEDS_PER_SEGMENT;i<34*LEDS_PER_SEGMENT;i++) if(i<NUM_LEDS) g_leds[i]=th;
            crgb_t pc=(cs==4)?random_color(pastel):dc;
            for(int i=10*LEDS_PER_SEGMENT;i<11*LEDS_PER_SEGMENT;i++) if(i<NUM_LEDS) g_leds[i]=pc;
        }
        break;
    case 2: // just humi
        if (humi < 100.0f) { display_number(t2,4,hc); display_number(t3,2,sc); }
        else { display_number(t1,5,dc); display_number(t2,3,hc); display_number(t3,1,sc); }
        break;
    }
}
```

- [ ] **Schritt 6: mode_scoreboard.c schreiben**

`components/clock_modes/mode_scoreboard.c`:
```c
#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"

void mode_scoreboard_update(void) {
    uint8_t sl1 = g_config.scoreboard_left  / 10;
    uint8_t sl2 = g_config.scoreboard_left  % 10;
    uint8_t sr1 = g_config.scoreboard_right / 10;
    uint8_t sr2 = g_config.scoreboard_right % 10;
    crgb_t lc = CRGB(g_config.r[13], g_config.g[13], g_config.b[13]);
    crgb_t rc = CRGB(g_config.r[14], g_config.g[14], g_config.b[14]);
    display_number(sl1,6,lc); display_number(sl2,4,lc);
    display_number(sr1,2,rc); display_number(sr2,0,rc);
}
```

- [ ] **Schritt 7: mode_countdown.c schreiben**

`components/clock_modes/mode_countdown.c`:
```c
#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"
#include "rtttl_player.h"
#include "esp_timer.h"
#include "esp_random.h"
#include <time.h>

int64_t g_countdown_end_us = 0;
static int64_t s_duration_us = 0;

void mode_countdown_start(int32_t duration_ms) {
    s_duration_us       = (int64_t)duration_ms * 1000;
    g_countdown_end_us  = esp_timer_get_time() + s_duration_us;
}

void end_countdown(void) {
    all_blank();
    crgb_t col = CRGB_RED;
    if (g_config.use_audible_alarm) {
        time_t now = time(NULL); struct tm ti; localtime_r(&now, &ti);
        int mday = ti.tm_mday, mont = ti.tm_mon + 1;
        int idx = esp_random() % 11;
        // date-based song overrides
        if (mday==22&&mont==10) idx=RTTTL_SONG_BIRTHDAY;
        else if (mday==25&&mont==12) idx=RTTTL_SONG_XMAS;
        else if (mday==4 &&mont==5)  idx=RTTTL_SONG_STARWARS;
        else if (mday==8 &&mont==9)  idx=9;  // star trek
        rtttl_play_song(idx);
    }
    // flash "End" for 5 seconds
    int cw = 0;
    for (int i = 0; i < 300 && !atomic_load(&g_rtttl_breakout); i++) {
        crgb_t c = color_wheel(cw++ & 0xFF);
        display_number(38,5,c); display_number(79,3,c); display_number(69,1,c);
        led_refresh();
        vTaskDelay(pdMS_TO_TICKS(16));
    }
    scroll("tIMEr Ended      tIMEr Ended");
    all_blank();
    g_countdown_end_us = 0;
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.clock_mode   = 0;
    g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    storage_save_all();
}

void mode_countdown_update(void) {
    if (g_countdown_end_us == 0) return;
    int64_t now_us = esp_timer_get_time();
    if (now_us >= g_countdown_end_us) { end_countdown(); return; }
    int64_t rest_ms = (g_countdown_end_us - now_us) / 1000;
    uint32_t hours   = (uint32_t)((rest_ms / 1000) / 3600);
    uint32_t minutes = (uint32_t)((rest_ms / 1000) / 60);
    uint32_t seconds = (uint32_t)(rest_ms / 1000);
    uint32_t rem_sec = seconds - minutes*60;
    uint32_t rem_min = minutes - hours*60;
    uint8_t h1=hours/10, h2=hours%10, m1=rem_min/10, m2=rem_min%10;
    uint8_t s1=rem_sec/10, s2=rem_sec%10;
    crgb_t col = CRGB(g_config.cd_r, g_config.cd_g, g_config.cd_b);
    if (rest_ms <= 10000 && g_config.color_change_cd) col = CRGB_RED;
    if (hours > 0) {
        display_number(h1,6,col); display_number(h2,4,col);
        display_number(m1,2,col); display_number(m2,0,col);
    } else {
        display_number(m1,6,col); display_number(m2,4,col);
        display_number(s1,2,col); display_number(s2,0,col);
    }
}
```

- [ ] **Schritt 8: mode_stopwatch.c schreiben**

`components/clock_modes/mode_stopwatch.c`:
```c
#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"
#include "esp_timer.h"

int64_t g_countup_start_us = 0;
int64_t g_countup_end_us   = 0;

void mode_stopwatch_start(int32_t duration_ms) {
    g_countup_start_us = esp_timer_get_time();
    g_countup_end_us   = g_countup_start_us + (int64_t)duration_ms * 1000;
}

void mode_stopwatch_update(void) {
    int64_t now_us = esp_timer_get_time();
    if (g_countup_end_us > 0 && now_us >= g_countup_end_us) {
        end_countdown();
        g_countup_start_us = 0;
        g_countup_end_us   = 0;
        return;
    }
    int64_t elapsed_ms = (now_us - g_countup_start_us) / 1000;
    uint32_t hours   = (uint32_t)(elapsed_ms / 3600000);
    uint32_t minutes = (uint32_t)(elapsed_ms / 60000);
    uint32_t seconds = (uint32_t)(elapsed_ms / 1000);
    uint32_t rem_sec = seconds - minutes*60;
    uint32_t rem_min = minutes - hours*60;
    uint8_t h1=hours/10, h2=hours%10, m1=rem_min/10, m2=rem_min%10;
    uint8_t s1=rem_sec/10, s2=rem_sec%10;
    crgb_t col = CRGB(g_config.cd_r, g_config.cd_g, g_config.cd_b);
    int64_t remaining_us = g_countup_end_us - now_us;
    if (remaining_us > 0 && remaining_us <= 10000000LL && g_config.color_change_cd) col = CRGB_RED;
    if (hours > 0) {
        display_number(h1,6,col); display_number(h2,4,col);
        display_number(m1,2,col); display_number(m2,0,col);
    } else {
        display_number(m1,6,col); display_number(m2,4,col);
        display_number(s1,2,col); display_number(s2,0,col);
    }
}
```

- [ ] **Schritt 9: mode_scroll.c schreiben**

`components/clock_modes/mode_scroll.c`:
```c
#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"
#include "sensors.h"
#include "wifi_manager.h"
#include "esp_random.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

// Zeichen → g_numbers-Index-Tabelle (97 Zeichen, Offset 0–96)
static uint8_t char_to_idx(char c) {
    if (c>='0'&&c<='9') return c-'0';
    if (c==' ') return 10;
    if (c=='`'||c=='\'') return 17;
    if (c==',') return 22; if (c=='-') return 23;
    if (c=='.') return 24; if (c==':') return 27;
    if (c=='^') return 26; if (c=='%') return 15;
    if (c>='A'&&c<='Z') return 34+(c-'A');
    if (c>='a'&&c<='z') return 66+(c-'a');
    return 10; // unknown → space
}

void scroll(const char *text) {
    if (!text || text[0]=='\0') return;
    size_t len = strlen(text);
    if (len > 256) { text = "ArE U A HAckEr"; len = strlen(text); }

    // Übersetzungsarray: 6 Pad-Front + len*2 (letter+padding) + 6 Pad-Back
    size_t arr_len = 6 + len*2 + 6;
    uint8_t *trans = malloc(arr_len);
    if (!trans) return;
    for (int i=0; i<6; i++) trans[i]=96;
    for (size_t i=0; i<len; i++) {
        trans[6 + i*2]     = char_to_idx(text[i]);
        trans[6 + i*2 + 1] = 96;
    }
    for (int i=0; i<6; i++) trans[6+len*2+i]=96;

    crgb_t sc;
    if (g_config.scroll_color_settings == 0) {
        sc = CRGB(g_config.r[16], g_config.g[16], g_config.b[16]);
    } else {
        sc = random_color(g_config.pastel_colors);
    }

    size_t total = 6 + len*2;
    for (size_t pos=0; pos < total; pos++) {
        xSemaphoreTake(g_led_mutex, portMAX_DELAY);
        for (int i=0; i<SEGMENTS_LEDS; i++) g_leds[i]=CRGB_BLACK;
        for (int d=0; d<7; d++) {
            size_t idx = pos+d;
            if (idx < arr_len && trans[idx] != 96)
                display_number(trans[idx], 6-d, sc);
        }
        led_refresh();
        xSemaphoreGive(g_led_mutex);
        vTaskDelay(pdMS_TO_TICKS(80));
    }
    free(trans);
}

void mode_scroll_update(void) {
    // clockMode==11: scrolle konfigurierten Text
    if (!g_config.scroll_override) { scroll(g_config.scroll_text); return; }
    // scroll_override: baue kombinierten String
    char out[512] = " ";
    char tmp[64];
    time_t now = time(NULL); struct tm ti; localtime_r(&now, &ti);

    if (g_config.scroll_options[0]) { // time
        snprintf(tmp,sizeof(tmp),"%.2d%.2d    ", ti.tm_hour, ti.tm_min);
        strncat(out,tmp,sizeof(out)-strlen(out)-1);
    }
    if (g_config.scroll_options[1]) { // DOW
        const char *dow[]={"Sun    ","Mon    ","tUES    ","WEd    ","thur    ","FrI    ","SAt    "};
        strncat(out,dow[ti.tm_wday],sizeof(out)-strlen(out)-1);
    }
    if (g_config.scroll_options[2]) { // date
        snprintf(tmp,sizeof(tmp),"%.2d-%.2d    ", ti.tm_mon+1, ti.tm_mday);
        strncat(out,tmp,sizeof(out)-strlen(out)-1);
    }
    if (g_config.scroll_options[3]) { // year
        snprintf(tmp,sizeof(tmp),"%d    ", ti.tm_year+1900);
        strncat(out,tmp,sizeof(out)-strlen(out)-1);
    }
    if (g_config.scroll_options[4]) { // temp
        float t = g_sensors.temperature_c + g_config.temperature_correction;
        if (g_config.temperature_symbol==39) t = g_sensors.temperature_c*1.8f+32.0f+g_config.temperature_correction;
        char sym = (g_config.temperature_symbol==39)?'F':'C';
        snprintf(tmp,sizeof(tmp),"%.1f^%c    ", t, sym);
        strncat(out,tmp,sizeof(out)-strlen(out)-1);
    }
    if (g_config.scroll_options[5]) { // humi
        snprintf(tmp,sizeof(tmp),"%.0fH    ", g_sensors.humidity);
        strncat(out,tmp,sizeof(out)-strlen(out)-1);
    }
    if (g_config.scroll_options[6]) { // custom text
        strncat(out,g_config.scroll_text,sizeof(out)-strlen(out)-1);
        strncat(out,"    ",sizeof(out)-strlen(out)-1);
    }
    if (g_config.scroll_options[7]) { // IP
        char ip[20]={0}; wifi_manager_get_ip(ip,sizeof(ip));
        strncat(out,ip,sizeof(out)-strlen(out)-1);
    }
    // Fallback: alle deaktiviert → custom text
    bool any=false;
    for(int i=0;i<8;i++) if(g_config.scroll_options[i]) {any=true;break;}
    if (!any) strncpy(out,g_config.scroll_text,sizeof(out)-1);

    scroll(out);
}

void mode_scroll_overlay(void) {
    mode_scroll_update();
}

// Zeitbedingter Overlay: in main_task nach Frequenz prüfen (scrollFrequency 1-6)
```

- [ ] **Schritt 10: Shared flags in clock_modes definieren**

Geteilte Flags in `mode_time.c` (oder einer neuen `clock_modes_flags.c`) definieren:
```c
bool g_flag_min   = false;
bool g_flag_hour  = false;
bool g_flag_day   = false;
bool g_flag_week  = false;
bool g_flag_month = false;
```

- [ ] **Schritt 11: Build verifizieren**

```bash
idf.py build
```

- [ ] **Schritt 12: Commit**

```bash
git add components/clock_modes/
git commit -m "feat: add clock_modes component (time, date, temp, humi, scoreboard, cd, sw, scroll)"
```

---

## Task 10: Lightshow & Spectrum-Komponente

**Dateien:**
- Erstellen: `components/lightshow/include/lightshow.h`
- Erstellen: `components/lightshow/lightshow.c`
- Erstellen: `components/lightshow/lightshow_fx.c`
- Erstellen: `components/lightshow/spectrum.c`

- [ ] **Schritt 1: Header schreiben**

`components/lightshow/include/lightshow.h`:
```c
#pragma once
#include <stdint.h>
#include <stdbool.h>

void lightshow_dispatch(void);   // ruft je nach lightshowMode die passende Funktion

// Einzelne Lightshow-Effekte
void lightshow_chase(void);
void lightshow_twinkles(void);
void lightshow_rainbow(void);
void lightshow_rain(void);
void lightshow_fire(void);
void lightshow_snake(void);
void lightshow_cylon(void);
void lightshow_green_matrix(void);

// Spectrum Analyzer (eigenständiger Task)
void spectrum_task_fn(void *arg);   // FreeRTOS-Task-Einstiegspunkt
```

- [ ] **Schritt 2: lightshow.c (Dispatch) schreiben**

`components/lightshow/lightshow.c`:
```c
#include "lightshow.h"
#include "storage.h"
#include "led_display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

// Einfaches EVERY_N_MS Makro über Zeitstempel
#define EVERY_N_MS(ms, last_ms, body) do { \
    int64_t _now = esp_timer_get_time()/1000; \
    if (_now - (last_ms) >= (ms)) { (last_ms) = _now; body } \
} while(0)

static int64_t s_twinkle_t = 0, s_matrix_t = 0;
static int64_t s_fire_t    = 0, s_snake_t  = 0, s_cylon_t  = 0;

void lightshow_dispatch(void) {
    uint8_t mode = g_config.lightshow_mode;
    if (mode == 0) { lightshow_chase();  led_refresh(); }
    else if (mode == 1) { EVERY_N_MS(30, s_twinkle_t, { lightshow_twinkles(); led_refresh(); }); }
    else if (mode == 2) { lightshow_rainbow(); led_refresh(); }
    else if (mode == 3) { EVERY_N_MS(100, s_matrix_t, { lightshow_green_matrix(); }); }
    else if (mode == 4) { lightshow_rain(); led_refresh(); }
    else if (mode == 5) { EVERY_N_MS(60, s_fire_t, { lightshow_fire(); led_refresh(); }); }
    else if (mode == 6) {
        static int get_slower = 180;
        EVERY_N_MS(get_slower, s_snake_t, { lightshow_snake(); led_refresh(); });
    }
    else if (mode == 7) { EVERY_N_MS(150, s_cylon_t, { lightshow_cylon(); led_refresh(); }); }
}
```

- [ ] **Schritt 3: lightshow_fx.c schreiben**

`components/lightshow/lightshow_fx.c`:
```c
#include "lightshow.h"
#include "storage.h"
#include "led_display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include <string.h>
#include <stdlib.h>

// ── Chase ─────────────────────────────────────────────────────────────────────
void lightshow_chase(void) {
    static int pos = 0;
    static int cw_pos = 0;
    for (int i = 0; i < SEGMENTS_LEDS; i++) {
        g_leds[FAKE_LEDs[i]] = CRGB_BLACK;
    }
    for (int j = 0; j < 5; j++) {
        int idx = (pos + j * 7) % SEGMENTS_LEDS;
        g_leds[FAKE_LEDs[idx]] = color_wheel((cw_pos + j * 20) & 0xFF);
    }
    pos = (pos + 1) % SEGMENTS_LEDS;
    cw_pos = (cw_pos + 1) & 0xFF;
}

// ── Twinkles ──────────────────────────────────────────────────────────────────
void lightshow_twinkles(void) {
    for (int i = 0; i < SEGMENTS_LEDS; i++) {
        fade_to_black_by(&g_leds[FAKE_LEDs[i]], 30);
    }
    int n = 10;
    for (int i = 0; i < n; i++) {
        int idx = esp_random() % SEGMENTS_LEDS;
        g_leds[FAKE_LEDs[idx]] = random_color(g_config.pastel_colors);
    }
}

// ── Rainbow ───────────────────────────────────────────────────────────────────
void lightshow_rainbow(void) {
    static int hue = 0;
    for (int i = 0; i < SEGMENTS_LEDS; i++) {
        g_leds[FAKE_LEDs[i]] = hsv_to_rgb((hue + i * 256/SEGMENTS_LEDS) & 0xFF, 255, 200);
    }
    hue = (hue + 2) & 0xFF;
}

// ── Rain ──────────────────────────────────────────────────────────────────────
#define RAIN_COLS 37
static uint8_t s_rain_heat[RAIN_COLS] = {0};
static bool    s_rain_inited = false;
void lightshow_rain(void) {
    if (!s_rain_inited) {
        memset(s_rain_heat, 0, sizeof(s_rain_heat));
        s_rain_inited = true;
    }
    for (int c = 0; c < RAIN_COLS; c++) {
        if (esp_random() % 10 == 0) s_rain_heat[c] = 200 + (esp_random() & 55);
        if (s_rain_heat[c] > 20) s_rain_heat[c] -= 20;
        else s_rain_heat[c] = 0;
        crgb_t col = s_rain_heat[c] > 0 ? CRGB(0, 0, s_rain_heat[c]) : CRGB_BLACK;
        int base = c * LEDS_PER_SEGMENT;
        for (int j = 0; j < LEDS_PER_SEGMENT; j++) {
            int real = FAKE_LEDs_C_RAIN[base + j];
            if (real < NUM_LEDS) g_leds[real] = col;
        }
    }
    led_refresh();
}

// ── Fire ──────────────────────────────────────────────────────────────────────
#define FIRE_COLS 37
static uint8_t s_fire_heat[FIRE_COLS] = {0};
void lightshow_fire(void) {
    for (int c = 0; c < FIRE_COLS; c++) {
        if (esp_random() % 5 == 0) s_fire_heat[c] = 180 + (esp_random() & 75);
        if (s_fire_heat[c] > 15) s_fire_heat[c] -= 15;
        else s_fire_heat[c] = 0;
        uint8_t h = s_fire_heat[c];
        crgb_t col;
        if      (h > 200) col = CRGB(255, 255, h-200);
        else if (h > 100) col = CRGB(255, h-100, 0);
        else if (h >   0) col = CRGB(h*2, 0, 0);
        else              col = CRGB_BLACK;
        int base = c * LEDS_PER_SEGMENT;
        for (int j = 0; j < LEDS_PER_SEGMENT; j++) {
            int real = FAKE_LEDs_C_FIRE[base + j];
            if (real < NUM_LEDS) g_leds[real] = col;
        }
    }
    led_refresh();
}

// ── Snake ─────────────────────────────────────────────────────────────────────
void lightshow_snake(void) {
    static int pos = 0;
    static int dir = 1;
    static int cw  = 0;
    for (int i = 0; i < SEGMENTS_LEDS; i++) fade_to_black_by(&g_leds[FAKE_LEDs_SNAKE[i]], 50);
    for (int j = 0; j < 14; j++) {
        int idx = (pos + j) % SEGMENTS_LEDS;
        g_leds[FAKE_LEDs_SNAKE[idx]] = color_wheel((cw + j*8) & 0xFF);
    }
    pos += dir;
    if (pos >= SEGMENTS_LEDS || pos < 0) { dir = -dir; pos += dir; }
    cw = (cw + 3) & 0xFF;
}

// ── Cylon ─────────────────────────────────────────────────────────────────────
void lightshow_cylon(void) {
    static int pos = 0;
    static int dir = 1;
    static int cw  = 0;
    for (int i = 0; i < SEGMENTS_LEDS; i++) fade_to_black_by(&g_leds[FAKE_LEDs[i]], 40);
    int base = pos * LEDS_PER_SEGMENT;
    for (int j = 0; j < LEDS_PER_SEGMENT; j++) {
        if (base+j < SEGMENTS_LEDS) g_leds[FAKE_LEDs[base+j]] = color_wheel(cw & 0xFF);
    }
    pos += dir;
    if (pos >= RAIN_COLS || pos < 0) { dir = -dir; pos += dir; }
    cw = (cw + 4) & 0xFF;
}

// ── GreenMatrix ───────────────────────────────────────────────────────────────
#define GM_COLS 37
static uint8_t s_gm_pos[GM_COLS];
static bool    s_gm_inited = false;
void lightshow_green_matrix(void) {
    if (!s_gm_inited) {
        for (int i=0;i<GM_COLS;i++) s_gm_pos[i]=esp_random()%LEDS_PER_SEGMENT;
        s_gm_inited=true;
    }
    for (int c=0; c<GM_COLS; c++) {
        int base=c*LEDS_PER_SEGMENT;
        for (int j=0;j<LEDS_PER_SEGMENT;j++) {
            int real=FAKE_LEDs_C_BMUP[base+j];
            if (real<NUM_LEDS) {
                if (j==s_gm_pos[c]) g_leds[real]=CRGB(0,255,0);
                else fade_to_black_by(&g_leds[real],60);
            }
        }
        if (esp_random()%3==0) s_gm_pos[c]=(s_gm_pos[c]+1)%LEDS_PER_SEGMENT;
    }
    led_refresh();
}
```

- [ ] **Schritt 4: spectrum.c schreiben**

`components/lightshow/spectrum.c`:
```c
#include "lightshow.h"
#include "storage.h"
#include "led_display.h"
#include "sensors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
#include <string.h>

static int  s_react     = 0;
static int  s_decay_cnt = 0;
static const int DECAY  = 3;
static int  s_cw_pos    = 0;
static int  s_cw_speed  = 2;

// Einfache Fire-Palette: [0]=kühl .. [15]=heiß → gelb-weiß
static const uint32_t FIRE_PAL[16] = {
    0x110000,0x330000,0x660000,0x990000,0xCC0000,0xFF0000,
    0xFF3300,0xFF6600,0xFF9900,0xFFCC00,0xFFFF00,0xFFFF33,
    0xFFFF66,0xFFFF99,0xFFFFCC,0xFFFFFF
};
static crgb_t fire_color(int i) {
    int idx = (i * 15) / (SPECTRUM_PIXELS - 1);
    uint32_t c = FIRE_PAL[idx];
    return CRGB((c>>16)&0xFF,(c>>8)&0xFF,c&0xFF);
}

void spectrum_task_fn(void *arg) {
    while (1) {
        if (g_config.clock_mode != 9 || !g_config.realtime_mode) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        int32_t level = i2s_mic_get_level();
        // Skaliere Amplitude auf 0..SPECTRUM_PIXELS
        int pre = (int)((long)SPECTRUM_PIXELS * (long)(level >> 8)) / 8192;
        if (pre > s_react) s_react = pre;
        if (s_react > SPECTRUM_PIXELS) s_react = SPECTRUM_PIXELS;

        xSemaphoreTake(g_led_mutex, portMAX_DELAY);
        uint8_t sm = g_config.spectrum_mode;
        for (int i = SPECTRUM_PIXELS-1; i >= 0; i--) {
            int fake_base = i * LEDS_PER_SEGMENT;
            int fire_rand = esp_random() % 4;
            for (int s = 0; s < LEDS_PER_SEGMENT; s++) {
                int real;
                switch (sm) {
                    case  0: real=FAKE_LEDs_C_BMUP[fake_base+s]; break;
                    case  1: real=FAKE_LEDs_C_CMOT[fake_base+s]; break;
                    case  2: real=FAKE_LEDs_C_BLTR[fake_base+s]; break;
                    case  3: real=FAKE_LEDs_C_TLBR[fake_base+s]; break;
                    case  4: real=FAKE_LEDs_C_VERT[fake_base+s]; break;
                    case  5: real=FAKE_LEDs_C_TMDN[fake_base+s]; break;
                    case  6: real=FAKE_LEDs_C_CSIN[fake_base+s]; break;
                    case  7: real=FAKE_LEDs_C_BRTL[fake_base+s]; break;
                    case  8: real=FAKE_LEDs_C_TRBL[fake_base+s]; break;
                    case  9: real=FAKE_LEDs_C_OUTS[fake_base+s]; break;
                    case 10: real=FAKE_LEDs_C_VERT2[fake_base+s]; break;
                    default: real=FAKE_LEDs_C_OUTS2[fake_base+s]; break;
                }
                if (real >= NUM_LEDS) continue;
                crgb_t col;
                if (i < s_react) {
                    uint8_t cs = g_config.spectrum_color_settings;
                    if (cs == 0) col = CRGB(g_config.r[15],g_config.g[15],g_config.b[15]);
                    else if (cs == 1) col = hsv_to_rgb((uint8_t)((255/SPECTRUM_PIXELS)*i),255,255);
                    else if (cs == 2) col = color_wheel((i*256/SPECTRUM_PIXELS + s_cw_pos) & 0xFF);
                    else if (cs == 3) col = fire_color(i);
                    else col = color_wheel((i*256/SPECTRUM_PIXELS + s_cw_pos) & 0xFF);
                } else {
                    uint8_t bs = g_config.spectrum_background_settings;
                    if (bs == 0) col = CRGB(g_config.r[17],g_config.g[17],g_config.b[17]);
                    else if (bs == 1) col = color_wheel((i*256/SPECTRUM_PIXELS + s_cw_pos) & 0xFF);
                    else col = color_wheel2(s_cw_pos & 0xFF);
                }
                g_leds[real] = col;
            }
        }
        led_refresh();
        xSemaphoreGive(g_led_mutex);

        s_cw_pos = (s_cw_pos - s_cw_speed) & 0xFF;
        s_decay_cnt++;
        if (s_decay_cnt >= DECAY) {
            s_decay_cnt = 0;
            if (s_react > 0) s_react--;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

- [ ] **Schritt 5: Build verifizieren**

```bash
idf.py build
```

- [ ] **Schritt 6: Commit**

```bash
git add components/lightshow/
git commit -m "feat: add lightshow and spectrum components (8 effects + I2S spectrum analyzer)"
```

---

## Task 11: Haupt-Applikation (main.c)

**Dateien:**
- Überschreiben: `main/main.c` (Stub aus Task 1 durch vollständige Implementation ersetzen)

- [ ] **Schritt 1: main.c schreiben**

`main/main.c`:
```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sntp.h"
#include <time.h>
#include <string.h>

#include "storage.h"
#include "led_display.h"
#include "sensors.h"
#include "clock_modes.h"
#include "lightshow.h"
#include "rtttl_player.h"
#include "web_server.h"
#include "wifi_manager.h"

static const char *TAG = "main";

// ── NTP ───────────────────────────────────────────────────────────────────────
static void ntp_sync_start(void) {
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_init();

    // POSIX TZ-String aus gmt_offset_sec und ds_time bauen
    int gmt_h = g_config.gmt_offset_sec / 3600;
    // Einfacher UTC+N String; für volle DST-Unterstützung ggf. POSIX-TZ anpassen
    char tz[32];
    snprintf(tz, sizeof(tz), "UTC%d", -gmt_h);
    setenv("TZ", tz, 1);
    tzset();

    // Warte max. 10s auf Sync
    int retry = 0;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        retry++;
    }
    if (sntp_get_sync_status() != SNTP_SYNC_STATUS_RESET) {
        ESP_LOGI(TAG, "NTP synchronized");
        // Täglich RTC aus NTP-Zeit setzen
        time_t now = time(NULL);
        struct tm ti; localtime_r(&now, &ti);
        ds3231_set_time(&ti);
    } else {
        ESP_LOGW(TAG, "NTP sync failed — trying RTC fallback");
        struct tm rtc_time;
        if (ds3231_get_time(&rtc_time) && !ds3231_lost_power()) {
            time_t t = mktime(&rtc_time);
            struct timeval tv = { .tv_sec = t };
            settimeofday(&tv, NULL);
            ESP_LOGI(TAG, "RTC time applied");
        }
    }
}

// ── Sensor-Update ─────────────────────────────────────────────────────────────
static void update_sensors(void) {
    // DHT11 (langsam, max 1 Hz)
    static int64_t s_dht_last = 0;
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (now_ms - s_dht_last >= 2000) {
        float tc=0, h=0;
        if (dht11_read(&tc, &h)) {
            g_sensors.temperature_c = tc + g_config.temperature_correction;
            g_sensors.temperature_f = tc * 1.8f + 32.0f + g_config.temperature_correction;
            g_sensors.humidity = h;
        }
        s_dht_last = now_ms;
    }
    // Fotowiderstand (ADC)
    g_sensors.light_level = adc_light_read();
    // I2S Audio-Level (für Auto-Dim außerhalb Spectrum-Mode)
    if (g_config.clock_mode != 9) {
        g_sensors.audio_level = i2s_mic_get_level();
    }
}

// ── Helligkeit ────────────────────────────────────────────────────────────────
static void apply_brightness(void) {
    uint8_t bright;
    if (g_config.brightness == 10) {
        bright = adc_light_to_brightness(g_sensors.light_level);
    } else {
        bright = g_config.brightness;
    }
    // Skaliere g_leds[] vor dem Refresh (Software-Dimming)
    xSemaphoreTake(g_led_mutex, portMAX_DELAY);
    for (int i = 0; i < NUM_LEDS; i++) {
        g_leds[i].r = (uint8_t)((uint16_t)g_leds[i].r * bright / 255);
        g_leds[i].g = (uint8_t)((uint16_t)g_leds[i].g * bright / 255);
        g_leds[i].b = (uint8_t)((uint16_t)g_leds[i].b * bright / 255);
    }
    xSemaphoreGive(g_led_mutex);
}

// ── Scroll-Overlay-Frequenz prüfen ────────────────────────────────────────────
static void check_scroll_overlay(struct tm *ti, int secs) {
    if (!g_config.scroll_override) return;
    int m1 = ti->tm_min / 10, m2 = ti->tm_min % 10;
    int cm = g_config.clock_mode;
    if (cm == 11 || cm == 1 || cm == 4) return; // Scroll/CD/Stopwatch: kein Overlay
    bool do_scroll = false;
    switch (g_config.scroll_frequency) {
        case 1: do_scroll = (secs == 0); break;
        case 2: do_scroll = ((m2==0||m2==5) && secs==0); break;
        case 3: do_scroll = (m2==0 && secs==0); break;
        case 4: do_scroll = (((m1==0&&m2==0)||(m1==1&&m2==5)||(m1==3&&m2==0)||(m1==4&&m2==5))&&secs==0); break;
        case 5: do_scroll = (((m1==0&&m2==0)||(m1==3&&m2==0))&&secs==0); break;
        case 6: do_scroll = (g_flag_hour && secs==0); break;
    }
    if (do_scroll) mode_scroll_overlay();
}

// ── Main Task ─────────────────────────────────────────────────────────────────
static void main_task(void *arg) {
    int prev_min = -1, prev_hour = -1, prev_day = -1, prev_month = -1, prev_week = -1;

    // Einmal NTP bei Boot synchronisieren
    if (wifi_manager_is_connected()) {
        ntp_sync_start();
    } else {
        struct tm rtc_time;
        if (ds3231_get_time(&rtc_time) && !ds3231_lost_power()) {
            time_t t = mktime(&rtc_time);
            struct timeval tv = { .tv_sec = t };
            settimeofday(&tv, NULL);
        }
    }

    // Startup-Sound
    rtttl_play_song(0); // SMB

    // 1111-Uhr-Check-Tracker
    bool wish_done_today = false;

    while (1) {
        int64_t tick_start = esp_timer_get_time();

        // Sensor-Daten aktualisieren
        update_sensors();

        // Aktuelle Zeit
        time_t now = time(NULL);
        struct tm ti; localtime_r(&now, &ti);
        int secs = ti.tm_sec;

        // Flags für Farbwechsel-Frequenz
        g_flag_min   = (ti.tm_min  != prev_min);
        g_flag_hour  = (ti.tm_hour != prev_hour);
        g_flag_day   = (ti.tm_mday != prev_day);
        g_flag_month = (ti.tm_mon  != prev_month);
        int cur_week = (ti.tm_yday + 7 - (ti.tm_wday ? ti.tm_wday-1 : 6)) / 7;
        g_flag_week  = (cur_week   != prev_week);

        if (g_flag_min)   prev_min   = ti.tm_min;
        if (g_flag_hour)  prev_hour  = ti.tm_hour;
        if (g_flag_day)   prev_day   = ti.tm_mday;
        if (g_flag_month) prev_month = ti.tm_mon;
        if (g_flag_week)  prev_week  = cur_week;

        // Täglich NTP re-sync + RTC update
        if (g_flag_day && wifi_manager_is_connected()) {
            esp_sntp_restart();
            vTaskDelay(pdMS_TO_TICKS(2000));
            time_t t2 = time(NULL);
            struct tm ti2; localtime_r(&t2, &ti2);
            ds3231_set_time(&ti2);
        }

        // 11:11 / 23:11 Easter-Egg
        if ((ti.tm_hour==11||ti.tm_hour==23) && ti.tm_min==11 && secs==0 && !wish_done_today) {
            wish_done_today = true;
            scroll("MAkE A WISH");
        }
        if (ti.tm_hour==0 && ti.tm_min==0) wish_done_today = false;

        // Scroll-Overlay prüfen
        check_scroll_overlay(&ti, secs);

        // Random-Spectrum-Modus
        if (g_config.random_spectrum_mode && g_config.clock_mode == 9 && g_flag_min) {
            all_blank();
            xSemaphoreTake(g_config_mutex, portMAX_DELAY);
            g_config.spectrum_mode = esp_random() % 12;
            xSemaphoreGive(g_config_mutex);
        }

        // Modus-Dispatch (suspend_type: 0=immer, 1=nur wenn wach)
        bool suspended = (g_config.suspend_type != 0); // vereinfacht: suspend_type==1 = Nacht-Aus
        bool show = !suspended;

        xSemaphoreTake(g_led_mutex, portMAX_DELAY);
        if (show || g_config.clock_mode == 1 || g_config.clock_mode == 4) {
            all_blank();
            switch (g_config.clock_mode) {
                case 0: mode_time_update();        break;
                case 1: mode_countdown_update();   break;
                case 2: mode_temperature_update(); break;
                case 3: mode_scoreboard_update();  break;
                case 4: mode_stopwatch_update();   break;
                case 5: lightshow_dispatch();      break;
                case 7: mode_date_update();        break;
                case 8: mode_humidity_update();    break;
                case 9: /* spectrum: eigener Task */break;
                case 10: /* display off */          break;
                case 11: mode_scroll_update();     break;
            }
            shelf_down_lights();
        }
        xSemaphoreGive(g_led_mutex);

        // Helligkeit anwenden und LEDs aktualisieren
        if (g_config.clock_mode != 9) {
            apply_brightness();
            xSemaphoreTake(g_led_mutex, portMAX_DELAY);
            led_refresh();
            xSemaphoreGive(g_led_mutex);
        }

        // Exakt 1-Sekunden-Takt halten
        int64_t elapsed = (esp_timer_get_time() - tick_start) / 1000;
        int32_t delay = 1000 - (int32_t)elapsed;
        if (delay > 10) vTaskDelay(pdMS_TO_TICKS(delay));
        else            vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ── Einstiegspunkt ────────────────────────────────────────────────────────────
void app_main(void) {
    // NVS initialisieren
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    // Einstellungen laden
    storage_init();
    storage_load();
    ESP_LOGI(TAG, "Config loaded, mode=%d", g_config.clock_mode);

    // Hardware initialisieren
    led_display_init();
    sensors_init();
    rtttl_player_init();

    // WiFi starten (blockiert bis verbunden oder SoftAP aktiv)
    bool wifi_ok = wifi_manager_init();

    // Web-Server (nur wenn WiFi verbunden)
    if (wifi_ok) {
        web_server_init();
        ESP_LOGI(TAG, "Web server ready at http://shelfclock.local");
    }

    // Spectrum-Task (hohe Priorität, läuft immer, suspendiert sich selbst wenn mode!=9)
    xTaskCreate(spectrum_task_fn, "spectrum", 4096, NULL, 6, NULL);

    // Haupt-Task
    xTaskCreate(main_task, "main", 8192, NULL, 5, NULL);

    // app_main darf zurückkehren — alle Arbeit in Tasks
}
```

- [ ] **Schritt 2: Build verifizieren (vollständig)**

```bash
idf.py build
```

Erwartete Ausgabe: `Project build complete.` ohne Fehler. Warnungen über unused variables sind OK.

- [ ] **Schritt 3: Commit**

```bash
git add main/main.c
git commit -m "feat: add main application with FreeRTOS tasks, NTP sync, 1s main loop"
```

---

## Task 12: SPIFFS-Daten-Image & Flash

**Ziel:** Die Web-Dateien aus `data/` als SPIFFS-Partition auf den ESP32-C3 flashen.

- [ ] **Schritt 1: CMakeLists.txt um SPIFFS-Image erweitern**

In `CMakeLists.txt` nach `project(shelfclock)` einfügen:

```cmake
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(shelfclock)

# SPIFFS-Image aus data/ Verzeichnis bauen
spiffs_create_partition_image(spiffs ../data FLASH_IN_PROJECT)
```

Das `FLASH_IN_PROJECT` sorgt dafür, dass `idf.py flash` das SPIFFS-Image automatisch mitflasht.

- [ ] **Schritt 2: data/-Verzeichnis prüfen**

```bash
ls data/
```

Erwartete Ausgabe: `index.html`, `settings.html`, `js/`, `css/` (oder ähnliche Web-Dateien aus dem Original-Projekt).

Falls `data/` leer ist oder fehlt: die HTML/CSS/JS-Dateien aus dem Original-Repo übernehmen. Die Web-Dateien im Original-Projekt sind dieselben wie in der Arduino-Version — sie wurden für SPIFFS konzipiert und können unverändert übernommen werden.

- [ ] **Schritt 3: Vollständiger Build mit SPIFFS**

```bash
idf.py build
```

Erwartete Ausgabe:
```
...
Generating spiffs.bin
Project build complete.
```

- [ ] **Schritt 4: Flashen (alle Partitionen)**

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Ersetzt `/dev/ttyUSB0` durch den korrekten Port (macOS: `/dev/cu.usbserial-*`).

`idf.py flash` flasht automatisch:
- Bootloader (`build/bootloader/bootloader.bin`)
- Partition table (`build/partition_table/partition-table.bin`)
- Firmware (`build/shelfclock.bin`)
- SPIFFS-Image (`build/spiffs.bin` → Adresse `0x320000` gemäß `partitions.csv`)

- [ ] **Schritt 5: Erste Inbetriebnahme**

1. Nach dem Flashen zeigt der Monitor-Output den Boot-Prozess.
2. Beim ersten Start (ohne WiFi-Credentials in NVS): SSID `ShelfClock-Setup` erscheint im WLAN.
3. Im Browser `192.168.4.1` öffnen → SSID + Passwort eingeben → ESP startet neu.
4. Nach Neustart: WiFi verbindet, NTP synchronisiert, `http://shelfclock.local` erreichbar.
5. Web-Interface öffnet sich mit den SPIFFS-HTML-Seiten.

- [ ] **Schritt 6: Nur SPIFFS neu flashen (nach Web-Dateien-Änderungen)**

```bash
idf.py -p /dev/cu.usbserial-* spiffs-flash
```

Oder manuell:
```bash
esptool.py --port /dev/cu.usbserial-* write_flash 0x320000 build/spiffs.bin
```

- [ ] **Schritt 7: Finaler Commit**

```bash
git add CMakeLists.txt
git commit -m "feat: add SPIFFS partition image build from data/ directory"
```

---

## Implementierungs-Reihenfolge (Zusammenfassung)

Tasks in dieser Reihenfolge ausführen — jeder Task baut auf dem vorherigen auf:

1. **Task 1** — Projekt-Gerüst: Verzeichnisse, CMakeLists.txt, Stubs
2. **Task 2** — Storage: NVS-Einstellungen, clock_config_t
3. **Task 3** — LED-Display: RMT, FAKE_LEDs-Arrays, display_number()
4. **Task 4** — Sensors: DHT11, DS3231, ADC, INMP441 I2S
5. **Task 5** — RTTTL-Player: LEDC, 24 Songs, Queue-Task
6. **Task 6** — WiFi-Manager: STA + SoftAP Captive Portal
7. **Task 7** — Web-Server-Kern: SPIFFS, mDNS, OTA, httpd_start
8. **Task 8** — Web-Server-Handler: alle ~90 Endpoints
9. **Task 9** — Clock-Modi: 8 Anzeigemodi
10. **Task 10** — Lightshow & Spectrum: 8 Effekte + I2S-Spectrum
11. **Task 11** — main.c: vollständige App-Verdrahtung
12. **Task 12** — SPIFFS-Flash: Web-Dateien flashen

Nach jedem Task: `idf.py build` + `git commit`.
