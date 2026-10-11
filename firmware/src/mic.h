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

#include <stddef.h>

#define MIC_SAMPLE_RATE         16000
#define MIC_MAX_RECORD_SECONDS  5
#define MIC_RECORD_MAX_SAMPLES  (MIC_SAMPLE_RATE * MIC_MAX_RECORD_SECONDS) // 80,000 samples (160 KB)

/**
 * Start capturing continuous PCM16 microphone samples into memory.
 * Resets recorded sample count and enables audio accumulation.
 */
void mic_record_start(void);

/**
 * Stop capturing microphone samples.
 * Returns the total number of valid samples recorded.
 */
size_t mic_record_stop(void);

/**
 * Query if recording is currently active.
 */
bool mic_is_recording(void);

/**
 * Get pointer to recorded 16-bit PCM buffer and optional sample count.
 */
const int16_t *mic_get_recorded_samples(size_t *out_sample_count);

/**
 * Get recorded sample count.
 */
size_t mic_get_recorded_count(void);

/**
 * Get current recorded duration in milliseconds while recording or after stop.
 */
uint32_t mic_get_recorded_duration_ms(void);

/**
 * Clear recorded buffer.
 */
void mic_record_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* CELESTIAL_MIC_H */
