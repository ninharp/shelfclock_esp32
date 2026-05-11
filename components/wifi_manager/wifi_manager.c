#include "wifi_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
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
            esp_wifi_connect();
            s_retry++;
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
    if (nvs_get_str(h, "ssid", ssid, &len) != ESP_OK) {
        nvs_close(h);
        return false;
    }
    len = sizeof(pass);
    nvs_get_str(h, "pass", pass, &len);
    nvs_close(h);
    if (strlen(ssid) == 0) return false;

    s_sta_netif = esp_netif_create_default_wifi_sta();
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

    if (!(bits & WIFI_CONNECTED_BIT)) {
        esp_wifi_stop();
    }
    return (bits & WIFI_CONNECTED_BIT) != 0;
}

bool wifi_manager_init(void) {
    s_events = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t wicfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wicfg));

    if (try_connect_from_nvs()) {
        ESP_LOGI(TAG, "WiFi connected");
        return true;
    }
    ESP_LOGW(TAG, "No credentials or connect failed -- starting SoftAP");
    captive_portal_start();
    return false;
}

bool wifi_manager_is_connected(void) { return s_connected; }

void wifi_manager_get_ip(char *buf, size_t len) {
    if (!s_sta_netif) {
        strncpy(buf, "0.0.0.0", len);
        return;
    }
    esp_netif_ip_info_t info;
    esp_netif_get_ip_info(s_sta_netif, &info);
    snprintf(buf, len, IPSTR, IP2STR(&info.ip));
}
