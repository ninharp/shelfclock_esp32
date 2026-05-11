#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"

extern bool g_digit_anim_active;

static digit_anim_t s_anims[7] = {
    DIGIT_ANIM_INIT, DIGIT_ANIM_INIT, DIGIT_ANIM_INIT, DIGIT_ANIM_INIT,
    DIGIT_ANIM_INIT, DIGIT_ANIM_INIT, DIGIT_ANIM_INIT
};

void mode_scoreboard_update(void) {
    uint8_t sl1 = g_config.scoreboard_left  / 10;
    uint8_t sl2 = g_config.scoreboard_left  % 10;
    uint8_t sr1 = g_config.scoreboard_right / 10;
    uint8_t sr2 = g_config.scoreboard_right % 10;
    crgb_t lc = CRGB(g_config.r[13], g_config.g[13], g_config.b[13]);
    crgb_t rc = CRGB(g_config.r[14], g_config.g[14], g_config.b[14]);
    bool any = false;
    any |= display_number_animated(sl1, 6, lc, &s_anims[6]);
    any |= display_number_animated(sl2, 4, lc, &s_anims[4]);
    any |= display_number_animated(sr1, 2, rc, &s_anims[2]);
    any |= display_number_animated(sr2, 0, rc, &s_anims[0]);
    g_digit_anim_active = any;
}
