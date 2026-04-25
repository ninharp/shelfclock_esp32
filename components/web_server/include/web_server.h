#pragma once
#include "esp_http_server.h"

extern httpd_handle_t g_httpd;

void web_server_init(void);

void register_handlers_clock(httpd_handle_t server);
void register_handlers_date_temp_humi(httpd_handle_t server);
void register_handlers_scores_cd_ls(httpd_handle_t server);
void register_handlers_scroll_spec(httpd_handle_t server);
void register_handlers_system(httpd_handle_t server);

esp_err_t get_body_param(httpd_req_t *req, const char *key,
                         char *val, size_t val_len);
void      parse_hex_color(const char *hex, uint8_t *r, uint8_t *g, uint8_t *b);
