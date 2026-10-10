#ifndef CELESTIAL_AUDIO_H
#define CELESTIAL_AUDIO_H

#include <stdbool.h>
#include <stdint.h>
#include "voice_data.h"

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
 * Automatically applies frequency-weighted Fletcher-Munson equal-loudness scaling
 * and soft raised-cosine envelope to eliminate harshness and clicks.
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
 * Voice speech playback functions.
 * Plays digitized speech clips stored in flash.
 */
void audio_play_voice(voice_phrase_id_t phrase_id);
void audio_stop_voice(void);
bool audio_is_voice_playing(void);
const char* audio_get_voice_name(voice_phrase_id_t phrase_id);
int audio_get_voice_count(void);

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

/**
 * 8-bit chiptune jukebox controls
 */
int audio_get_chiptune_count(void);
const char* audio_get_chiptune_name(int track_id);
void audio_play_chiptune(int track_id);
void audio_stop_chiptune(void);
bool audio_is_chiptune_playing(void);
float audio_get_current_note_freq(void);

#ifdef __cplusplus
}
#endif

#endif /* CELESTIAL_AUDIO_H */
