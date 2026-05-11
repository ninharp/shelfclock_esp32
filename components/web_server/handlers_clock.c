#include "web_server.h"
#include "storage.h"
#include "led_display.h"
#include "clock_modes.h"
#include "rtttl_player.h"
#include "sensors.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define SEND_OK(req)  do { httpd_resp_set_type(req,"text/json"); \
    httpd_resp_sendstr(req,"{\"result\":\"ok\"}"); return ESP_OK; } while(0)
#define SEND_INT(req, v) do { char _b[16]; snprintf(_b,sizeof(_b),"%d",(int)(v)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_b); return ESP_OK; } while(0)
#define SEND_HEX(req, r, g, b) do { char _c[8]; \
    snprintf(_c,sizeof(_c),"#%02X%02X%02X",(r),(g),(b)); \
    httpd_resp_set_type(req,"text/plain"); httpd_resp_sendstr(req,_c); return ESP_OK; } while(0)
#define REG(s, m, u, h) do { httpd_uri_t _u={.uri=(u),.method=(m),.handler=(h),.user_ctx=NULL}; \
    httpd_register_uri_handler((s),&_u); } while(0)

static esp_err_t h_go_clock(httpd_req_t *r) {
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 0; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    storage_save_all(); SEND_OK(r);
}
static esp_err_t h_go_temperature(httpd_req_t *r) {
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 2; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    storage_save_all(); SEND_OK(r);
}
static esp_err_t h_go_date(httpd_req_t *r) {
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 7; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    storage_save_all(); SEND_OK(r);
}
static esp_err_t h_go_humidity(httpd_req_t *r) {
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 8; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    storage_save_all(); SEND_OK(r);
}
static esp_err_t h_go_display_off(httpd_req_t *r) {
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 10; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    rtttl_stop(); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_go_scroll(httpd_req_t *r) {
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 11; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    storage_save_all(); SEND_OK(r);
}
static esp_err_t h_go_scoreboard(httpd_req_t *r) {
    char lbuf[8]={0}, rbuf[8]={0};
    get_body_param(r, "left",  lbuf, sizeof(lbuf));
    get_body_param(r, "right", rbuf, sizeof(rbuf));
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    g_config.scoreboard_left  = atoi(lbuf);
    g_config.scoreboard_right = atoi(rbuf);
    if (g_config.scoreboard_left  < 0)  g_config.scoreboard_left  = 0;
    if (g_config.scoreboard_left  > 99) g_config.scoreboard_left  = 99;
    if (g_config.scoreboard_right < 0)  g_config.scoreboard_right = 0;
    if (g_config.scoreboard_right > 99) g_config.scoreboard_right = 99;
    all_blank(); g_config.clock_mode = 3; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    storage_save_all(); SEND_OK(r);
}
static esp_err_t h_go_countdown(httpd_req_t *r) {
    char msbuf[16]={0};
    get_body_param(r, "ms", msbuf, sizeof(msbuf));
    int32_t ms = atoi(msbuf);
    if (ms < 1000)     ms = 1000;
    if (ms > 86400000) ms = 86400000;
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 1; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    mode_countdown_start(ms); SEND_OK(r);
}
static esp_err_t h_go_stopwatch(httpd_req_t *r) {
    char msbuf[16]={0};
    get_body_param(r, "ms", msbuf, sizeof(msbuf));
    int32_t ms = atoi(msbuf);
    if (ms < 1000)     ms = 1000;
    if (ms > 86400000) ms = 86400000;
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 4; g_config.realtime_mode = 0;
    xSemaphoreGive(g_config_mutex);
    mode_stopwatch_start(ms); SEND_OK(r);
}
static esp_err_t h_go_lightshow(httpd_req_t *r) {
    char buf[8]={0};
    get_body_param(r, "lightshowMode", buf, sizeof(buf));
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 5; g_config.realtime_mode = 1;
    g_config.lightshow_mode = atoi(buf);
    xSemaphoreGive(g_config_mutex);
    storage_save_all(); SEND_OK(r);
}
static esp_err_t h_go_spectrum(httpd_req_t *r) {
    char buf[8]={0};
    get_body_param(r, "spectrumMode", buf, sizeof(buf));
    xSemaphoreTake(g_config_mutex, portMAX_DELAY);
    all_blank(); g_config.clock_mode = 9; g_config.realtime_mode = 1;
    g_config.spectrum_mode = atoi(buf);
    xSemaphoreGive(g_config_mutex);
    storage_save_all(); SEND_OK(r);
}
static esp_err_t h_get_lightshow_speed(httpd_req_t *r) { SEND_INT(r, g_config.lightshow_speed); }
static esp_err_t h_set_lightshow_speed(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"lightshowSpeed",buf,sizeof(buf));
    uint8_t v = (uint8_t)atoi(buf);
    if (v < 1) v = 1;
    if (v > 5) v = 5;
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.lightshow_speed = v;
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

static esp_err_t h_get_preset1(httpd_req_t *r) { storage_load_preset(1); SEND_OK(r); }
static esp_err_t h_get_preset2(httpd_req_t *r) { storage_load_preset(2); SEND_OK(r); }
static esp_err_t h_set_preset1(httpd_req_t *r) { storage_save_preset(1); SEND_OK(r); }
static esp_err_t h_set_preset2(httpd_req_t *r) { storage_save_preset(2); SEND_OK(r); }

static esp_err_t h_setdate(httpd_req_t *r) {
    char year[8]={0}, month[4]={0}, day[4]={0};
    char hour[4]={0}, min[4]={0}, sec[4]={0};
    get_body_param(r, "year",  year,  sizeof(year));
    get_body_param(r, "month", month, sizeof(month));
    get_body_param(r, "day",   day,   sizeof(day));
    get_body_param(r, "hour",  hour,  sizeof(hour));
    get_body_param(r, "min",   min,   sizeof(min));
    get_body_param(r, "sec",   sec,   sizeof(sec));
    struct tm tm = {0};
    tm.tm_year = atoi(year) - 1900;
    tm.tm_mon  = atoi(month) - 1;
    tm.tm_mday = atoi(day);
    tm.tm_hour = atoi(hour);
    tm.tm_min  = atoi(min);
    tm.tm_sec  = atoi(sec);
    time_t t = mktime(&tm);
    struct timeval tv = { .tv_sec = t };
    settimeofday(&tv, NULL);
    ds3231_set_time(&tm);
    SEND_OK(r);
}

static esp_err_t h_get_clock_display_type(httpd_req_t *r) { SEND_INT(r, g_config.clock_display_type); }
static esp_err_t h_get_colon_type(httpd_req_t *r)         { SEND_INT(r, g_config.colon_type); }
static esp_err_t h_get_gmt_offset(httpd_req_t *r)         { SEND_INT(r, g_config.gmt_offset_sec); }
static esp_err_t h_get_ds_time(httpd_req_t *r)            { SEND_INT(r, g_config.ds_time); }
static esp_err_t h_get_clock_color_settings(httpd_req_t *r){ SEND_INT(r, g_config.clock_color_settings); }
static esp_err_t h_get_color_hour(httpd_req_t *r)  { SEND_HEX(r,g_config.r[1],g_config.g[1],g_config.b[1]); }
static esp_err_t h_get_color_mins(httpd_req_t *r)  { SEND_HEX(r,g_config.r[2],g_config.g[2],g_config.b[2]); }
static esp_err_t h_get_color_colon(httpd_req_t *r) { SEND_HEX(r,g_config.r[3],g_config.g[3],g_config.b[3]); }

static esp_err_t h_update_clock_display_type(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"clockDisplayType",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.clock_display_type=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_colon_type(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"colonType",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.colon_type=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_timezone(httpd_req_t *r) {
    char gmt[16]={0};
    get_body_param(r,"TimezoneSetting",gmt,sizeof(gmt));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.gmt_offset_sec=atoi(gmt);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_ds_time(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"DSTime",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.ds_time=parse_bool_str(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_hour_color(httpd_req_t *r) {
    uint8_t rv,gv,bv;
    if (get_body_rgb(r,&rv,&gv,&bv)!=ESP_OK) { SEND_OK(r); }
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.r[1]=rv; g_config.g[1]=gv; g_config.b[1]=bv;
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_mins_color(httpd_req_t *r) {
    uint8_t rv,gv,bv;
    if (get_body_rgb(r,&rv,&gv,&bv)!=ESP_OK) { SEND_OK(r); }
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.r[2]=rv; g_config.g[2]=gv; g_config.b[2]=bv;
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_colon_color(httpd_req_t *r) {
    uint8_t rv,gv,bv;
    if (get_body_rgb(r,&rv,&gv,&bv)!=ESP_OK) { SEND_OK(r); }
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.r[3]=rv; g_config.g[3]=gv; g_config.b[3]=bv;
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}
static esp_err_t h_update_clock_color_settings(httpd_req_t *r) {
    char buf[8]={0}; get_body_param(r,"ClockColorSettings",buf,sizeof(buf));
    xSemaphoreTake(g_config_mutex,portMAX_DELAY);
    g_config.clock_color_settings=atoi(buf);
    xSemaphoreGive(g_config_mutex); storage_save_all(); SEND_OK(r);
}

void register_handlers_clock(httpd_handle_t s) {
    REG(s, HTTP_POST, "/goClockMode",       h_go_clock);
    REG(s, HTTP_POST, "/goTemperatureMode", h_go_temperature);
    REG(s, HTTP_POST, "/goDateMode",        h_go_date);
    REG(s, HTTP_POST, "/goHumidityMode",    h_go_humidity);
    REG(s, HTTP_POST, "/goDisplayOffMode",  h_go_display_off);
    REG(s, HTTP_POST, "/goScrollingMode",   h_go_scroll);
    REG(s, HTTP_POST, "/goScoreboardMode",  h_go_scoreboard);
    REG(s, HTTP_POST, "/goCountdownMode",   h_go_countdown);
    REG(s, HTTP_POST, "/goStopwatchMode",   h_go_stopwatch);
    REG(s, HTTP_POST, "/goLightshowMode",   h_go_lightshow);
    REG(s, HTTP_GET,  "/getLightshowSpeed", h_get_lightshow_speed);
    REG(s, HTTP_POST, "/setLightshowSpeed", h_set_lightshow_speed);
    REG(s, HTTP_POST, "/goSpectrumMode",    h_go_spectrum);
    REG(s, HTTP_POST, "/getPreset1",        h_get_preset1);
    REG(s, HTTP_POST, "/getPreset2",        h_get_preset2);
    REG(s, HTTP_POST, "/setpreset1",        h_set_preset1);
    REG(s, HTTP_POST, "/setpreset2",        h_set_preset2);
    REG(s, HTTP_POST, "/setdate",           h_setdate);
    REG(s, HTTP_GET,  "/getClockDisplayType",     h_get_clock_display_type);
    REG(s, HTTP_GET,  "/getcolonType",            h_get_colon_type);
    REG(s, HTTP_GET,  "/getgmtOffset_sec",        h_get_gmt_offset);
    REG(s, HTTP_GET,  "/getDSTime",               h_get_ds_time);
    REG(s, HTTP_GET,  "/getClockColorSettings",   h_get_clock_color_settings);
    REG(s, HTTP_GET,  "/getcolorHour",            h_get_color_hour);
    REG(s, HTTP_GET,  "/getcolorMins",            h_get_color_mins);
    REG(s, HTTP_GET,  "/getcolorColon",           h_get_color_colon);
    REG(s, HTTP_POST, "/updateClockDisplayType",  h_update_clock_display_type);
    REG(s, HTTP_POST, "/updateColonType",         h_update_colon_type);
    REG(s, HTTP_POST, "/updateTimezoneSettings",  h_update_timezone);
    REG(s, HTTP_POST, "/updateDSTime",            h_update_ds_time);
    REG(s, HTTP_POST, "/updateHourColor",         h_update_hour_color);
    REG(s, HTTP_POST, "/updateMinsColor",         h_update_mins_color);
    REG(s, HTTP_POST, "/updateColonColor",        h_update_colon_color);
    REG(s, HTTP_POST, "/updateClockColorSettings",h_update_clock_color_settings);
}
