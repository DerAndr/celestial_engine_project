#ifndef CELESTIAL_LEDS_H
#define CELESTIAL_LEDS_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define RAINBOW_COLOR_COUNT  10

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
    LED_ANIM_GALAXY,         // 11. Milky Way galactic spiral & Sol beacon
    LED_ANIM_EARTH,          // 12. Terra globe: azure oceans, emerald land, aurora
    LED_ANIM_HALLOWEEN,      // 13. Halloween Jack-o'-Lantern & spooky ectoplasm
    LED_ANIM_CLOCK,          // 14. Celestial Chronometer: 3-ring astronomical clock with central stone
    LED_ANIM_HOURGLASS,      // 15. Aetheric Hourglass: orientation-independent concentric ring accumulator
    LED_ANIM_PRISMATIC,      // 16. Chroma Prismatic: full spectral color-encoded clock (orientation-free)
    LED_ANIM_ORRERY,         // 17. Celestial Orrery: kinematic concentric triple orbital clock
    LED_ANIM_SPIRAL_SNAKE,   // 18. Ouroboros Snake: 2D serpent spiraling through rings 24->12->7 and erupting outward
    LED_ANIM_RADIAL_WAVE,    // 19. Concentric Ripple / Shockwave propagating outward with inverse-square damping
    LED_ANIM_COLLIDING_WAVES,// 20. Counter-propagating waves colliding at 0° and 180° with white interference flash
    LED_ANIM_ARCHIMEDES_SPIRAL, // 21. Hypnotic rotating Archimedean dual spiral arms with radial phase lag
    LED_ANIM_SACRED_POLYGON, // 22. Stroboscopic geometric runes (Triquetra, Hexagram, Dodecagram gear ratios)
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
 * Set active AI model for dynamic speech & processing color themes.
 */
void leds_set_ai_model(int model_idx);

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
 * Get currently selected color palette index.
 */
int leds_get_rainbow_index(void);

/**
 * Get total number of available color palette entries.
 */
int leds_get_rainbow_count(void);

/**
 * Get name of rainbow color at index (0..RAINBOW_COLOR_COUNT-1).
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

/**
 * Set current time for the Celestial Chronometer clock animation.
 */
void leds_set_clock_time(uint8_t hours, uint8_t minutes, uint8_t seconds, uint16_t millis);

#endif /* CELESTIAL_LEDS_H */
