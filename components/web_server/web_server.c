#include "web_server.h"
#include "storage.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "web_server";
httpd_handle_t g_httpd = NULL;

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

static esp_err_t static_file_handler(httpd_req_t *req) {
    char path[640];
    snprintf(path, sizeof(path), "/spiffs%s",
             strcmp(req->uri, "/") == 0 ? "/index.html" : req->uri);

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

static esp_ota_handle_t s_ota_handle = 0;
static const esp_partition_t *s_ota_part = NULL;

static esp_err_t ota_post_handler(httpd_req_t *req) {
    char buf[1024];
    int remaining = (int)req->content_len;
    bool started = false;
    esp_err_t err = ESP_OK;

    while (remaining > 0) {
        int recv_len = remaining < (int)sizeof(buf) ? remaining : (int)sizeof(buf);
        int got = httpd_req_recv(req, buf, recv_len);
        if (got <= 0) { err = ESP_FAIL; break; }
        if (!started) {
            s_ota_part = esp_ota_get_next_update_partition(NULL);
            err = esp_ota_begin(s_ota_part, OTA_WITH_SEQUENTIAL_WRITES, &s_ota_handle);
            if (err != ESP_OK) break;
            started = true;
        }
        err = esp_ota_write(s_ota_handle, buf, got);
        if (err != ESP_OK) break;
        remaining -= got;
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

esp_err_t get_body_param(httpd_req_t *req, const char *key,
                          char *val, size_t val_len) {
    char body[512] = {0};
    size_t to_read = req->content_len < sizeof(body)-1 ? req->content_len : sizeof(body)-1;
    int n = httpd_req_recv(req, body, to_read);
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

void web_server_init(void) {
    spiffs_init();

    mdns_init();
    mdns_hostname_set("shelfclock");
    mdns_instance_name_set("ShelfClock");
    ESP_LOGI(TAG, "mDNS: http://shelfclock.local");

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 120;
    cfg.stack_size       = 8192;
    cfg.lru_purge_enable = true;
    cfg.uri_match_fn     = httpd_uri_match_wildcard;

    ESP_ERROR_CHECK(httpd_start(&g_httpd, &cfg));

    httpd_uri_t ota = {
        .uri     = "/update",
        .method  = HTTP_POST,
        .handler = ota_post_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(g_httpd, &ota);

    register_handlers_clock(g_httpd);
    register_handlers_date_temp_humi(g_httpd);
    register_handlers_scores_cd_ls(g_httpd);
    register_handlers_scroll_spec(g_httpd);
    register_handlers_system(g_httpd);

    /* Wildcard for static files — MUST be registered last */
    httpd_uri_t static_get = {
        .uri     = "/*",
        .method  = HTTP_GET,
        .handler = static_file_handler,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(g_httpd, &static_get);

    ESP_LOGI(TAG, "HTTP server started");
}
