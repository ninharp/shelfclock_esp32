#include "rtttl_player.h"
#include "rtttl_songs.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include <stdatomic.h>

__attribute__((unused)) static const char *TAG = "rtttl";
static QueueHandle_t s_queue;
static bool s_playing = false;

atomic_bool g_rtttl_breakout = false;

const char *const RTTTL_SONGS[] = {
    RTTTL_SMB_UNDER, RTTTL_STARWARS, RTTTL_RICKROLL2, RTTTL_MARIO,
    RTTTL_FINALCOUNT, RTTTL_RICKROLL, RTTTL_MSPACMAN, RTTTL_AULDLANG,
    RTTTL_STARTREK,  RTTTL_XMEN,     RTTTL_GALAGA,    RTTTL_BEETHOVEN,
    RTTTL_PUFFS,     RTTTL_ADAMS,    RTTTL_BURGERTIME, RTTTL_TRON,
    RTTTL_HALLOWEEN, RTTTL_MANDY,    RTTTL_MACGYVER,  RTTTL_TAKEONME,
    RTTTL_NOKIA,     RTTTL_BIRTHDAY, RTTTL_XMAS,      RTTTL_CINCO,
    RTTTL_RICKROLL,
};
const int RTTTL_NUM_SONGS = sizeof(RTTTL_SONGS) / sizeof(RTTTL_SONGS[0]);

static void ledc_set_note(uint32_t freq_hz) {
    if (freq_hz == 0) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    } else {
        ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, freq_hz);
        ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
}

static const uint32_t NOTE_FREQ[7] = {262, 294, 330, 349, 392, 440, 494};

static uint32_t note_to_freq(char note, bool sharp, int octave) {
    if (note == 'p') return 0;
    int idx = -1;
    switch (note) {
        case 'c': idx = 0; break; case 'd': idx = 1; break; case 'e': idx = 2; break;
        case 'f': idx = 3; break; case 'g': idx = 4; break; case 'a': idx = 5; break;
        case 'b': idx = 6; break;
    }
    if (idx < 0) return 0;
    uint32_t f = NOTE_FREQ[idx];
    if (sharp) f = (uint32_t)(f * 1.059463f);
    int shift = octave - 4;
    if (shift > 0) f <<= shift;
    else if (shift < 0) f >>= (-shift);
    return f;
}

static void play_rtttl(const char *song) {
    const char *p = strchr(song, ':');
    if (!p) return;
    p++;
    int default_dur = 4, default_oct = 6, bpm = 63;
    if (*p == 'd') { p += 2; default_dur = atoi(p); while (*p && *p != ',') p++; p++; }
    if (*p == 'o') { p += 2; default_oct = atoi(p); while (*p && *p != ',') p++; p++; }
    if (*p == 'b') { p += 2; bpm = atoi(p); while (*p && *p != ':') p++; p++; }

    uint32_t whole_ms = 240000 / bpm;

    while (*p && !atomic_load(&g_rtttl_breakout)) {
        int dur = 0;
        if (*p >= '1' && *p <= '9') { dur = atoi(p); while (*p >= '0' && *p <= '9') p++; }
        if (!dur) dur = default_dur;

        char note = 0;
        if ((*p >= 'a' && *p <= 'g') || *p == 'p') note = *p++;

        bool sharp = (*p == '#') ? (p++, true) : false;
        bool dot   = false;
        int oct = default_oct;
        if (*p >= '4' && *p <= '7') oct = *p++ - '0';
        if (*p == '.') { dot = true; p++; }

        uint32_t dur_ms = whole_ms / dur;
        if (dot) dur_ms = dur_ms * 3 / 2;

        ledc_set_note(note_to_freq(note, sharp, oct));
        vTaskDelay(pdMS_TO_TICKS(dur_ms * 85 / 100));
        ledc_set_note(0);
        vTaskDelay(pdMS_TO_TICKS(dur_ms * 15 / 100));

        if (*p == ',') p++;
    }
    ledc_set_note(0);
}

static void rtttl_task(void *arg) {
    int song_idx;
    while (1) {
        if (xQueueReceive(s_queue, &song_idx, portMAX_DELAY)) {
            s_playing = true;
            atomic_store(&g_rtttl_breakout, false);
            if (song_idx >= 0 && song_idx < RTTTL_NUM_SONGS) {
                play_rtttl(RTTTL_SONGS[song_idx]);
            }
            s_playing = false;
        }
    }
}

void rtttl_player_init(void) {
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 2000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer);
    ledc_channel_config_t ch = {
        .gpio_num   = BUZZER_GPIO_NUM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&ch);
    s_queue = xQueueCreate(4, sizeof(int));
    xTaskCreate(rtttl_task, "rtttl", 4096, NULL, 3, NULL);
}

void rtttl_play_song(int idx) { xQueueSend(s_queue, &idx, 0); }
void rtttl_stop(void) { atomic_store(&g_rtttl_breakout, true); }
bool rtttl_is_playing(void) { return s_playing; }
