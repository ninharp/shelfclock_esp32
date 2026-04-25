#include "web_server.h"
#include "storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SEND_OK(req)  do { httpd_resp_set_type(req,"text/json"); \
    httpd_resp_sendstr(req,"{\"result\":\"ok\"}"); return ESP_OK; } while(0)
#define SEND_INT(req, v) do { char _b[16]; snprintf(_b,sizeof(_b),"%d",(int)(v)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_b); return ESP_OK; } while(0)
#define SEND_HEX(req, r, g, b) do { char _c[8]; \
    snprintf(_c,sizeof(_c),"#%02X%02X%02X",(r),(g),(b)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_c); return ESP_OK; } while(0)
#define REG(s,m,u,h) do { httpd_uri_t _u={.uri=(u),.method=(m),.handler=(h),.user_ctx=NULL}; \
    httpd_register_uri_handler((s),&_u); } while(0)

static esp_err_t h_get_scroll_freq(httpd_req_t *r)     { SEND_INT(r,g_config.scroll_frequency); }
static esp_err_t h_get_scroll_override(httpd_req_t *r) { SEND_INT(r,g_config.scroll_override); }
static esp_err_t h_get_scroll_color(httpd_req_t *r)    { SEND_HEX(r,g_config.r[16],g_config.g[16],g_config.b[16]); }
static esp_err_t h_get_scroll_color_set(httpd_req_t *r){ SEND_INT(r,g_config.scroll_color_settings); }
static esp_err_t h_get_scroll_text(httpd_req_t *r) {
    httpd_resp_set_type(r,"text/plain");
    httpd_resp_sendstr(r,g_config.scroll_text);
    return ESP_OK;
}
static esp_err_t h_get_scroll_opt(httpd_req_t *r) {
    int idx = (int)(r->uri[strlen(r->uri)-1] - '1');
    if (idx < 0 || idx > 7) { httpd_resp_send_404(r); return ESP_FAIL; }
    SEND_INT(r,g_config.scroll_options[idx]);
}
static esp_err_t h_update_scroll_freq(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"scrollFrequency",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.scroll_frequency=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_scroll_override(httpd_req_t *r) {
    char buf[4]={0}; get_body_param(r,"scrollOverride",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.scroll_override=atoi(buf)?1:0;
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
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.scroll_color_settings=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_scroll_opt(httpd_req_t *r) {
    int idx = (int)(r->uri[strlen(r->uri)-1] - '1');
    if (idx < 0 || idx > 7) { httpd_resp_send_404(r); return ESP_FAIL; }
    char buf[4]={0};
    char key[18]; snprintf(key,sizeof(key),"scrollOptions%d",idx+1);
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

static esp_err_t h_get_rand_spec(httpd_req_t *r)      { SEND_INT(r,g_config.random_spectrum_mode); }
static esp_err_t h_get_spec_color(httpd_req_t *r)     { SEND_HEX(r,g_config.r[15],g_config.g[15],g_config.b[15]); }
static esp_err_t h_get_spec_bg(httpd_req_t *r)        { SEND_HEX(r,g_config.r[17],g_config.g[17],g_config.b[17]); }
static esp_err_t h_get_spec_color_set(httpd_req_t *r) { SEND_INT(r,g_config.spectrum_color_settings); }
static esp_err_t h_get_spec_bg_set(httpd_req_t *r)    { SEND_INT(r,g_config.spectrum_background_settings); }
static esp_err_t h_update_rand_spec(httpd_req_t *r) {
    char buf[4]={0}; get_body_param(r,"randomSpectrumMode",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.random_spectrum_mode=atoi(buf)?1:0;
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
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.spectrum_color_settings=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_spec_bg_set(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"spectrumBackgroundSettings",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.spectrum_background_settings=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

void register_handlers_scroll_spec(httpd_handle_t s) {
    REG(s,HTTP_GET, "/getscrollFrequency",       h_get_scroll_freq);
    REG(s,HTTP_GET, "/getscrollOverride",         h_get_scroll_override);
    REG(s,HTTP_GET, "/getscrollColor",            h_get_scroll_color);
    REG(s,HTTP_GET, "/getscrollColorSettings",    h_get_scroll_color_set);
    REG(s,HTTP_GET, "/getscrollText",             h_get_scroll_text);
    REG(s,HTTP_GET, "/getscrollOptions1",         h_get_scroll_opt);
    REG(s,HTTP_GET, "/getscrollOptions2",         h_get_scroll_opt);
    REG(s,HTTP_GET, "/getscrollOptions3",         h_get_scroll_opt);
    REG(s,HTTP_GET, "/getscrollOptions4",         h_get_scroll_opt);
    REG(s,HTTP_GET, "/getscrollOptions5",         h_get_scroll_opt);
    REG(s,HTTP_GET, "/getscrollOptions6",         h_get_scroll_opt);
    REG(s,HTTP_GET, "/getscrollOptions7",         h_get_scroll_opt);
    REG(s,HTTP_GET, "/getscrollOptions8",         h_get_scroll_opt);
    REG(s,HTTP_POST,"/updatescrollFrequency",     h_update_scroll_freq);
    REG(s,HTTP_POST,"/updatescrollOverride",      h_update_scroll_override);
    REG(s,HTTP_POST,"/updatescrollColor",         h_update_scroll_color);
    REG(s,HTTP_POST,"/updatescrollColorSettings", h_update_scroll_color_set);
    REG(s,HTTP_POST,"/updatescrollText",          h_update_scroll_text);
    REG(s,HTTP_POST,"/updatescrollOptions1",      h_update_scroll_opt);
    REG(s,HTTP_POST,"/updatescrollOptions2",      h_update_scroll_opt);
    REG(s,HTTP_POST,"/updatescrollOptions3",      h_update_scroll_opt);
    REG(s,HTTP_POST,"/updatescrollOptions4",      h_update_scroll_opt);
    REG(s,HTTP_POST,"/updatescrollOptions5",      h_update_scroll_opt);
    REG(s,HTTP_POST,"/updatescrollOptions6",      h_update_scroll_opt);
    REG(s,HTTP_POST,"/updatescrollOptions7",      h_update_scroll_opt);
    REG(s,HTTP_POST,"/updatescrollOptions8",      h_update_scroll_opt);
    REG(s,HTTP_GET, "/getrandomSpectrumMode",           h_get_rand_spec);
    REG(s,HTTP_GET, "/getspectrumColor",                h_get_spec_color);
    REG(s,HTTP_GET, "/getspectrumBackground",           h_get_spec_bg);
    REG(s,HTTP_GET, "/getspectrumColorSettings",        h_get_spec_color_set);
    REG(s,HTTP_GET, "/getspectrumBackgroundSettings",   h_get_spec_bg_set);
    REG(s,HTTP_POST,"/updaterandomSpectrumMode",        h_update_rand_spec);
    REG(s,HTTP_POST,"/updatespectrumColor",             h_update_spec_color);
    REG(s,HTTP_POST,"/updatespectrumBackground",        h_update_spec_bg);
    REG(s,HTTP_POST,"/updatespectrumColorSettings",     h_update_spec_color_set);
    REG(s,HTTP_POST,"/updatespectrumBackgroundSettings",h_update_spec_bg_set);
}
