#pragma once
#include <stdint.h>

void display_time_mode(void);
void display_date_mode(void);
void display_temperature_mode(void);
void display_humidity_mode(void);
void display_scoreboard_mode(void);
void display_countdown_mode(void);
void display_stopwatch_mode(void);
void display_scroll_mode(void);

void mode_countdown_start(int32_t ms);
void mode_stopwatch_start(int32_t ms);
