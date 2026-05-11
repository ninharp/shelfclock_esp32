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
#define SEND_RGBA(req, r, g, b) do { char _c[32]; \
    snprintf(_c,sizeof(_c),"rgba(%d,%d,%d,1)",(r),(g),(b)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_c); return ESP_OK; } while(0)
#define REG(s,m,u,h) do { httpd_uri_t _u={.uri=(u),.method=(m),.handler=(h),.user_ctx=NULL}; \
    httpd_register_uri_handler((s),&_u); } while(0)

static esp_err_t h_get_sb_left(httpd_req_t *r)      { SEND_HEX(r,g_config.r[13],g_config.g[13],g_config.b[13]); }
static esp_err_t h_get_sb_right(httpd_req_t *r)     { SEND_HEX(r,g_config.r[14],g_config.g[14],g_config.b[14]); }
static esp_err_t h_get_sb_left_rgb(httpd_req_t *r)  { SEND_RGBA(r,g_config.r[13],g_config.g[13],g_config.b[13]); }
static esp_err_t h_get_sb_right_rgb(httpd_req_t *r) { SEND_RGBA(r,g_config.r[14],g_config.g[14],g_config.b[14]); }

static esp_err_t h_update_sb_left(httpd_req_t *r) {
    uint8_t rv,gv,bv;
    if (get_body_rgb(r,&rv,&gv,&bv)!=ESP_OK) { SEND_OK(r); }
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.r[13]=rv; g_config.g[13]=gv; g_config.b[13]=bv;
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_sb_right(httpd_req_t *r) {
    uint8_t rv,gv,bv;
    if (get_body_rgb(r,&rv,&gv,&bv)!=ESP_OK) { SEND_OK(r); }
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.r[14]=rv; g_config.g[14]=gv; g_config.b[14]=bv;
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

static esp_err_t h_get_cd_color(httpd_req_t *r)       { SEND_HEX(r,g_config.cd_r,g_config.cd_g,g_config.cd_b); }
static esp_err_t h_get_cd_colorchange(httpd_req_t *r) { SEND_INT(r,g_config.color_change_cd); }
static esp_err_t h_get_cd_alarm(httpd_req_t *r)       { SEND_INT(r,g_config.use_audible_alarm); }

static esp_err_t h_update_cd_color(httpd_req_t *r) {
    uint8_t rv,gv,bv;
    if (get_body_rgb(r,&rv,&gv,&bv)!=ESP_OK) { SEND_OK(r); }
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.cd_r=rv; g_config.cd_g=gv; g_config.cd_b=bv;
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_cd_colorchange(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"colorchangeCD",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.color_change_cd=parse_bool_str(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_cd_alarm(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"alarmCD",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.use_audible_alarm=parse_bool_str(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

void register_handlers_scores_cd_ls(httpd_handle_t s) {
    REG(s,HTTP_GET, "/getscoreboardColorLeft",     h_get_sb_left);
    REG(s,HTTP_GET, "/getscoreboardColorRight",    h_get_sb_right);
    REG(s,HTTP_GET, "/getscoreboardColorLeftRGB",  h_get_sb_left_rgb);
    REG(s,HTTP_GET, "/getscoreboardColorRightRGB", h_get_sb_right_rgb);
    REG(s,HTTP_POST,"/updatescoreboardColorLeft",  h_update_sb_left);
    REG(s,HTTP_POST,"/updatescoreboardColorRight", h_update_sb_right);
    REG(s,HTTP_GET, "/getcolorCD",          h_get_cd_color);
    REG(s,HTTP_GET, "/getcolorchangeCD",    h_get_cd_colorchange);
    REG(s,HTTP_GET, "/getalarmCD",          h_get_cd_alarm);
    REG(s,HTTP_POST,"/updatecolorCD",       h_update_cd_color);
    REG(s,HTTP_POST,"/updatecolorchangeCD", h_update_cd_colorchange);
    REG(s,HTTP_POST,"/updatealarmCD",       h_update_cd_alarm);
}
