#include "led_display.h"
#include "storage.h"
#include "led_strip.h"
#include "esp_log.h"
#include "esp_random.h"
#include <string.h>

// static const char *TAG = "led_display";
static led_strip_handle_t s_strip;

crgb_t g_leds[NUM_LEDS];
SemaphoreHandle_t g_led_mutex;

const uint8_t g_numbers[97] = {
    0b0111111, 0b0100001, 0b1110110, 0b1110011, 0b1101001,
    0b1011011, 0b1011111, 0b0110001, 0b1111111, 0b1111011,
    0b0000000, 0b0100001, 0b0101000, 0b1101111, 0b1011011,
    0b1100100, 0b1100001, 0b0001000, 0b0011010, 0b0110010,
    0b0011000, 0b1001100, 0b0000100, 0b1000000, 0b0000010,
    0b1100100, 0b1111000, 0b0010010, 0b0010011, 0b1011000,
    0b1000010, 0b1110000, 0b1110100, 0b1110111, 0b1111101,
    0b1001111, 0b0011110, 0b1100111, 0b1011110, 0b1011100,
    0b0011111, 0b1101101, 0b0000100, 0b0100111, 0b1011101,
    0b0001110, 0b0010101, 0b0111101, 0b0111111, 0b1111100,
    0b1111010, 0b0111100, 0b1011011, 0b1001110, 0b0101111,
    0b0101111, 0b0101010, 0b1101101, 0b1101011, 0b1110110,
    0b0011110, 0b1001001, 0b0110011, 0b0111000, 0b0000010,
    0b0100000, 0b1110111, 0b1001111, 0b1000110, 0b1100111,
    0b1111110, 0b1011100, 0b1111011, 0b1001101, 0b0000100,
    0b0000011, 0b1011101, 0b0001100, 0b0000101, 0b1000101,
    0b1000111, 0b1111100, 0b1111001, 0b1000100, 0b1011011,
    0b1001110, 0b0000111, 0b0000111, 0b0000101, 0b1101101,
    0b1101011, 0b1110110, 0b1100001, 0b0001100, 0b1001100,
    0b0010000, 0b1010010
};

#define seg(n) \
    (n)*LEDS_PER_SEGMENT+0,(n)*LEDS_PER_SEGMENT+1,(n)*LEDS_PER_SEGMENT+2, \
    (n)*LEDS_PER_SEGMENT+3,(n)*LEDS_PER_SEGMENT+4,(n)*LEDS_PER_SEGMENT+5, \
    (n)*LEDS_PER_SEGMENT+6

#define digit0  seg(0),  seg(1),  seg(2),  seg(3),  seg(4),  seg(5),  seg(6)
#define fdigit1 seg(2),  seg(7),  seg(10), seg(15), seg(8),  seg(3),  seg(9)
#define digit2  seg(10), seg(11), seg(12), seg(13), seg(14), seg(15), seg(16)
#define fdigit3 seg(12), seg(17), seg(20), seg(25), seg(18), seg(13), seg(19)
#define digit4  seg(20), seg(21), seg(22), seg(23), seg(24), seg(25), seg(26)
#define fdigit5 seg(22), seg(27), seg(30), seg(35), seg(28), seg(23), seg(29)
#define digit6  seg(30), seg(31), seg(32), seg(33), seg(34), seg(35), seg(36)

const uint16_t FAKE_LEDs[FAKE_NUM_LEDS] = {
    digit0, fdigit1, digit2, fdigit3, digit4, fdigit5, digit6
};

