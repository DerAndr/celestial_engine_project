#ifndef CELESTIAL_AUDIO_H
#define CELESTIAL_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize I2S audio subsystem for MAX98357A amplifier.
 * Uses PIO1 State Machine 0.
 * Pins: GP16 (BCLK), GP17 (LRC), GP18 (DIN).
 * Configures double-buffered DMA streaming at 16,000 Hz.
 */
void audio_init(void);

/**
 * Play a synthesized tone (frequency in Hz, duration in milliseconds).
 */
void audio_play_tone(float freq_hz, uint32_t duration_ms);

/**
 * Play a multi-note celestial chime (harmonic chord).
 */
void audio_play_chime(void);

/**
 * Play an alert / click feedback blip.
 */
void audio_play_blip(void);

/**
 * Non-blocking audio update. Fills DMA ping-pong buffers.
 * Call frequently from main loop.
 */
void audio_update(void);

/**
 * Check if audio is currently playing.
 */
bool audio_is_playing(void);

/**
 * Set master audio volume percentage (0 to 100).
 */
void audio_set_volume(uint8_t volume_percent);

/**
 * Get current master audio volume percentage (0 to 100).
 */
uint8_t audio_get_volume(void);

#ifdef __cplusplus
}
#endif

#endif /* CELESTIAL_AUDIO_H */
