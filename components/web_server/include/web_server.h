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
esp_err_t get_body_rgb(httpd_req_t *req, uint8_t *r, uint8_t *g, uint8_t *b);

/* Parst "true"/"1" → 1, alles andere → 0 (jQuery sendet Booleans als "true"/"false"). */
static inline int parse_bool_str(const char *s) {
    return (s[0] == '1' || s[0] == 't' || s[0] == 'T');
}
