# ShelfClock ESP-IDF 5.4.3 Port — Design-Dokument

**Datum:** 2026-04-25
**Ziel-Hardware:** ESP32-C3
**Framework:** ESP-IDF 5.4.3 (natives CMake Build-System)
**Original:** Arduino-basierter Code (`old/ShelfClock.ino`)

---

## Überblick

Portierung des ShelfClock Arduino-Projekts auf ESP-IDF 5.4.3 für den ESP32-C3.
Alle Funktionen des Originals werden erhalten. Der alte Arduino-Code verbleibt im
Verzeichnis `old/` als Referenz.

Wesentliche Hardware-Änderung: Das analoge Mikrofon (ADC) wird durch ein
**INMP441 I2S-MEMS-Mikrofon** ersetzt. Der `AUDIO_GATE_PIN` entfällt.

---

## Funktionsumfang (vollständig erhalten)

- **Uhrmodus** — 12h/24h/Militär/Blinkend-Mitte, Neujahrs-Countdown
- **Datumsmodus** — 6 Anzeigetypen (MM.DD, Wochentag, Jahr, etc.)
- **Temperaturmodus** — DHT11, Celsius/Fahrenheit, 5 Anzeigetypen
- **Feuchtigkeitsmodus** — DHT11, 3 Anzeigetypen
- **Scoreboard** — Links/Rechts Punktestand
- **Countdown-Timer** — mit optionalem RTTTL-Alarm
- **Stoppuhr / Countup-Timer**
- **Textscroller** — 97-Zeichen Zeichensatz, konfigurierbar
- **Scroll-Overlay** — Zeit/Datum/Temp/Feuchte/IP automatisch eingeblendet
- **Lightshow-Modi** — Chase, Twinkles, Rainbow, Rain, Fire, Snake, Cylon, GreenMatrix
- **Spectrum Analyzer** — 11 Layout-Modi, via INMP441 I2S
- **Spotlights** — Unterbeleuchtungs-LEDs mit eigener Farbeinstellung
- **Auto-Helligkeit** — Fotowiderstand (ADC)
- **Web-Interface** — vollständiges HTML/CSS/JS aus SPIFFS
- **OTA-Updates** — Firmware-Upload via Web-Interface
- **mDNS** — erreichbar unter `http://shelfclock`
- **WiFi-Provisioning** — SoftAP Captive Portal (ersetzt AutoConnect)
- **NVS-Einstellungen** — alle Parameter persistent, 2 Presets
- **DS3231 RTC** — Fallback wenn kein NTP erreichbar, täglich sync
- **RTTTL-Alarme** — 25 Songs, datumsabhängige Spezial-Songs
- **Sleep-Timer** — konfigurierbares Abschalten

---

## Projektstruktur

```
ShelfClock/
├── old/                          # Original Arduino-Code (unverändert)
│   └── ShelfClock.ino
├── CMakeLists.txt                # Top-Level CMake
├── sdkconfig.defaults            # ESP32-C3 Standardeinstellungen
├── partitions.csv                # Angepasstes Partitionsschema
├── main/
│   ├── CMakeLists.txt
│   └── main.c                   # Einstiegspunkt, FreeRTOS Task-Erstellung
└── components/
    ├── led_display/             # WS2812B via RMT, CRGB-Typen, displayNumber()
    ├── sensors/                 # DHT11, DS3231 (I2C), ADC (Foto), INMP441 (I2S)
    ├── clock_modes/             # Alle 12 Anzeigemodi
    ├── lightshow/               # Chase, Rainbow, Rain, Fire, Snake, Cylon, Spectrum
    ├── rtttl_player/            # RTTTL-Musik via LEDC/PWM, non-blocking
    ├── web_server/              # esp_http_server, SPIFFS, OTA, mDNS, alle Endpoints
    ├── wifi_manager/            # STA + SoftAP Captive Portal Provisioning
    └── storage/                 # NVS-Einstellungen, clock_config_t, Presets
```

---

## Architektur

### Globaler Zustand

Alle Einstellungen leben in einer einzigen `clock_config_t`-Struktur in der
`storage`-Komponente. Sie ist global lesbar (`extern clock_config_t g_config`).
Schreibzugriffe erfolgen nur über `storage`-Funktionen mit anschließendem
NVS-Save. Ein `g_config_mutex` schützt gleichzeitige Zugriffe zwischen
HTTP-Task und Main-Task.

