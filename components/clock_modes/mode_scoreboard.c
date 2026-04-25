#include "clock_modes.h"
#include "storage.h"
#include "led_display.h"

void mode_scoreboard_update(void) {
    uint8_t sl1 = g_config.scoreboard_left  / 10;
    uint8_t sl2 = g_config.scoreboard_left  % 10;
    uint8_t sr1 = g_config.scoreboard_right / 10;
    uint8_t sr2 = g_config.scoreboard_right % 10;
    crgb_t lc = CRGB(g_config.r[13], g_config.g[13], g_config.b[13]);
    crgb_t rc = CRGB(g_config.r[14], g_config.g[14], g_config.b[14]);
    display_number(sl1, 6, lc); display_number(sl2, 4, lc);
    display_number(sr1, 2, rc); display_number(sr2, 0, rc);
}
