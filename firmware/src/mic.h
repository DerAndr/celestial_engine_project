#ifndef CELESTIAL_MIC_H
#define CELESTIAL_MIC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize I2S microphone subsystem for INMP441 MEMS microphone.
 * Uses PIO0 State Machine 0.
 * Pins: GP10 (SCK), GP11 (WS), GP12 (SD).
 * Configures continuous circular DMA buffer at 16,000 Hz.
 */
void mic_init(void);

/**
 * Update rolling audio amplitude metrics.
 * Call frequently from main loop.
 */
void mic_update(void);

/**
 * Get current normalized audio volume level (0 to 100%).
 * Includes smooth envelope decay for responsive VU meter rendering.
 */
uint8_t mic_get_level_percent(void);

/**
 * Get raw instantaneous peak amplitude (0 to 32767).
 */
int16_t mic_get_peak_amplitude(void);

#ifdef __cplusplus
}
#endif

#endif /* CELESTIAL_MIC_H */