```c
typedef struct {
    uint8_t  clock_mode;
    uint8_t  clock_display_type;
    uint8_t  date_display_type;
    uint8_t  temp_display_type;
    uint8_t  humi_display_type;
    uint8_t  brightness;          // 10 = Auto
    int32_t  gmt_offset_sec;
    bool     ds_time;
    uint8_t  clock_color_settings;
    uint8_t  date_color_settings;
    uint8_t  temp_color_settings;
    uint8_t  humi_color_settings;
    // Farben r0..r17, g0..g17, b0..b17
    uint8_t  r[18], g[18], b[18];
    // Countdown/Countup
    uint8_t  cd_r, cd_g, cd_b;
    // Einstellungen
    uint8_t  temperature_symbol;  // 36=C, 39=F
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
    bool     scroll_options[8];   // 0=MilTime,1=DOW,2=Date,3=Year,4=Temp,5=Humi,6=Text,7=IP
    uint8_t  lightshow_mode;
    uint8_t  suspend_frequency;
    uint8_t  suspend_type;
    char     scroll_text[257];
} clock_config_t;

extern clock_config_t g_config;
extern SemaphoreHandle_t g_config_mutex;
extern SemaphoreHandle_t g_led_mutex;
```

### Sensor-Daten

```c
typedef struct {
    float    temperature_c;
    float    temperature_f;
    float    humidity;
    uint16_t light_level;     // 0-4095 (ADC 12-bit)
    int32_t  audio_level;     // Peak-Amplitude aus I2S
    struct tm rtc_time;
} sensor_data_t;

extern sensor_data_t g_sensors;
```

---

## FreeRTOS Tasks

| Task | Stack | Priorität | Funktion |
|---|---|---|---|
| `main_task` | 8192 | 5 | 1-Sekunden-Timer-Loop, Modus-Dispatch, LED-Show |
| `http_task` | 8192 | 4 | HTTP-Server (nicht-blockierend, `httpd_start`) |
| `rtttl_task` | 4096 | 3 | Musik-Queue abarbeiten, LEDC PWM |
| `spectrum_task` | 4096 | 6 | I2S lesen, Amplitude berechnen, LEDs updaten |

`spectrum_task` läuft nur wenn `clock_mode == 9`. In allen anderen Modi ist er
suspendiert (`vTaskSuspend`/`vTaskResume`).

---

## Komponenten-Details

### `led_display`

- Verwendet die ESP-IDF `led_strip` Komponente (RMT v2)
- Implementiert `CRGB`-Struct und alle Display-Hilfsfunktionen:
  `display_number()`, `all_blank()`, `shelf_down_lights()`, `blink_dots()`
- Das LED-Array `g_leds[NUM_LEDS]` ist global, geschützt durch `g_led_mutex`
- `led_strip_refresh()` ersetzt `FastLED.show()`

### `sensors`

- **DHT11:** GPIO Bit-Banging mit `esp_timer` für Mikrosekunden-Timing
- **DS3231:** `i2c_master` Treiber, I2C-Adresse 0x68, Register-direktzugriff
- **Fotowiderstand:** `adc_oneshot` API, 15 Samples Glättung wie im Original
- **INMP441:** `i2s_std` Treiber, 32-bit Slots, 16kHz Sample-Rate, linker Kanal,
  Peak-Detektion für Spectrum Analyzer und Auto-Dim

### `clock_modes`

- Alle 12 Modi als einzelne Funktionen: `display_time_mode()`,
  `display_date_mode()`, `display_temperature_mode()`, etc.
- Jede Funktion liest aus `g_config` und `g_sensors`, schreibt in `g_leds`
- `scroll()` Funktion mit 97-Zeichen Zeichensatz vollständig portiert
- Neujahrs-Countdown mit DST-Berechnung erhalten

### `lightshow`

- Alle 8 Lightshow-Funktionen portiert: `chase()`, `twinkles()`, `rainbow()`,
  `rain()`, `fire()`, `snake()`, `cylon()`, `green_matrix()`
- Alle FAKE_LED Layout-Arrays (BMUP, CMOT, BLTR, etc.) unverändert übernommen
- Spectrum Analyzer mit I2S-Amplitude statt ADC-Rohwert

### `rtttl_player`

- 25 RTTTL-Songs aus dem Original übernommen
- Queue-basiert: `rtttl_queue_t` nimmt Song-Index entgegen
- `ledc_set_freq()` + `ledc_set_duty()` steuern Buzzer
- Non-blocking: eigener Task, `vTaskDelay(1)` zwischen Noten
- `breakout_flag` (atomares `atomic_bool`) stoppt Wiedergabe auf Web-Befehl

### `web_server`

- `esp_http_server` mit allen ~90 Endpoints aus dem Original
- SPIFFS-Dateien werden via `esp_vfs_spiffs_register()` gemountet
- Statische Dateien (HTML/CSS/JS) direkt aus SPIFFS serviert
- OTA via `esp_ota_begin/write/end` API
- mDNS: `mdns_init()` + `mdns_hostname_set("shelfclock")`
- Debug-Endpoint `/debugpage` erhalten

### `wifi_manager`

**Normaler Start (Credentials vorhanden):**
1. `esp_wifi_set_mode(WIFI_MODE_STA)` + connect
2. 60s Timeout — bei Erfolg: weiter
3. Bei Fehlschlag: SoftAP starten

