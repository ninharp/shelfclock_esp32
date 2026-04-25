#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
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
        if (len >= sizeof(raw_ssid)) len = sizeof(raw_ssid) - 1;
        strncpy(raw_ssid, s, len);
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
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid            = "ShelfClock-Setup",
            .ssid_len        = 16,
            .channel         = 1,
            .max_connection  = 4,
            .authmode        = WIFI_AUTH_OPEN,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();
    ESP_LOGI(TAG, "SoftAP 'ShelfClock-Setup' started @ 192.168.4.1");

    httpd_handle_t server = NULL;
    httpd_config_t hcfg = HTTPD_DEFAULT_CONFIG();
    hcfg.lru_purge_enable = true;
    httpd_start(&server, &hcfg);

    httpd_uri_t root = { .uri="/",     .method=HTTP_GET,  .handler=root_handler, .user_ctx=NULL };
    httpd_uri_t save = { .uri="/save", .method=HTTP_POST, .handler=save_handler, .user_ctx=NULL };
    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &save);
}