const uint16_t FAKE_LEDs_C_BMUP[SEGMENTS_LEDS] = {
    seg(17),seg(11),seg(21),seg(12),seg(20),seg(19),seg(27),seg(7),seg(22),
    seg(10),seg(26),seg(16),seg(25),seg(13),seg(18),seg(1),seg(31),seg(2),
    seg(30),seg(9),seg(29),seg(15),seg(23),seg(14),seg(24),seg(0),seg(32),
    seg(6),seg(36),seg(3),seg(35),seg(8),seg(28),seg(5),seg(33),seg(4),seg(34)
};
const uint16_t FAKE_LEDs_C_CMOT[SEGMENTS_LEDS] = {
    seg(19),seg(26),seg(16),seg(20),seg(13),seg(25),seg(12),seg(17),seg(18),
    seg(21),seg(14),seg(24),seg(11),seg(29),seg(9),seg(22),seg(15),seg(23),
    seg(10),seg(28),seg(7),seg(27),seg(8),seg(36),seg(6),seg(30),seg(3),
    seg(35),seg(2),seg(34),seg(1),seg(31),seg(4),seg(32),seg(5),seg(33),seg(0)
};
const uint16_t FAKE_LEDs_C_BLTR[SEGMENTS_LEDS] = {
    seg(32),seg(31),seg(27),seg(30),seg(36),seg(33),seg(34),seg(35),seg(29),
    seg(22),seg(21),seg(17),seg(20),seg(26),seg(23),seg(28),seg(24),seg(25),
    seg(19),seg(12),seg(11),seg(7),seg(10),seg(16),seg(13),seg(18),seg(14),
    seg(15),seg(9),seg(2),seg(1),seg(0),seg(6),seg(3),seg(8),seg(4),seg(5)
};
const uint16_t FAKE_LEDs_C_TLBR[SEGMENTS_LEDS] = {
    seg(34),seg(33),seg(32),seg(36),seg(35),seg(28),seg(24),seg(23),seg(29),
    seg(30),seg(31),seg(27),seg(22),seg(26),seg(25),seg(18),seg(14),seg(13),
    seg(19),seg(20),seg(21),seg(17),seg(12),seg(16),seg(15),seg(8),seg(4),
    seg(3),seg(9),seg(10),seg(11),seg(7),seg(2),seg(6),seg(5),seg(0),seg(1)
};
const uint16_t FAKE_LEDs_C_TMDN[SEGMENTS_LEDS] = {
    seg(18),seg(14),seg(24),seg(13),seg(25),seg(19),seg(8),seg(28),seg(15),
    seg(23),seg(16),seg(26),seg(12),seg(20),seg(17),seg(4),seg(34),seg(3),
    seg(35),seg(9),seg(29),seg(10),seg(22),seg(11),seg(21),seg(5),seg(33),
    seg(6),seg(36),seg(2),seg(30),seg(7),seg(27),seg(0),seg(32),seg(1),seg(31)
};
const uint16_t FAKE_LEDs_C_CSIN[SEGMENTS_LEDS] = {
    seg(5),seg(32),seg(0),seg(33),seg(6),seg(36),seg(4),seg(31),seg(1),
    seg(34),seg(3),seg(30),seg(2),seg(35),seg(9),seg(29),seg(8),seg(27),
    seg(7),seg(28),seg(15),seg(22),seg(10),seg(23),seg(16),seg(26),seg(14),
    seg(21),seg(11),seg(24),seg(13),seg(20),seg(12),seg(25),seg(19),seg(18),seg(17)
};
const uint16_t FAKE_LEDs_C_BRTL[SEGMENTS_LEDS] = {
    seg(0),seg(1),seg(7),seg(2),seg(6),seg(5),seg(4),seg(3),seg(9),
    seg(10),seg(11),seg(17),seg(12),seg(16),seg(15),seg(8),seg(14),seg(13),
    seg(19),seg(20),seg(21),seg(27),seg(22),seg(26),seg(25),seg(18),seg(24),
    seg(23),seg(29),seg(30),seg(31),seg(32),seg(36),seg(35),seg(28),seg(34),seg(33)
};
const uint16_t FAKE_LEDs_C_TRBL[SEGMENTS_LEDS] = {
    seg(4),seg(5),seg(0),seg(6),seg(3),seg(8),seg(14),seg(15),seg(9),
    seg(2),seg(1),seg(7),seg(10),seg(16),seg(13),seg(18),seg(24),seg(25),
    seg(19),seg(12),seg(11),seg(17),seg(20),seg(26),seg(23),seg(28),seg(34),
    seg(35),seg(29),seg(22),seg(21),seg(27),seg(30),seg(36),seg(33),seg(32),seg(31)
};
const uint16_t FAKE_LEDs_C_OUTS[SEGMENTS_LEDS] = {
    seg(31),seg(36),seg(36),seg(36),seg(34),seg(36),seg(27),seg(36),seg(29),
    seg(36),seg(28),seg(36),seg(21),seg(36),seg(26),seg(36),seg(24),seg(36),
    seg(17),seg(36),seg(19),seg(36),seg(18),seg(36),seg(11),seg(36),seg(16),
    seg(36),seg(14),seg(36),seg(7),seg(36),seg(9),seg(8),seg(1),seg(6),seg(4)
};
const uint16_t FAKE_LEDs_C_OUTS2[SEGMENTS_LEDS] = {
    seg(1),seg(36),seg(6),seg(36),seg(4),seg(36),seg(7),seg(36),seg(9),
    seg(36),seg(8),seg(36),seg(11),seg(36),seg(16),seg(36),seg(14),seg(36),
    seg(17),seg(36),seg(19),seg(36),seg(18),seg(36),seg(21),seg(36),seg(26),
    seg(36),seg(24),seg(36),seg(27),seg(36),seg(29),seg(28),seg(31),seg(36),seg(34)
};
const uint16_t FAKE_LEDs_C_VERT[SEGMENTS_LEDS] = {
    seg(32),seg(36),seg(33),seg(36),seg(30),seg(36),seg(35),seg(36),seg(36),
    seg(22),seg(36),seg(23),seg(36),seg(36),seg(20),seg(36),seg(25),seg(36),
    seg(36),seg(12),seg(36),seg(13),seg(36),seg(36),seg(10),seg(36),seg(15),
    seg(36),seg(36),seg(2),seg(36),seg(3),seg(36),seg(0),seg(36),seg(5),seg(36)
};
const uint16_t FAKE_LEDs_C_VERT2[SEGMENTS_LEDS] = {
    seg(0),seg(36),seg(5),seg(36),seg(36),seg(2),seg(36),seg(3),seg(36),
    seg(36),seg(10),seg(36),seg(15),seg(36),seg(36),seg(12),seg(36),seg(13),
    seg(36),seg(36),seg(20),seg(36),seg(25),seg(36),seg(36),seg(22),seg(36),
    seg(23),seg(36),seg(30),seg(36),seg(35),seg(36),seg(32),seg(36),seg(33),seg(36)
};
const uint16_t FAKE_LEDs_C_FIRE[SEGMENTS_LEDS] = {
    seg(17),seg(11),seg(21),seg(12),seg(20),seg(19),seg(27),seg(7),seg(22),
    seg(10),seg(26),seg(16),seg(25),seg(13),seg(18),seg(1),seg(31),seg(2),
    seg(30),seg(9),seg(29),seg(15),seg(23),seg(14),seg(24),seg(0),seg(32),
    seg(6),seg(36),seg(3),seg(35),seg(8),seg(28),seg(5),seg(33),seg(4),seg(34)
};
const uint16_t FAKE_LEDs_C_RAIN[SEGMENTS_LEDS] = {
    seg(30),seg(35),seg(1),seg(6),seg(4),seg(22),seg(23),seg(31),seg(36),
    seg(34),seg(12),seg(13),seg(7),seg(9),seg(8),seg(0),seg(5),seg(17),
    seg(19),seg(18),seg(10),seg(15),seg(21),seg(26),seg(24),seg(2),seg(3),
    seg(27),seg(29),seg(28),seg(20),seg(25),seg(11),seg(16),seg(14),seg(32),seg(33)
};
const uint16_t FAKE_LEDs_SNAKE[SEGMENTS_LEDS] = {
    seg(0),seg(1),seg(7),seg(2),seg(6),seg(5),seg(4),seg(3),seg(9),
    seg(10),seg(11),seg(17),seg(12),seg(16),seg(15),seg(8),seg(14),seg(13),
    seg(19),seg(20),seg(21),seg(27),seg(22),seg(26),seg(25),seg(18),seg(24),
    seg(23),seg(29),seg(30),seg(31),seg(32),seg(36),seg(35),seg(28),seg(34),seg(33)
};

