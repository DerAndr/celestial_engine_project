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
 * Configures double-buffered DMA at 16,000 frames/s, 32-bit stereo slots.
 * Carries signed 24-bit PCM left aligned in each slot.
 */
void audio_init(void);

/**
 * Play a synthesized tone (frequency in Hz, duration in milliseconds).
 * Applies a soft envelope and the same master gain as voice playback.
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
 * 16-bit PCM playback functions (16 kHz mono signed PCM).
 * Plays raw recorded speech or external incoming voice stream.
 */
void audio_play_pcm16(const int16_t *samples, size_t count);
void audio_stop_pcm16(void);
bool audio_is_pcm16_playing(void);

/**
 * Streaming PCM16 ring buffer functions.
 * Allows infinite-duration streaming playback without length truncation.
 */
void audio_stream_reset(void);
bool audio_stream_push(const int16_t *samples, size_t count);
void audio_stream_finish(void);
bool audio_is_stream_playing(void);

/**
 * Non-blocking sequencer update; diagnostic builds keep silent I2S clocks running.
 * DMA IRQ0 fills ping-pong buffers; call this frequently from main loop.
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

/**
 * Cosmic Sci-Fi Sound FX Demo Suite
 */
typedef enum {
    COSMIC_SFX_WARP = 0,     // 1. Hyperdrive / Warp jump (rising sweep)
    COSMIC_SFX_PULSAR,       // 2. Pulsar beacon (rhythmic cosmic ping)
    COSMIC_SFX_ALIEN,        // 3. Alien communicator / scanner (sci-fi arpeggio)
    COSMIC_SFX_LASER,        // 4. Photon laser blaster (downward chirp)
    COSMIC_SFX_TRACTOR,      // 5. Gravitational tractor beam (vibrato hum)
    COSMIC_SFX_PORTAL,       // 6. Stargate portal (harmonic shimmer)
    COSMIC_SFX_SUPERNOVA,    // 7. Supernova rumble (deep sub-bass rumble)
    COSMIC_SFX_COUNT
} cosmic_sfx_id_t;

int audio_get_cosmic_sfx_count(void);
const char* audio_get_cosmic_sfx_name(int sfx_id);
void audio_play_cosmic_sfx(int sfx_id);
void audio_stop_cosmic_sfx(void);
bool audio_is_cosmic_sfx_playing(void);

/* Fault-isolation controls: fixed 440 Hz sine, stop all sources, IRQ/FIFO stats. */
void audio_play_diagnostic_tone(void);
/* 1 s 440 Hz at current volume; polarity > 0: all samples >= 0, < 0: all samples <= 0. */
void audio_play_polarity_tone(int polarity);
void audio_stop_all(void);
void audio_print_diagnostics(void);

#ifdef __cplusplus
}
#endif

#endif /* CELESTIAL_AUDIO_H */
