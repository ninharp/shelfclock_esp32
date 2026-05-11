#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"
#include "rtttl_player.h"
extern bool g_digit_anim_active;
#include "esp_timer.h"
#include "esp_random.h"
#include <time.h>
#include <math.h>

// Global flags (defined here, extern in clock_modes.h)
bool g_flag_min   = false;
bool g_flag_hour  = false;
bool g_flag_day   = false;
bool g_flag_week  = false;
bool g_flag_month = false;

static bool s_dots_on = true;
static int  s_prev_sec = -1;

static digit_anim_t s_anims[7] = {
    DIGIT_ANIM_INIT, DIGIT_ANIM_INIT, DIGIT_ANIM_INIT, DIGIT_ANIM_INIT,
    DIGIT_ANIM_INIT, DIGIT_ANIM_INIT, DIGIT_ANIM_INIT
};

static crgb_t pick_color(uint8_t cs, int ridx, bool flag) {
    (void)flag;
    bool pastel = g_config.pastel_colors;
    uint8_t ccf = g_config.color_change_frequency;
    bool trigger = (ccf == 0) ||
                   (ccf == 1 && g_flag_min)   || (ccf == 2 && g_flag_hour) ||
                   (ccf == 3 && g_flag_day)   || (ccf == 4 && g_flag_week) ||
                   (ccf == 5 && g_flag_month);
    if (cs == 0 || cs == 1) return CRGB(g_config.r[ridx], g_config.g[ridx], g_config.b[ridx]);
    if ((cs == 2 || cs == 3) && trigger) return random_color(pastel);
    return CRGB(g_config.r[ridx], g_config.g[ridx], g_config.b[ridx]);
}

void mode_time_update(void) {
    time_t now = time(NULL);
    struct tm ti;
    localtime_r(&now, &ti);
    int hour = ti.tm_hour, mins = ti.tm_min, secs = ti.tm_sec;

    // New-Year-Countdown (clockDisplayType==4)
    if (g_config.clock_display_type == 4) {
        int mday = ti.tm_mday, mont = ti.tm_mon + 1, year = ti.tm_year + 1900;
        int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        int days_left = 0;
        bool leap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
        if (leap) days_left++;
        if (mont != 12) {
            for (int i = mont + 1; i < 13; i++) days_left += days_in_month[i - 1];
        }
        days_left += days_in_month[mont - 1] - mday;
        int hours_left = days_left * 24 + (23 - hour);
        // DST adjustment
        int y = year - 2000, x = (y + y / 4 + 2) % 7;
        bool dst = false;
        if (mont == 3  && mday == (14 - x) && hour >= 2) dst = true;
        if ((mont == 3 && mday >  (14 - x)) || mont > 3) dst = true;
        if (mont == 11 && mday == (7 - x)  && hour >= 2) dst = false;
        if ((mont == 11 && mday > (7 - x)) || mont > 11 || mont < 3) dst = false;
        if (dst) hours_left++;
        int mins_left  = hours_left * 60 + (59 - mins);
        int secs_left  = mins_left * 60  + (60 - secs);
        int display    = hours_left;
        crgb_t col = CRGB(g_config.r[1], g_config.g[1], g_config.b[1]);
        if (mins_left  <= 9999 && secs_left > 9999) { display = mins_left;  col = CRGB(g_config.r[2], g_config.g[2], g_config.b[2]); }
        if (secs_left  <= 9999 && secs_left > 10)   { display = secs_left;  col = CRGB(g_config.r[3], g_config.g[3], g_config.b[3]); }
        if (secs_left  <= 10)                        { display = secs_left; }

        uint8_t n1 = display / 1000, n2 = (display % 1000) / 100;
        uint8_t n3 = (display % 100) / 10, n4 = display % 10;
        all_blank();
        if (display >= 1000) {
            display_number(n1, 6, col); display_number(n2, 4, col);
            display_number(n3, 2, col); display_number(n4, 0, col);
        } else if (display >= 100) {
            display_number(n2, 5, col); display_number(n3, 3, col); display_number(n4, 1, col);
        } else {
            display_number(n3, 4, col); display_number(n4, 2, col);
        }
        if (mday == 1 && mont == 1 && hour == 0 && mins == 0 && secs <= 3) scroll("hAPPy nEW yEAr");
        return;
    }

    // Blinking dots for clockDisplayType 0 and 3
    if (secs != s_prev_sec) { s_dots_on = !s_dots_on; s_prev_sec = secs; }

    uint8_t cs = g_config.clock_color_settings;
    crgb_t hc = pick_color(cs, 1, true);
    crgb_t mc = (cs == 1 || cs == 3) ? hc : pick_color(cs, 2, true);

    int disp_hour = hour;
    if (g_config.clock_display_type != 1) {
        if (disp_hour > 12) disp_hour -= 12;
        if (disp_hour < 1)  disp_hour += 12;
    }
    uint8_t h1 = disp_hour / 10, h2 = disp_hour % 10;
    uint8_t m1 = mins / 10,      m2 = mins % 10;

    // clockDisplayType: 0=center, 1=24h military, 2=space-padded, 3=blink-center
    bool any_anim = false;
    if (g_config.clock_display_type == 1) {
        // 24-Stunden (zero-padded)
        any_anim |= display_number_animated(h1 < 1 ? 0 : h1, 6, hc, &s_anims[6]);
        any_anim |= display_number_animated(h2,                4, hc, &s_anims[4]);
        any_anim |= display_number_animated(m1,                2, mc, &s_anims[2]);
        any_anim |= display_number_animated(m2,                0, mc, &s_anims[0]);
    } else if (g_config.clock_display_type == 2) {
        // 12h space-padded
        any_anim |= display_number_animated(h1 < 1 ? 10 : h1, 6, hc, &s_anims[6]);
        any_anim |= display_number_animated(h2,                4, hc, &s_anims[4]);
        any_anim |= display_number_animated(m1,                2, mc, &s_anims[2]);
        any_anim |= display_number_animated(m2,                0, mc, &s_anims[0]);
    } else {
        // 0 oder 3: zentriert, Stunden-Zehner über rohe Segmente
        if (h1 > 0) {
            crgb_t th = (cs == 4) ? random_color(g_config.pastel_colors) : hc;
            for (int i = 32 * LEDS_PER_SEGMENT; i < 33 * LEDS_PER_SEGMENT; i++)
                if (i < NUM_LEDS) g_leds[i] = th;
            th = (cs == 4) ? random_color(g_config.pastel_colors) : hc;
            for (int i = 33 * LEDS_PER_SEGMENT; i < 34 * LEDS_PER_SEGMENT; i++)
                if (i < NUM_LEDS) g_leds[i] = th;
        } else {
            for (int i = 32 * LEDS_PER_SEGMENT; i < 34 * LEDS_PER_SEGMENT; i++)
                if (i < NUM_LEDS) g_leds[i] = CRGB_BLACK;
        }
        any_anim |= display_number_animated(h2, 5, hc, &s_anims[5]);
        any_anim |= display_number_animated(m1, 2, mc, &s_anims[2]);
        any_anim |= display_number_animated(m2, 0, mc, &s_anims[0]);
        if (g_config.clock_display_type == 3 || g_config.clock_display_type == 0) {
            blink_dots(&s_dots_on);
        }
    }
    g_digit_anim_active = any_anim;
}
