#ifndef CELESTIAL_LEDS_H
#define CELESTIAL_LEDS_H

#include <stdint.h>
#include <stdbool.h>

#define LED_RING_COUNT       7
#define RAINBOW_COLOR_COUNT  7

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    const char *name;
} led_color_t;

extern const led_color_t RAINBOW_COLORS[RAINBOW_COLOR_COUNT];

/**
 * Initialize WS2812 LED subsystem on GP15 using PIO2 SM0.
 */
void leds_init(void);

/**
 * Set all 7 LEDs to a specific RGB color (0..255).
 * Brightness is automatically scaled by master brightness cap.
 */
void leds_set_all_rgb(uint8_t r, uint8_t g, uint8_t b);

/**
 * Set all 7 LEDs to one of the 7 rainbow colors (index 0..6).
 */
void leds_set_rainbow_index(int color_idx);

/**
 * Get name of rainbow color at index (0..6).
 */
const char* leds_get_rainbow_name(int color_idx);

/**
 * Update LED animations (call regularly from main loop).
 * - spinning rainbow vortex if easter egg 67 is active
 * - solid chosen rainbow color in LED menu
 * - gentle cosmic glow in other modes
 */
void leds_update(bool easter_egg_active, bool led_menu_active, int selected_rainbow_idx);

/**
 * Set master brightness cap percent (0..100). Default is 20%.
 */
void leds_set_brightness(uint8_t brightness_pct);

#endif /* CELESTIAL_LEDS_H */
