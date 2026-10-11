#ifndef CELESTIAL_LEDS_H
#define CELESTIAL_LEDS_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define RAINBOW_COLOR_COUNT  7

typedef enum {
    LED_ANIM_BREATHE = 0,    // 0. Slow cosmic breathing
    LED_ANIM_VORTEX,         // 1. Spinning galaxy comet vortex
    LED_ANIM_SNAKE,          // 2. Cosmic dragon snake winding across all rings
    LED_ANIM_RAINBOW,        // 3. Smooth chromatic rainbow wave
    LED_ANIM_PULSAR,         // 4. Cosmic heartbeat / pulsar shockwave
    LED_ANIM_PLASMA,         // 5. Living solar plasma & fire flames
    LED_ANIM_STARS,          // 6. Deep navy & diamond starlight sparkle
    LED_ANIM_VOICE,          // 7. Live audio/voice reactive VU halo
    LED_ANIM_SOLAR,          // 8. Solar Orrery planetary rings
    LED_ANIM_DATA_RECV,      // 9. Concentric data reception radar ripples
    LED_ANIM_EXPAND_SPEECH,  // 10. Expanding sonic ripples from center outward to rims
    LED_ANIM_COUNT
} led_anim_mode_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    const char *name;
} led_color_t;

extern const led_color_t RAINBOW_COLORS[RAINBOW_COLOR_COUNT];

/**
 * Initialize WS2812 LED subsystem on GP15 using PIO0 SM1.
 */
void leds_init(void);

/**
 * Set individual LED color (0..LED_RING_COUNT-1).
 */
void leds_set_pixel_rgb(int index, uint8_t r, uint8_t g, uint8_t b);

/**
 * Set every configured LED slot to a specific RGB color (0..255).
 */
void leds_set_all_rgb(uint8_t r, uint8_t g, uint8_t b);

/**
 * Set active animation mode preset.
 */
void leds_set_anim_mode(int mode);

/**
 * Get active animation mode preset.
 */
int leds_get_anim_mode(void);

/**
 * Get friendly display name of animation preset.
 */
const char* leds_get_anim_name(int mode);

/**
 * Set color palette index for animations that use a base color.
 */
void leds_set_rainbow_index(int color_idx);

/**
 * Get name of rainbow color at index (0..6).
 */
const char* leds_get_rainbow_name(int color_idx);

/**
 * Update LED animations (call continuously in main loop).
 */
void leds_update(bool easter_egg_active, bool led_menu_active, int selected_anim_idx, uint8_t mic_level_pct);

/**
 * Set brightness percent, clamped to the configured LED_BRIGHTNESS_LIMIT.
 */
void leds_set_brightness(uint8_t brightness_pct);

#endif /* CELESTIAL_LEDS_H */