void led_display_init(void) {
    g_led_mutex = xSemaphoreCreateMutex();
    memset(g_leds, 0, sizeof(g_leds));

    led_strip_config_t strip_cfg = {
        .strip_gpio_num          = LED_GPIO_NUM,
        .max_leds                = NUM_LEDS,
        .led_model               = LED_MODEL_WS2812,
        .color_component_format  = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags.invert_out        = false,
    };
    led_strip_rmt_config_t rmt_cfg = {
        .clk_src       = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip));
    led_strip_clear(s_strip);
}

void led_refresh(void) {
    for (int i = 0; i < NUM_LEDS; i++) {
        led_strip_set_pixel(s_strip, i, g_leds[i].r, g_leds[i].g, g_leds[i].b);
    }
    led_strip_refresh(s_strip);
}

void all_blank(void) {
    memset(g_leds, 0, sizeof(g_leds));
}

void display_number(uint8_t num, int digit_pos, crgb_t color) {
    if (num >= 97) return;
    for (int s = 0; s < SEGMENTS_PER_NUMBER; s++) {
        bool seg_on = (g_numbers[num] >> s) & 1;
        crgb_t c = seg_on ? color : CRGB_BLACK;
        for (int led = 0; led < LEDS_PER_SEGMENT; led++) {
            int fake_idx = (digit_pos * SEGMENTS_PER_NUMBER + s) * LEDS_PER_SEGMENT + led;
            if (fake_idx >= FAKE_NUM_LEDS) continue;
            uint16_t real_idx = FAKE_LEDs[fake_idx];
            if (real_idx < NUM_LEDS) g_leds[real_idx] = c;
        }
    }
}

