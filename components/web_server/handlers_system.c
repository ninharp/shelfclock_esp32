#include "web_server.h"
#include "storage.h"
#include "sensors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SEND_OK(req)  do { httpd_resp_set_type(req,"text/json"); \
    httpd_resp_sendstr(req,"{\"result\":\"ok\"}"); return ESP_OK; } while(0)
#define SEND_INT(req, v) do { char _b[16]; snprintf(_b,sizeof(_b),"%d",(int)(v)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_b); return ESP_OK; } while(0)
#define SEND_HEX(req, r, g, b) do { char _c[8]; \
    snprintf(_c,sizeof(_c),"#%02X%02X%02X",(r),(g),(b)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_c); return ESP_OK; } while(0)
#define REG(s,m,u,h) do { httpd_uri_t _u={.uri=(u),.method=(m),.handler=(h),.user_ctx=NULL}; \
    httpd_register_uri_handler((s),&_u); } while(0)

static esp_err_t h_get_brightness(httpd_req_t *r)      { SEND_INT(r,g_config.brightness); }
static esp_err_t h_get_spotlights_color(httpd_req_t *r){ SEND_HEX(r,g_config.r[0],g_config.g[0],g_config.b[0]); }
static esp_err_t h_get_spotlights_set(httpd_req_t *r)  { SEND_INT(r,g_config.spotlights_color_settings); }
static esp_err_t h_get_use_spotlights(httpd_req_t *r)  { SEND_INT(r,g_config.use_spotlights); }
static esp_err_t h_get_pastel(httpd_req_t *r)          { SEND_INT(r,g_config.pastel_colors); }
static esp_err_t h_get_ccfreq(httpd_req_t *r)          { SEND_INT(r,g_config.color_change_frequency); }
static esp_err_t h_get_suspend_type(httpd_req_t *r)    { SEND_INT(r,g_config.suspend_type); }
static esp_err_t h_get_suspend_freq(httpd_req_t *r)    { SEND_INT(r,g_config.suspend_frequency); }

static esp_err_t h_update_brightness(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"rangeBrightness",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.brightness=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_spotlights_color(httpd_req_t *r) {
    char hex[8]={0}; get_body_param(r,"spotlightsColor",hex,sizeof(hex));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    parse_hex_color(hex,&g_config.r[0],&g_config.g[0],&g_config.b[0]);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_spotlights_set(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"spotlightsColorSettings",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.spotlights_color_settings=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_use_spotlights(httpd_req_t *r) {
    char buf[4]={0}; get_body_param(r,"useSpotlights",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.use_spotlights=atoi(buf)?1:0;
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_pastel(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"pastelColors",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.pastel_colors=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_ccfreq(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"ColorChangeFrequency",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.color_change_frequency=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_suspend_type(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"suspendType",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.suspend_type=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_suspend_freq(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"suspendFrequency",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY); g_config.suspend_frequency=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

static esp_err_t h_debugpage(httpd_req_t *r) {
    char buf[1024];
    time_t now = time(NULL);
    struct tm ti; localtime_r(&now, &ti);
    struct tm rtci; ds3231_get_time(&rtci);
    snprintf(buf, sizeof(buf),
        "<html><body><pre>"
        "ESP32 Time: %04d-%02d-%02d %02d:%02d:%02d\n"
        "DS3231 Time: %04d-%02d-%02d %02d:%02d:%02d\n"
        "Temp: %.1f C / %.1f F  Humi: %.1f%%\n"
        "Light: %d  Audio: %d\n"
        "Mode: %d  Brightness: %d\n"
        "</pre></body></html>",
        ti.tm_year+1900, ti.tm_mon+1, ti.tm_mday,
        ti.tm_hour, ti.tm_min, ti.tm_sec,
        rtci.tm_year+1900, rtci.tm_mon+1, rtci.tm_mday,
        rtci.tm_hour, rtci.tm_min, rtci.tm_sec,
        g_sensors.temperature_c, g_sensors.temperature_f, g_sensors.humidity,
        (int)g_sensors.light_level, (int)g_sensors.audio_level,
        (int)g_config.clock_mode, (int)g_config.brightness);
    httpd_resp_set_type(r,"text/html");
    httpd_resp_sendstr(r,buf);
    return ESP_OK;
}

void register_handlers_system(httpd_handle_t s) {
    REG(s,HTTP_GET, "/getrangeBrightness",           h_get_brightness);
    REG(s,HTTP_GET, "/getspotlightsColor",           h_get_spotlights_color);
    REG(s,HTTP_GET, "/getspotlightsColorSettings",   h_get_spotlights_set);
    REG(s,HTTP_GET, "/getuseSpotlights",             h_get_use_spotlights);
    REG(s,HTTP_GET, "/getpastelColors",              h_get_pastel);
    REG(s,HTTP_GET, "/getColorChangeFrequency",      h_get_ccfreq);
    REG(s,HTTP_GET, "/getsuspendType",               h_get_suspend_type);
    REG(s,HTTP_GET, "/getsuspendFrequency",          h_get_suspend_freq);
    REG(s,HTTP_POST,"/updaterangeBrightness",        h_update_brightness);
    REG(s,HTTP_POST,"/updatespotlightsColor",        h_update_spotlights_color);
    REG(s,HTTP_POST,"/updatespotlightsColorSettings",h_update_spotlights_set);
    REG(s,HTTP_POST,"/updateuseSpotlights",          h_update_use_spotlights);
    REG(s,HTTP_POST,"/updatePastelColors",           h_update_pastel);
    REG(s,HTTP_POST,"/updateColorChangeFrequency",   h_update_ccfreq);
    REG(s,HTTP_POST,"/updatesuspendType",            h_update_suspend_type);
    REG(s,HTTP_POST,"/updatesuspendFrequency",       h_update_suspend_freq);
    REG(s,HTTP_GET, "/debugpage",                    h_debugpage);
}
