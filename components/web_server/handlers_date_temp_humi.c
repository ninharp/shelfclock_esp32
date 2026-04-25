#include "web_server.h"
#include "storage.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdlib.h>
#include <string.h>

#define SEND_OK(req)  do { httpd_resp_set_type(req,"text/json"); \
    httpd_resp_sendstr(req,"{\"result\":\"ok\"}"); return ESP_OK; } while(0)
#define SEND_INT(req, v) do { char _b[16]; snprintf(_b,sizeof(_b),"%d",(int)(v)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_b); return ESP_OK; } while(0)
#define SEND_HEX(req, r, g, b) do { char _c[8]; \
    snprintf(_c,sizeof(_c),"#%02X%02X%02X",(r),(g),(b)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_c); return ESP_OK; } while(0)
#define REG(s,m,u,h) do { httpd_uri_t _u={.uri=(u),.method=(m),.handler=(h),.user_ctx=NULL}; \
    httpd_register_uri_handler((s),&_u); } while(0)

static esp_err_t h_get_date_display_type(httpd_req_t *r) { SEND_INT(r,g_config.date_display_type); }
static esp_err_t h_get_date_color_settings(httpd_req_t *r){ SEND_INT(r,g_config.date_color_settings); }
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

static esp_err_t h_get_temp_symbol(httpd_req_t *r)        { SEND_INT(r,g_config.temperature_symbol); }
static esp_err_t h_get_temp_correction(httpd_req_t *r)    { SEND_INT(r,g_config.temperature_correction); }
static esp_err_t h_get_temp_display_type(httpd_req_t *r)  { SEND_INT(r,g_config.temp_display_type); }
static esp_err_t h_get_temp_color_settings(httpd_req_t *r){ SEND_INT(r,g_config.temp_color_settings); }
static esp_err_t h_get_temp_color(httpd_req_t *r)   { SEND_HEX(r,g_config.r[7],g_config.g[7],g_config.b[7]); }
static esp_err_t h_get_type_color(httpd_req_t *r)   { SEND_HEX(r,g_config.r[8],g_config.g[8],g_config.b[8]); }
static esp_err_t h_get_degree_color(httpd_req_t *r) { SEND_HEX(r,g_config.r[9],g_config.g[9],g_config.b[9]); }

static esp_err_t h_update_temp_type(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"temperatureSymbol",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.temperature_symbol=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_temp_correction(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"temperatureCorrection",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.temperature_correction=(int8_t)atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_temp_display_type(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"tempDisplayType",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.temp_display_type=atoi(buf);
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
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.temp_color_settings=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

static esp_err_t h_get_humi_display_type(httpd_req_t *r)   { SEND_INT(r,g_config.humi_display_type); }
static esp_err_t h_get_humi_color_settings(httpd_req_t *r) { SEND_INT(r,g_config.humi_color_settings); }
static esp_err_t h_get_humi_color(httpd_req_t *r)          { SEND_HEX(r,g_config.r[10],g_config.g[10],g_config.b[10]); }
static esp_err_t h_get_symbol_color(httpd_req_t *r)        { SEND_HEX(r,g_config.r[11],g_config.g[11],g_config.b[11]); }
static esp_err_t h_get_humi_decimal_color(httpd_req_t *r)  { SEND_HEX(r,g_config.r[12],g_config.g[12],g_config.b[12]); }

static esp_err_t h_update_humi_display_type(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"humiDisplayType",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.humi_display_type=atoi(buf);
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
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.humi_color_settings=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

void register_handlers_date_temp_humi(httpd_handle_t s) {
    REG(s,HTTP_GET, "/getDateDisplayType",     h_get_date_display_type);
    REG(s,HTTP_GET, "/getDateColorSettings",   h_get_date_color_settings);
    REG(s,HTTP_GET, "/getdayColor",            h_get_day_color);
    REG(s,HTTP_GET, "/getmonthColor",          h_get_month_color);
    REG(s,HTTP_GET, "/getseparatorColor",      h_get_sep_color);
    REG(s,HTTP_POST,"/updateDateDisplayType",  h_update_date_display_type);
    REG(s,HTTP_POST,"/updatedayColor",         h_update_day_color);
    REG(s,HTTP_POST,"/updatemonthColor",       h_update_month_color);
    REG(s,HTTP_POST,"/updateseparatorColor",   h_update_sep_color);
    REG(s,HTTP_POST,"/updateDateColorSettings",h_update_date_color_settings);
    REG(s,HTTP_GET, "/gettemperatureSymbol",      h_get_temp_symbol);
    REG(s,HTTP_GET, "/gettemperatureCorrection",  h_get_temp_correction);
    REG(s,HTTP_GET, "/gettempDisplayType",        h_get_temp_display_type);
    REG(s,HTTP_GET, "/gettempColorSettings",      h_get_temp_color_settings);
    REG(s,HTTP_GET, "/gettempColor",              h_get_temp_color);
    REG(s,HTTP_GET, "/gettypeColor",              h_get_type_color);
    REG(s,HTTP_GET, "/getdegreeColor",            h_get_degree_color);
    REG(s,HTTP_POST,"/updateTempType",            h_update_temp_type);
    REG(s,HTTP_POST,"/updateCorrectionSelect",    h_update_temp_correction);
    REG(s,HTTP_POST,"/updateTempDisplayType",     h_update_temp_display_type);
    REG(s,HTTP_POST,"/updateTempColor",           h_update_temp_color);
    REG(s,HTTP_POST,"/updateTypeColor",           h_update_type_color);
    REG(s,HTTP_POST,"/updateDegreeColor",         h_update_degree_color);
    REG(s,HTTP_POST,"/updateTempColorSettings",   h_update_temp_color_settings);
    REG(s,HTTP_GET, "/gethumiDisplayType",        h_get_humi_display_type);
    REG(s,HTTP_GET, "/gethumiColorSettings",      h_get_humi_color_settings);
    REG(s,HTTP_GET, "/gethumiColor",              h_get_humi_color);
    REG(s,HTTP_GET, "/getsymbolColor",            h_get_symbol_color);
    REG(s,HTTP_GET, "/gethumiDecimalColor",       h_get_humi_decimal_color);
    REG(s,HTTP_POST,"/updateHumiDisplayType",     h_update_humi_display_type);
    REG(s,HTTP_POST,"/updateHumiColor",           h_update_humi_color);
    REG(s,HTTP_POST,"/updateSymbolColor",         h_update_symbol_color);
    REG(s,HTTP_POST,"/updateHumiDecimalColor",    h_update_humi_decimal_color);
    REG(s,HTTP_POST,"/updateHumiColorSettings",   h_update_humi_color_settings);
}
