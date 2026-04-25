#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"
#include "wifi_manager.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

static uint8_t char_to_idx(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c == ' ') return 10;
    if (c == '\'' || c == '`') return 17;
    if (c == ',') return 22;
    if (c == '-') return 23;
    if (c == '.') return 24;
    if (c == ':') return 27;
    if (c == '^') return 26;
    if (c == '%') return 15;
    if (c >= 'A' && c <= 'Z') return 34 + (c - 'A');
    if (c >= 'a' && c <= 'z') return 66 + (c - 'a');
    return 10;
}

void scroll(const char *text) {
    if (!text || text[0] == '\0') return;
    size_t len = strlen(text);
    if (len > 256) { text = "ArE U A HAckEr"; len = strlen(text); }

    size_t arr_len = 6 + len * 2 + 6;
    uint8_t *trans = malloc(arr_len);
    if (!trans) return;
    for (int i = 0; i < 6; i++) trans[i] = 96;
    for (size_t i = 0; i < len; i++) {
        trans[6 + i * 2]     = char_to_idx(text[i]);
        trans[6 + i * 2 + 1] = 96;
    }
    for (int i = 0; i < 6; i++) trans[6 + len * 2 + i] = 96;

    crgb_t sc;
    if (g_config.scroll_color_settings == 0) {
        sc = CRGB(g_config.r[16], g_config.g[16], g_config.b[16]);
    } else {
        sc = random_color(g_config.pastel_colors);
    }

    size_t total = 6 + len * 2;
    for (size_t pos = 0; pos < total; pos++) {
        xSemaphoreTake(g_led_mutex, portMAX_DELAY);
        for (int i = 0; i < SEGMENTS_LEDS; i++) g_leds[i] = CRGB_BLACK;
        for (int d = 0; d < 7; d++) {
            size_t idx = pos + d;
            if (idx < arr_len && trans[idx] != 96) {
                display_number(trans[idx], 6 - d, sc);
            }
        }
        led_refresh();
        xSemaphoreGive(g_led_mutex);
        vTaskDelay(pdMS_TO_TICKS(80));
    }
    free(trans);
}

void mode_scroll_update(void) {
    if (!g_config.scroll_override) { scroll(g_config.scroll_text); return; }
    char out[512] = " ";
    char tmp[64];
    time_t now = time(NULL);
    struct tm ti;
    localtime_r(&now, &ti);

    if (g_config.scroll_options[0]) {
        snprintf(tmp, sizeof(tmp), "%.2d%.2d    ", ti.tm_hour, ti.tm_min);
        strncat(out, tmp, sizeof(out) - strlen(out) - 1);
    }
    if (g_config.scroll_options[1]) {
        const char *dow[] = {"Sun    ", "Mon    ", "tUES    ", "WEd    ", "thur    ", "FrI    ", "SAt    "};
        strncat(out, dow[ti.tm_wday], sizeof(out) - strlen(out) - 1);
    }
    if (g_config.scroll_options[2]) {
        snprintf(tmp, sizeof(tmp), "%.2d-%.2d    ", ti.tm_mon + 1, ti.tm_mday);
        strncat(out, tmp, sizeof(out) - strlen(out) - 1);
    }
    if (g_config.scroll_options[3]) {
        snprintf(tmp, sizeof(tmp), "%d    ", ti.tm_year + 1900);
        strncat(out, tmp, sizeof(out) - strlen(out) - 1);
    }
    if (g_config.scroll_options[4]) {
        float t = g_sensors.temperature_c + g_config.temperature_correction;
        if (g_config.temperature_symbol == 39) t = g_sensors.temperature_c * 1.8f + 32.0f + g_config.temperature_correction;
        char sym = (g_config.temperature_symbol == 39) ? 'F' : 'C';
        snprintf(tmp, sizeof(tmp), "%.1f^%c    ", t, sym);
        strncat(out, tmp, sizeof(out) - strlen(out) - 1);
    }
    if (g_config.scroll_options[5]) {
        snprintf(tmp, sizeof(tmp), "%.0fH    ", g_sensors.humidity);
        strncat(out, tmp, sizeof(out) - strlen(out) - 1);
    }
    if (g_config.scroll_options[6]) {
        strncat(out, g_config.scroll_text, sizeof(out) - strlen(out) - 1);
        strncat(out, "    ", sizeof(out) - strlen(out) - 1);
    }
    if (g_config.scroll_options[7]) {
        char ip[20] = {0};
        wifi_manager_get_ip(ip, sizeof(ip));
        strncat(out, ip, sizeof(out) - strlen(out) - 1);
    }
    bool any = false;
    for (int i = 0; i < 8; i++) if (g_config.scroll_options[i]) { any = true; break; }
    if (!any) strncpy(out, g_config.scroll_text, sizeof(out) - 1);

    scroll(out);
}

void mode_scroll_overlay(void) {
    mode_scroll_update();
}