**Provisioning (keine Credentials / Fehlschlag):**
1. `esp_wifi_set_mode(WIFI_MODE_AP)`, SSID: `ShelfClock-Setup`, kein Passwort
2. IP: `192.168.4.1`, DNS-Catch-All für Captive Portal
3. Einfache HTML-Seite: SSID-Liste (WiFi-Scan) + Passwort-Eingabe
4. Bei Submit: Credentials in NVS speichern → `esp_restart()`

### `storage`

- NVS-Namespace: `"shelfclock"` (Haupteinstellungen)
- NVS-Namespace: `"shelfclock-p1"` (Preset 1)
- NVS-Namespace: `"shelfclock-p2"` (Preset 2)
- `storage_load()` beim Boot, `storage_save_key()` bei jeder Änderung
- `storage_save_preset(n)` / `storage_load_preset(n)` für Presets

---

## Pin-Belegung (final)

| GPIO | Funktion | Treiber | Hinweis |
|---|---|---|---|
| GPIO1 | Fotowiderstand | `adc_oneshot` ADC1_CH1 | 10kΩ Spannungsteiler nach GND |
| GPIO4 | WS2812B LEDs | `led_strip` (RMT) | Level-Shifter 3.3V→5V empfohlen |
| GPIO6 | INMP441 SCK | `i2s_std` | Bit-Clock |
| GPIO7 | INMP441 WS | `i2s_std` | Word-Select |
| GPIO8 | INMP441 SD | `i2s_std` | Daten-Eingang |
| GPIO10 | DHT11 | GPIO Bit-Bang | 10kΩ Pull-Up nach 3.3V |
| GPIO16 | Buzzer | `ledc` Kanal 0 | Piezo direkt oder via NPN |
| GPIO18 | DS3231 SDA | `i2c_master` | 4.7kΩ Pull-Up nach 3.3V |
| GPIO19 | DS3231 SCL | `i2c_master` | 4.7kΩ Pull-Up nach 3.3V |

---

## Partitionsschema

```csv
# partitions.csv
# Name,   Type, SubType,  Offset,   Size
nvs,      data, nvs,      0x9000,   0x7000,
otadata,  data, ota,      0x10000,  0x2000,
app0,     app,  ota_0,    0x20000,  0x180000,
app1,     app,  ota_1,    0x1A0000, 0x180000,
spiffs,   data, spiffs,   0x320000, 0xE0000,
```

---

## sdkconfig.defaults (Schlüsseleinstellungen)

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
CONFIG_I2C_ENABLE_DEBUG_LOG=n
```

---

## Arduino → ESP-IDF Ersetzungstabelle

| Arduino / Library | ESP-IDF 5.x Ersatz |
|---|---|
| `FastLED` / `CRGB` | `led_strip` Komponente + eigene `crgb_t` Struct |
| `FastLED.show()` | `led_strip_refresh()` |
| `fill_solid()` | Eigene Hilfsfunktion |
| `CHSV()` | Eigene HSV→RGB Konvertierung |
| `Preferences` | `nvs_flash` + `nvs_open/get/set` |
| `WebServer` | `esp_http_server` |
| `HTTPUpdateServer` | `esp_ota_begin/write/end` |
| `SPIFFS.begin()` | `esp_vfs_spiffs_register()` |
| `ESPmDNS` | `mdns` Komponente |
| `AutoConnect` | Eigener `wifi_manager` mit SoftAP + Captive Portal |
| `DHT.h` | GPIO Bit-Bang in `sensors` Komponente |
| `RTClib` (DS3231) | `i2c_master` + DS3231 Register direkt |
| `analogRead()` | `adc_oneshot_read()` |
| `NonBlockingRtttl` | Eigene `rtttl_player` Komponente mit `ledc` |
| `analogRead(MIC)` | I2S `i2s_channel_read()` + Peak-Detektion |
| `millis()` | `esp_timer_get_time() / 1000` |
| `delay()` | `vTaskDelay(pdMS_TO_TICKS(ms))` |
| `pinMode/digitalWrite` | `gpio_set_direction/level()` |
| `Serial.println()` | `ESP_LOGI(TAG, ...)` |
| `WiFi.localIP()` | `esp_netif_get_ip_info()` |
| `configTime()` + NTP | `esp_sntp_setservername()` + `sntp_restart()` |
| `getLocalTime()` | `localtime_r()` nach SNTP sync |
| `settimeofday()` | `settimeofday()` (gleich, POSIX) |

---

## Nicht übernommen

- `MegunoLink.h` — war nur für serielle Visualisierung während Entwicklung, entfällt
- `AUDIO_GATE_PIN` — entfällt, INMP441 ersetzt SparkFun Sound Detector komplett
- Arduino-spezifisches Partitionsschema (`default_nvs.csv`) — durch `partitions.csv` ersetzt
