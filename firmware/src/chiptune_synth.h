#ifndef CELESTIAL_CHIPTUNE_SYNTH_H
#define CELESTIAL_CHIPTUNE_SYNTH_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "music_data.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float freq;
    uint32_t dur_ms;
} chiptune_note_t;

typedef enum {
    TRACK_TYPE_ADPCM = 0,
    TRACK_TYPE_NOTES
} chiptune_track_type_t;

typedef struct {
    const char *name;
    chiptune_track_type_t type;
    int adpcm_id;
    const chiptune_note_t *notes;
    size_t count;
} chiptune_track_t;

void chiptune_synth_init(void);

// Track registry (24 tracks: 21 retro chiptunes + 3 ADPCM master tracks)
int chiptune_synth_get_count(void);
const char* chiptune_synth_get_name(int track_id);
const chiptune_track_t* chiptune_synth_get_track(int track_id);

// Chiptune Playback
void chiptune_synth_play(int track_id);
void chiptune_synth_stop(void);
bool chiptune_synth_is_playing(void);
int chiptune_synth_get_active_track(void);
float chiptune_synth_get_current_freq(void);

// Cosmic Sci-Fi SFX (14 sound effect presets)
int chiptune_synth_get_sfx_count(void);
const char* chiptune_synth_get_sfx_name(int sfx_id);
void chiptune_synth_play_sfx(int sfx_id);
void chiptune_synth_stop_sfx(void);
bool chiptune_synth_is_sfx_playing(void);

// Non-blocking sequencer update (called periodically from audio_update)
void chiptune_synth_update(void);

// Sample-accurate synthesis (called per audio frame inside fill_audio_buffer)
// Generates warm retro arcade waveform (fundamental + 3rd + 5th harmonics with anti-click envelope)
// Returns true if a sample was rendered, placing it into *out_sample.
bool chiptune_synth_render_sample(float *out_sample);

#ifdef __cplusplus
}
#endif

#endif /* CELESTIAL_CHIPTUNE_SYNTH_H */

