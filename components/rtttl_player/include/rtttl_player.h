#pragma once
#include <stdbool.h>
#include <stdatomic.h>

#define BUZZER_GPIO_NUM    5
#define RTTTL_SONG_SMB      0
#define RTTTL_SONG_BIRTHDAY 3
#define RTTTL_SONG_STARWARS 1
#define RTTTL_SONG_AULDLANG 7
#define RTTTL_SONG_XMAS     10

extern atomic_bool g_rtttl_breakout;

void rtttl_player_init(void);
void rtttl_play_song(int song_index);
void rtttl_stop(void);
bool rtttl_is_playing(void);
