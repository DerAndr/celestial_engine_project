#ifndef CELESTIAL_WS2812_DRIVER_H
#define CELESTIAL_WS2812_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "hardware/pio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Self-contained WS2812B Hardware Driver for RP2350 (PIO + DMA).
 * Isolated from application state, rings geometry, and color models.
 */

// Initializes the WS2812 PIO program, state machine, and DMA channel.
bool ws2812_driver_init(PIO pio, uint sm, uint pin, float freq_hz);

// Transmits an array of left-aligned 32-bit GRB words (bits 31..8 = G8-R8-B8)
// via DMA and waits for transmission and >=300us latch low pulse.
void ws2812_driver_transmit_blocking(const uint32_t *grb_words_left_aligned, size_t count);

// Returns true if DMA transmission is currently active.
bool ws2812_driver_is_busy(void);

#ifdef __cplusplus
}
#endif

#endif // CELESTIAL_WS2812_DRIVER_H

