#include "leds.h"
#include "config.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pico/time.h"
#include "ws2812.pio.h"
#include <math.h>

static PIO led_pio = pio0;
static uint led_sm = 1;
static uint led_offset = 0;

static uint8_t master_brightness = 18; // Default 18% (safe low-current, gentle on eyes)
static absolute_time_t next_anim_time;
static uint16_t rainbow_rotation_deg = 0;

const led_color_t RAINBOW_COLORS[RAINBOW_COLOR_COUNT] = {
    { 255,   0,   0, "RED" },     // 0. Red
    { 255,  85,   0, "ORANGE" },  // 1. Orange
    { 255, 210,   0, "YELLOW" },  // 2. Yellow
    {   0, 255,   0, "GREEN" },   // 3. Green
    {   0, 200, 255, "CYAN" },    // 4. Cyan
    {   0,  30, 255, "BLUE" },    // 5. Blue
    { 180,   0, 255, "VIOLET" }   // 6. Violet
};

static inline uint32_t rgb_to_grb(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t r_scaled = (uint32_t)(r * master_brightness) / 100;
    uint32_t g_scaled = (uint32_t)(g * master_brightness) / 100;
    uint32_t b_scaled = (uint32_t)(b * master_brightness) / 100;
    return ((uint32_t)g_scaled << 16) | ((uint32_t)r_scaled << 8) | (uint32_t)b_scaled;
}

static inline void put_pixel(uint32_t pixel_grb) {
    // Non-blocking FIFO write (TX FIFO joined has 8 slots, easily holding 7 LED words)
    pio_sm_put(led_pio, led_sm, pixel_grb << 8u);
}

// Simple HSV to RGB converter (h: 0..359, s: 0..255, v: 0..255)
static void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (s == 0) {
        *r = *g = *b = v;
        return;
    }
    uint8_t region = h / 60;
    uint8_t remainder = (h - (region * 60)) * 6;
    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    switch (region) {
        case 0: *r = v; *g = t; *b = p; break;
        case 1: *r = q; *g = v; *b = p; break;
        case 2: *r = p; *g = v; *b = t; break;
        case 3: *r = p; *g = q; *b = v; break;
        case 4: *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

void leds_init(void) {
    led_offset = pio_add_program(led_pio, &ws2812_program);
    ws2812_program_init(led_pio, led_sm, led_offset, PIN_WS2812_DATA, 800000.0f, false);

    // Initial blank (all off)
    leds_set_all_rgb(0, 0, 0);
    next_anim_time = get_absolute_time();
}

void leds_set_all_rgb(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t grb = rgb_to_grb(r, g, b);
    for (int i = 0; i < LED_RING_COUNT; i++) {
        put_pixel(grb);
    }
    sleep_us(80); // Reset latch (>50us)
}

void leds_set_rainbow_index(int color_idx) {
    if (color_idx < 0) color_idx = 0;
    color_idx = color_idx % RAINBOW_COLOR_COUNT;
    leds_set_all_rgb(RAINBOW_COLORS[color_idx].r,
                     RAINBOW_COLORS[color_idx].g,
                     RAINBOW_COLORS[color_idx].b);
}

const char* leds_get_rainbow_name(int color_idx) {
    if (color_idx < 0) color_idx = 0;
    color_idx = color_idx % RAINBOW_COLOR_COUNT;
    return RAINBOW_COLORS[color_idx].name;
}

void leds_set_brightness(uint8_t brightness_pct) {
    if (brightness_pct > 100) brightness_pct = 100;
    master_brightness = brightness_pct;
}

void leds_update(bool easter_egg_active, bool led_menu_active, int selected_rainbow_idx) {
    // 30 Hz animation rate
    if (absolute_time_diff_us(next_anim_time, get_absolute_time()) < 33000) {
        return;
    }
    next_anim_time = get_absolute_time();

    if (easter_egg_active) {
        // Fast spinning rainbow vortex around the 7-LED ring
        rainbow_rotation_deg = (rainbow_rotation_deg + 15) % 360;
        for (int i = 0; i < LED_RING_COUNT; i++) {
            uint16_t led_hue = (rainbow_rotation_deg + (i * 360 / LED_RING_COUNT)) % 360;
            uint8_t r, g, b;
            hsv_to_rgb(led_hue, 255, 255, &r, &g, &b);
            put_pixel(rgb_to_grb(r, g, b));
        }
        sleep_us(80);
    } else if (led_menu_active) {
        // Solid selected rainbow color
        leds_set_rainbow_index(selected_rainbow_idx);
    } else {
        // Subtle ambient celestial glow in active color (slow breathing)
        rainbow_rotation_deg = (rainbow_rotation_deg + 3) % 360;
        float breath = (sinf((float)rainbow_rotation_deg * 3.14159f / 180.0f) + 1.0f) * 0.5f; // 0.0 to 1.0
        uint8_t r = (uint8_t)(RAINBOW_COLORS[selected_rainbow_idx % RAINBOW_COLOR_COUNT].r * (0.3f + 0.7f * breath));
        uint8_t g = (uint8_t)(RAINBOW_COLORS[selected_rainbow_idx % RAINBOW_COLOR_COUNT].g * (0.3f + 0.7f * breath));
        uint8_t b = (uint8_t)(RAINBOW_COLORS[selected_rainbow_idx % RAINBOW_COLOR_COUNT].b * (0.3f + 0.7f * breath));
        leds_set_all_rgb(r, g, b);
    }
}
