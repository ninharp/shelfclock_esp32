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
#include <stdio.h>

static const char *TAG = "captive_portal";

static const char PORTAL_HTML[] =
    "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ShelfClock Setup</title>"
    "<style>"
    "body{font-family:sans-serif;max-width:400px;margin:30px auto;padding:16px;background:#f0f0f0}"
    ".card{background:#fff;border-radius:8px;padding:20px;box-shadow:0 2px 6px rgba(0,0,0,.15)}"
    "h2{margin:0 0 16px;color:#333}"
    "label{display:block;font-size:.85em;color:#666;margin:12px 0 3px}"
    "select,input{width:100%;padding:8px;box-sizing:border-box;border:1px solid #ccc;"
    "border-radius:4px;font-size:.95em;margin-bottom:2px}"
    ".btn{display:block;width:100%;padding:11px;margin-top:14px;"
    "background:#1976D2;color:#fff;border:none;border-radius:4px;"
    "font-size:1em;cursor:pointer}"
    ".btn:disabled{background:#90CAF9;cursor:default}"
    "#st{margin-top:10px;font-size:.9em;color:#555;text-align:center;min-height:1.2em}"
    "</style></head>"
    "<body><div class='card'>"
    "<h2>ShelfClock WiFi</h2>"
    "<label>Netzwerk</label>"
    "<select id='net' onchange=\"document.getElementById('ssid').value=this.value\">"
    "<option value=''>-- wird gescannt --</option>"
    "</select>"
    "<input id='ssid' placeholder='SSID manuell eingeben'>"
    "<label>Passwort</label>"
    "<input id='pass' type='password' placeholder='(leer lassen falls offen)'>"
    "<button class='btn' id='btn' onclick='doSave()'>Verbinden</button>"
    "<div id='st'></div>"
    "</div>"
    "<script>"
    "function setst(t){document.getElementById('st').textContent=t}"
    "function doScan(){"
    "setst('Suche Netzwerke...');"
    "fetch('/scan').then(function(r){return r.json()}).then(function(d){"
    "var sel=document.getElementById('net');"
    "sel.innerHTML='';"
    "if(!d||!d.length){"
    "sel.innerHTML='<option value=\"\">Keine gefunden</option>';"
    "setst('Kein Netzwerk gefunden');return;}"
    "d.forEach(function(n){"
    "var o=document.createElement('option');"
    "o.value=n.s;"
    "o.textContent=n.s+' ('+n.r+' dBm)';"
    "sel.appendChild(o);});"
    "document.getElementById('ssid').value=d[0].s;"
    "setst(d.length+' Netzwerk(e) gefunden');"
    "}).catch(function(){setst('Scan fehlgeschlagen')})}"
    "function doSave(){"
    "var s=document.getElementById('ssid').value.trim();"
    "if(!s){setst('Bitte SSID eingeben!');return;}"
    "var btn=document.getElementById('btn');"
    "btn.disabled=true;setst('Speichere...');"
    "var b='ssid='+encodeURIComponent(s)+'&pass='+encodeURIComponent(document.getElementById('pass').value);"
    "fetch('/save',{method:'POST',body:b,"
    "headers:{'Content-Type':'application/x-www-form-urlencoded'}})"
    ".then(function(r){return r.text()}).then(function(t){setst(t)})"
    ".catch(function(){setst('Gespeichert! Neustart laeuft...')})}"
    "doScan();"
    "</script></body></html>";

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

static void restart_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(1200));
    esp_restart();
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
        size_t len = strlen(p);
        while (len > 0 && ((unsigned char)p[len-1] < 0x20)) len--;
        if (len >= sizeof(raw_pass)) len = sizeof(raw_pass) - 1;
        strncpy(raw_pass, p, len);
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
        ESP_LOGI(TAG, "Credentials saved for SSID: %s", ssid);
    } else {
        ESP_LOGE(TAG, "NVS write failed");
        httpd_resp_sendstr(req, "Fehler beim Speichern!");
        return ESP_OK;
    }

    httpd_resp_sendstr(req, "Gespeichert! Verbinde mit Netzwerk...");
    xTaskCreate(restart_task, "restart", 1024, NULL, 5, NULL);
    return ESP_OK;
}

static esp_err_t scan_handler(httpd_req_t *req) {
    wifi_scan_config_t scan_cfg = {
        .ssid        = NULL,
        .bssid       = NULL,
        .channel     = 0,
        .show_hidden = false,
    };
    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true);
    if (err != ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count > 20) ap_count = 20;

    wifi_ap_record_t *records = calloc(ap_count, sizeof(wifi_ap_record_t));
    if (!records) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }
    esp_wifi_scan_get_ap_records(&ap_count, records);

    /* JSON: [{"s":"SSID","r":-70}, ...] — max ~50 chars per entry */
    char *json = malloc(ap_count * 60 + 8);
    if (!json) {
        free(records);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }

    int pos = 0;
    pos += sprintf(json + pos, "[");
    bool first = true;
    for (int i = 0; i < ap_count; i++) {
        const char *ssid = (const char *)records[i].ssid;
        if (!*ssid) continue;
        bool safe = true;
        for (const char *c = ssid; *c; c++) {
            if (*c == '"' || *c == '\\' || (unsigned char)*c < 0x20) {
                safe = false; break;
            }
        }
        if (!safe) continue;
        if (!first) pos += sprintf(json + pos, ",");
        pos += sprintf(json + pos, "{\"s\":\"%s\",\"r\":%d}", ssid, records[i].rssi);
        first = false;
    }
    pos += sprintf(json + pos, "]");
    free(records);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    free(json);
    return ESP_OK;
}

void captive_portal_start(void) {
    /* WiFi already initialized by wifi_manager_init() */
    esp_netif_create_default_wifi_ap();

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid           = "ShelfClock-Setup",
            .ssid_len       = 16,
            .channel        = 6,
            .max_connection = 4,
            .authmode       = WIFI_AUTH_OPEN,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "SoftAP 'ShelfClock-Setup' gestartet @ 192.168.4.1");

    httpd_handle_t server = NULL;
    httpd_config_t hcfg = HTTPD_DEFAULT_CONFIG();
    hcfg.lru_purge_enable = true;
    hcfg.recv_wait_timeout = 30;
    hcfg.send_wait_timeout = 30;
    httpd_start(&server, &hcfg);

    httpd_uri_t root_uri = { .uri = "/",     .method = HTTP_GET,  .handler = root_handler, .user_ctx = NULL };
    httpd_uri_t save_uri = { .uri = "/save", .method = HTTP_POST, .handler = save_handler, .user_ctx = NULL };
    httpd_uri_t scan_uri = { .uri = "/scan", .method = HTTP_GET,  .handler = scan_handler, .user_ctx = NULL };
    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &save_uri);
    httpd_register_uri_handler(server, &scan_uri);
}
