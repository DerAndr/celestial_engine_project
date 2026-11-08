#ifndef CELESTIAL_LED_PATTERNS_H
#define CELESTIAL_LED_PATTERNS_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "leds.h"

#ifdef __cplusplus
extern "C" {
#endif

// Simple HSV to RGB converter (h: 0..359, s: 0..255, v: 0..255)
void led_patterns_hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b);

static inline void hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v, uint8_t *r, uint8_t *g, uint8_t *b) {
    led_patterns_hsv_to_rgb(h, s, v, r, g, b);
}

// Set clock time used by chronometer / celestial clock patterns
void led_patterns_set_clock_time(uint8_t hours, uint8_t minutes, uint8_t seconds, uint16_t millis);

// Renders one frame of the specified animation preset into pixels_r/g/b arrays
void led_patterns_render(int anim_mode, uint32_t anim_tick, int base_color_idx,
                         int anim_ai_model, uint8_t mic_level_pct,
                         uint8_t *pixels_r, uint8_t *pixels_g, uint8_t *pixels_b);

// Render chiptune frequency-reactive animation
void led_patterns_render_chiptune_sync(uint32_t anim_tick,
                                      uint8_t *pixels_r, uint8_t *pixels_g, uint8_t *pixels_b);

#if CELESTIAL_FOUR_RINGS && CELESTIAL_RING_EXPERIMENT
void led_patterns_render_spare_ring(uint32_t anim_tick, int mode, uint8_t mic_level_pct,
                                   bool music_active,
                                   uint8_t *pixels_r, uint8_t *pixels_g, uint8_t *pixels_b);
#endif

#ifdef __cplusplus
}
#endif

#endif /* CELESTIAL_LED_PATTERNS_H */