void shelf_down_lights(void) {
    if (!g_config.use_spotlights) {
        for (int i = SEGMENTS_LEDS; i < NUM_LEDS; i++) g_leds[i] = CRGB_BLACK;
        return;
    }
    crgb_t sc;
    if (g_config.spotlights_color_settings == 0) {
        sc = CRGB(g_config.r[0], g_config.g[0], g_config.b[0]);
    } else {
        sc = random_color(g_config.pastel_colors);
    }
    for (int i = SEGMENTS_LEDS; i < NUM_LEDS; i++) g_leds[i] = sc;
}

void blink_dots(bool *dots_on) {
    crgb_t col = *dots_on ? CRGB(g_config.r[3], g_config.g[3], g_config.b[3])
                           : CRGB_BLACK;
    if (g_config.colon_type == 0) {
        int mid = LEDS_PER_SEGMENT / 2;
        for (int i = 25*LEDS_PER_SEGMENT+mid-1; i <= 25*LEDS_PER_SEGMENT+mid; i++)
            if (i < NUM_LEDS) g_leds[i] = col;
        for (int i = 20*LEDS_PER_SEGMENT+mid-1; i <= 20*LEDS_PER_SEGMENT+mid; i++)
            if (i < NUM_LEDS) g_leds[i] = col;
    } else if (g_config.colon_type == 1) {
        for (int i = 25*LEDS_PER_SEGMENT; i < 26*LEDS_PER_SEGMENT; i++)
            if (i < NUM_LEDS) g_leds[i] = col;
        for (int i = 20*LEDS_PER_SEGMENT; i < 21*LEDS_PER_SEGMENT; i++)
            if (i < NUM_LEDS) g_leds[i] = col;
    } else {
        for (int i = 20*LEDS_PER_SEGMENT; i < 21*LEDS_PER_SEGMENT; i++)
            if (i < NUM_LEDS) g_leds[i] = col;
    }
    *dots_on = !(*dots_on);
}

void set_brightness(uint8_t brightness) {
    (void)brightness;
}

crgb_t color_wheel(int pos) {
    pos &= 0xFF;
    crgb_t c = {0, 0, 0};
    if (pos < 85) {
        c.r = pos * 3; c.g = 0; c.b = 255 - pos * 3;
    } else if (pos < 170) {
        pos -= 85; c.r = 255 - pos * 3; c.g = pos * 3; c.b = 0;
    } else {
        pos -= 170; c.r = 0; c.g = 255 - pos * 3; c.b = pos * 3;
    }
    return c;
}

crgb_t color_wheel2(int pos) { return color_wheel(pos); }

crgb_t hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v) {
    if (s == 0) return CRGB(v, v, v);
    uint8_t region = h / 43;
    uint8_t remainder = (h - (region * 43)) * 6;
    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    switch (region) {
        case 0: return CRGB(v, t, p);
        case 1: return CRGB(q, v, p);
        case 2: return CRGB(p, v, t);
        case 3: return CRGB(p, q, v);
        case 4: return CRGB(t, p, v);
        default: return CRGB(v, p, q);
    }
}

crgb_t random_color(bool pastel) {
    if (!pastel) return hsv_to_rgb(esp_random() & 0xFF, 255, 255);
    return CRGB(esp_random() & 0xFF, esp_random() & 0xFF, esp_random() & 0xFF);
}

void fade_to_black_by(crgb_t *c, uint8_t amount) {
    c->r = c->r > amount ? c->r - amount : 0;
    c->g = c->g > amount ? c->g - amount : 0;
    c->b = c->b > amount ? c->b - amount : 0;
}
