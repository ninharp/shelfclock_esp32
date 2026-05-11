#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

// Countdown/Stopwatch timestamps (esp_timer_get_time() in µs)
extern int64_t g_countdown_end_us;
extern int64_t g_countup_start_us;
extern int64_t g_countup_end_us;

// Shared flags for color-change frequency (set/cleared in main_task)
extern bool g_flag_min;
extern bool g_flag_hour;
extern bool g_flag_day;
extern bool g_flag_week;
extern bool g_flag_month;

void mode_time_update(void);
void mode_date_update(void);
void mode_temperature_update(void);
void mode_humidity_update(void);
void mode_scoreboard_update(void);
void mode_countdown_update(void);
void mode_stopwatch_update(void);
void mode_scroll_update(void);     // standalone scroll mode (clockMode==11)
void mode_scroll_overlay(void);    // periodic overlay scroll

void mode_countdown_start(int32_t duration_ms);
bool mode_countdown_check_ended(void);
void mode_stopwatch_start(int32_t duration_ms);

void scroll(const char *text);     // directly callable from main_task (e.g. "MAkE A WISH")
void end_countdown(void);
