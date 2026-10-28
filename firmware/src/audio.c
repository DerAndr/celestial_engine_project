#include "audio.h"
#include "config.h"
#include "voice_data.h"
#include "music_data.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include "audio_i2s.pio.h"
#include <math.h>

#define AUDIO_SAMPLE_RATE     16000
#define AUDIO_BUFFER_SAMPLES  512    // 32 ms per buffer
#define AUDIO_BUFFER_WORDS    AUDIO_BUFFER_SAMPLES // One 32-bit word per stereo frame (16-bit L + 16-bit R)
#define AUDIO_DMA_RING_BITS   11 // 2048-byte read ring per channel (512 words * 4 bytes)
_Static_assert(AUDIO_BUFFER_WORDS * sizeof(uint32_t) == (1u << AUDIO_DMA_RING_BITS),
               "DMA read ring must exactly cover one audio buffer");
#if AUDIO_I2S_SLOT_BITS != 16
#error "audio_i2s.pio and the PCM buffer layout require 16-bit channel slots"
#endif

static PIO audio_pio = pio1;
static uint audio_sm = 0;
static uint audio_offset = 0;

static int dma_chan_a = -1;
static int dma_chan_b = -1;

_Alignas(1u << AUDIO_DMA_RING_BITS) static uint32_t buf_a[AUDIO_BUFFER_WORDS];
_Alignas(1u << AUDIO_DMA_RING_BITS) static uint32_t buf_b[AUDIO_BUFFER_WORDS];

// Percentage controls digital gain. Acoustic level must be verified on the actual assembly.
static volatile uint8_t master_volume_percent = AUDIO_DEFAULT_VOLUME;
static volatile float master_gain = 0.0f;
static bool amp_is_sleeping = true;
static absolute_time_t silence_since;
static void amp_sleep(void);
static void amp_wake(void);
static volatile bool diagnostic_sine = false;
// Fault isolation (exp. 016): 0 = normal signed sine, +1 = samples >= 0 only, -1 = samples <= 0 only.
static volatile int diagnostic_polarity = 0;
static volatile uint32_t max_refill_us = 0;
static volatile uint32_t tx_stall_observations = 0;
static volatile uint32_t dma_restarts = 0;
static volatile uint32_t late_refills = 0;
static volatile uint32_t playback_peak_pcm24 = 0;
static bool dma_debug_started = false;


// Tone synthesizer state
static volatile float current_amplitude = 0.0f;
static volatile float current_freq = 0.0f;
static float current_phase = 0.0f;
static volatile uint32_t total_note_samples = 0;
static volatile uint32_t samples_played = 0;

// Voice speech player state
static const voice_clip_t * volatile active_voice_clip = NULL;
static volatile size_t voice_sample_idx = 0;
static volatile bool voice_active = false;

// ADPCM Music player state
static const music_adpcm_clip_t * volatile active_adpcm_clip = NULL;
static volatile size_t adpcm_sample_idx = 0;
static volatile int adpcm_predicted = 0;
static volatile int adpcm_step_idx = 0;
static volatile bool adpcm_active = false;
static volatile audio_play_mode_t music_play_mode = AUDIO_PLAY_SINGLE;
static volatile int current_music_track_idx = -1;

static const int8_t adpcm_index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

static const int16_t adpcm_step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544,
    598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878,
    2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
    6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818,
    18500, 20350, 22385, 24623, 27086, 29794, 32767
};

// 16-bit PCM playback state (for voice recording review and external speech)
static const int16_t * volatile active_pcm16_samples = NULL;
static volatile size_t pcm16_sample_count = 0;
static volatile size_t pcm16_sample_idx = 0;
static volatile bool pcm16_active = false;

// Streaming 16-bit PCM ring buffer state (for infinite-duration live server speech playback)
#define AUDIO_STREAM_RING_SIZE   65536 // 65,536 samples (131 KB = 4.096 seconds)
#define AUDIO_STREAM_RING_MASK   (AUDIO_STREAM_RING_SIZE - 1)
static int16_t audio_stream_ring[AUDIO_STREAM_RING_SIZE];
static volatile uint32_t audio_stream_write_idx = 0;
static volatile uint32_t audio_stream_read_idx = 0;
static volatile bool audio_stream_active = false;
static volatile bool audio_stream_eof = false;
static absolute_time_t last_stream_packet_time;

// Celestial chime sequence state (warm gentle octave 4 harmonics: C4, E4, G4, C5)
typedef struct {
    float freq;
    uint32_t dur_ms;
} chime_note_t;

static const chime_note_t CHIME_SEQUENCE[] = {
    { 261.63f, 70 }, // C4 - warm, deep, gentle
    { 329.63f, 70 }, // E4 - mellow
    { 392.00f, 70 }, // G4 - harmonic
    { 523.25f, 150 } // C5 - soft peaceful resolution
};
static int chime_index = -1;
static absolute_time_t next_note_time;

// =========================================================================
// 8-bit Chiptune Tracks (Extended retro gaming classics)
// =========================================================================

// 1. Super Mario Bros (Main Theme - Full Overworld Intro & Verses)
static const chime_note_t MARIO_THEME[] = {
    // Intro fanfare
    { 659.25f, 120 }, { 659.25f, 120 }, { 0.0f, 120 },   { 659.25f, 120 },
    { 0.0f, 120 },   { 523.25f, 120 }, { 659.25f, 120 }, { 783.99f, 240 },
    { 0.0f, 240 },   { 392.00f, 240 }, { 0.0f, 240 },
    // Main Verse A
    { 523.25f, 180 }, { 0.0f, 90 },    { 392.00f, 180 }, { 0.0f, 90 },
    { 329.63f, 180 }, { 0.0f, 90 },    { 440.00f, 150 }, { 493.88f, 150 },
    { 466.16f, 120 }, { 440.00f, 180 }, { 392.00f, 140 }, { 659.25f, 160 },
    { 783.99f, 160 }, { 880.00f, 180 }, { 698.46f, 130 }, { 783.99f, 150 },
    { 0.0f, 90 },    { 659.25f, 160 }, { 523.25f, 130 }, { 587.33f, 130 },
    { 493.88f, 220 }, { 0.0f, 100 },
    // Main Verse B (Second repeating theme)
    { 523.25f, 180 }, { 0.0f, 90 },    { 392.00f, 180 }, { 0.0f, 90 },
    { 329.63f, 180 }, { 0.0f, 90 },    { 440.00f, 150 }, { 493.88f, 150 },
    { 466.16f, 120 }, { 440.00f, 180 }, { 392.00f, 140 }, { 659.25f, 160 },
    { 783.99f, 160 }, { 880.00f, 180 }, { 698.46f, 130 }, { 783.99f, 150 },
    { 0.0f, 90 },    { 659.25f, 160 }, { 523.25f, 130 }, { 587.33f, 130 },
    { 493.88f, 260 }, { 0.0f, 120 },
    // Bridge cadence
    { 783.99f, 120 }, { 739.99f, 120 }, { 698.46f, 120 }, { 622.25f, 120 },
    { 659.25f, 160 }, { 0.0f, 80 },    { 415.30f, 120 }, { 440.00f, 120 },
    { 523.25f, 120 }, { 0.0f, 80 },    { 440.00f, 120 }, { 523.25f, 120 },
    { 587.33f, 120 }, { 0.0f, 120 },   { 523.25f, 280 }
};

// 2. Tetris (Korobeiniki - Extended Stanzas A, B & Folk Bridge)
static const chime_note_t TETRIS_THEME[] = {
    // Stanza A
    { 659.25f, 220 }, { 493.88f, 110 }, { 523.25f, 110 }, { 587.33f, 220 },
    { 523.25f, 110 }, { 493.88f, 110 }, { 440.00f, 220 }, { 440.00f, 110 },
    { 523.25f, 110 }, { 659.25f, 220 }, { 587.33f, 110 }, { 523.25f, 110 },
    { 493.88f, 330 }, { 523.25f, 110 }, { 587.33f, 220 }, { 659.25f, 220 },
    { 523.25f, 220 }, { 440.00f, 220 }, { 440.00f, 330 }, { 0.0f, 110 },
    // Stanza B (High octave run)
    { 587.33f, 220 }, { 698.46f, 110 }, { 880.00f, 220 }, { 783.99f, 110 },
    { 698.46f, 110 }, { 659.25f, 330 }, { 523.25f, 110 }, { 659.25f, 220 },
    { 587.33f, 110 }, { 523.25f, 110 }, { 493.88f, 220 }, { 493.88f, 110 },
    { 523.25f, 110 }, { 587.33f, 220 }, { 659.25f, 220 }, { 523.25f, 220 },
    { 440.00f, 220 }, { 440.00f, 330 }, { 0.0f, 110 },
    // Folk Russian bass cadence
    { 329.63f, 200 }, { 261.63f, 200 }, { 293.66f, 200 }, { 246.94f, 200 },
    { 261.63f, 200 }, { 220.00f, 200 }, { 207.65f, 200 }, { 246.94f, 200 },
    { 329.63f, 200 }, { 261.63f, 200 }, { 293.66f, 200 }, { 246.94f, 200 },
    { 261.63f, 200 }, { 329.63f, 200 }, { 440.00f, 350 }, { 415.30f, 350 },
    // Final joyful resolution
    { 659.25f, 180 }, { 493.88f, 90 },  { 523.25f, 90 },  { 587.33f, 180 },
    { 523.25f, 90 },  { 493.88f, 90 },  { 440.00f, 280 }
};

// 3. Doom E1M1 (At Doom's Gate - Driving Metal Riff & Bridge)
static const chime_note_t DOOM_THEME[] = {
    // Main Riff Part 1
    { 164.81f, 85 }, { 164.81f, 85 }, { 329.63f, 110 }, { 164.81f, 85 },
    { 164.81f, 85 }, { 293.66f, 110 }, { 164.81f, 85 }, { 164.81f, 85 },
    { 261.63f, 110 }, { 164.81f, 85 }, { 164.81f, 85 }, { 233.08f, 130 },
    { 164.81f, 85 }, { 164.81f, 85 }, { 246.94f, 110 }, { 261.63f, 110 },
    // Main Riff Part 2
    { 164.81f, 85 }, { 164.81f, 85 }, { 329.63f, 110 }, { 164.81f, 85 },
    { 164.81f, 85 }, { 293.66f, 110 }, { 164.81f, 85 }, { 164.81f, 85 },
    { 261.63f, 110 }, { 164.81f, 85 }, { 164.81f, 85 }, { 233.08f, 160 },
    // Bridge in G
    { 196.00f, 85 }, { 196.00f, 85 }, { 392.00f, 110 }, { 196.00f, 85 },
    { 196.00f, 85 }, { 349.23f, 110 }, { 196.00f, 85 }, { 196.00f, 85 },
    { 329.63f, 110 }, { 196.00f, 85 }, { 196.00f, 85 }, { 293.66f, 130 },
    { 196.00f, 85 }, { 196.00f, 85 }, { 311.13f, 110 }, { 329.63f, 110 },
    // Bridge in C
    { 130.81f, 85 }, { 130.81f, 85 }, { 261.63f, 110 }, { 130.81f, 85 },
    { 130.81f, 85 }, { 233.08f, 110 }, { 130.81f, 85 }, { 130.81f, 85 },
    { 207.65f, 110 }, { 130.81f, 85 }, { 130.81f, 85 }, { 174.61f, 160 },
    // Final Riff punch
    { 164.81f, 85 }, { 164.81f, 85 }, { 329.63f, 110 }, { 164.81f, 85 },
    { 164.81f, 85 }, { 293.66f, 110 }, { 164.81f, 85 }, { 164.81f, 85 },
    { 261.63f, 110 }, { 164.81f, 85 }, { 164.81f, 85 }, { 233.08f, 220 }
};

// 4. The Legend of Zelda (Overworld March - Fanfare & Main Theme)
static const chime_note_t ZELDA_THEME[] = {
    // Fanfare Intro
    { 466.16f, 260 }, { 349.23f, 180 }, { 0.0f, 80 },    { 466.16f, 140 },
    { 466.16f, 90 },  { 523.25f, 90 },  { 587.33f, 90 },  { 622.25f, 90 },
    // Main Overworld Theme
    { 698.46f, 450 }, { 0.0f, 120 },   { 698.46f, 130 }, { 698.46f, 90 },
    { 739.99f, 90 },  { 830.61f, 90 },  { 932.33f, 450 }, { 0.0f, 120 },
    { 932.33f, 130 }, { 932.33f, 90 },  { 830.61f, 90 },  { 739.99f, 90 },
    { 830.61f, 260 }, { 739.99f, 130 }, { 698.46f, 450 }, { 0.0f, 100 },
    { 622.25f, 130 }, { 622.25f, 90 },  { 698.46f, 90 },  { 739.99f, 260 },
    { 698.46f, 130 }, { 622.25f, 130 }, { 554.37f, 130 }, { 523.25f, 130 },
    { 587.33f, 130 }, { 659.25f, 260 }, { 783.99f, 180 }, { 698.46f, 400 },
    // Bridge & Heroic Resolution
    { 698.46f, 140 }, { 622.25f, 140 }, { 554.37f, 140 }, { 523.25f, 140 },
    { 466.16f, 280 }, { 523.25f, 140 }, { 587.33f, 140 }, { 622.25f, 140 },
    { 698.46f, 140 }, { 783.99f, 140 }, { 880.00f, 280 }, { 932.33f, 500 }
};

// 5. Star Wars (Imperial March - Darth Vader's Theme)
static const chime_note_t IMPERIAL_THEME[] = {
    // First Phrase
    { 392.00f, 320 }, { 392.00f, 320 }, { 392.00f, 320 }, { 311.13f, 240 },
    { 466.16f, 80 },  { 392.00f, 320 }, { 311.13f, 240 }, { 466.16f, 80 },
    { 392.00f, 600 }, { 0.0f, 120 },
    // Second Phrase
    { 587.33f, 320 }, { 587.33f, 320 }, { 587.33f, 320 }, { 622.25f, 240 },
    { 466.16f, 80 },  { 369.99f, 320 }, { 311.13f, 240 }, { 466.16f, 80 },
    { 392.00f, 600 }, { 0.0f, 120 },
    // Climax & Modulation
    { 783.99f, 320 }, { 392.00f, 240 }, { 392.00f, 80 },  { 783.99f, 320 },
    { 739.99f, 240 }, { 698.46f, 80 },  { 659.25f, 90 },  { 622.25f, 90 },
    { 659.25f, 220 }, { 0.0f, 120 },   { 415.30f, 180 }, { 554.37f, 320 },
    { 523.25f, 240 }, { 493.88f, 80 },  { 466.16f, 90 },  { 440.00f, 90 },
    { 466.16f, 220 }, { 0.0f, 120 },   { 311.13f, 180 }, { 369.99f, 320 },
    { 311.13f, 240 }, { 466.16f, 80 },  { 392.00f, 650 }
};

// 6. Ghostbusters (Instant Iconic Lead Hook & Retro Chiptune Theme)
static const chime_note_t GHOSTBUSTERS_THEME[] = {
    // 1. Instant Iconic Brass Hook: "Duh-duh DAH! Duh DAH! DAH!"
    {  493.88f,  120 }, {    0.00f,   30 }, {  493.88f,  120 }, {    0.00f,   30 },
    {  622.25f,  240 }, {  493.88f,  120 }, {  554.37f,  240 }, {  440.00f,  360 }, {    0.00f,  120 },
    // Syncopated Response: "Duh-duh-duh-duh duh DUH!"
    {  493.88f,  120 }, {    0.00f,   30 }, {  493.88f,  120 }, {    0.00f,   30 },
    {  493.88f,  120 }, {  493.88f,  120 }, {  440.00f,  240 }, {  493.88f,  360 }, {    0.00f,  120 },

    // Hook Reprise
    {  493.88f,  120 }, {    0.00f,   30 }, {  493.88f,  120 }, {    0.00f,   30 },
    {  622.25f,  240 }, {  493.88f,  120 }, {  554.37f,  240 }, {  440.00f,  360 }, {    0.00f,  120 },
    {  493.88f,  120 }, {    0.00f,   30 }, {  493.88f,  120 }, {    0.00f,   30 },
    {  493.88f,  120 }, {  493.88f,  120 }, {  440.00f,  240 }, {  554.37f,  120 }, {  493.88f,  480 }, {    0.00f,  160 },

    // 2. Vocal Verse 1: "If there's something strange in your neighborhood"
    {  493.88f,  140 }, {  493.88f,  140 }, {  493.88f,  140 }, {  493.88f,  140 },
    {  440.00f,  180 }, {  493.88f,  450 }, {    0.00f,  140 },

    // "Who you gonna call?!"
    {  493.88f,  180 }, {  587.33f,  220 }, {  493.88f,  180 }, {  587.33f,  480 }, {    0.00f,  160 },

    // "GHOSTBUSTERS!" (Punchy brass shout)
    {  493.88f,  140 }, {  440.00f,  140 }, {  493.88f,  240 }, {  493.88f,  240 }, {  493.88f,  500 }, {    0.00f,  160 },

    // 3. Stabs: "I ain't afraid of no ghost!"
    {  587.33f,  140 }, {  493.88f,  140 }, {  493.88f,  140 }, {  493.88f,  140 }, {  493.88f,  480 }, {    0.00f,  100 },
    {  739.99f,  360 }, {    0.00f,   50 }, {  739.99f,  180 }, {  739.99f,  260 }, {    0.00f,  140 },

    // 4. Vocal Verse 2: "If there's something weird and it don't look good"
    {  493.88f,  140 }, {  493.88f,  140 }, {  493.88f,  140 }, {  493.88f,  140 },
    {  440.00f,  180 }, {  493.88f,  450 }, {    0.00f,  140 },

    // "Who you gonna call?!"
    {  493.88f,  180 }, {  587.33f,  220 }, {  493.88f,  180 }, {  587.33f,  480 }, {    0.00f,  160 },

    // "GHOSTBUSTERS!"
    {  493.88f,  140 }, {  440.00f,  140 }, {  493.88f,  240 }, {  493.88f,  240 }, {  493.88f,  500 }, {    0.00f,  160 },

    // "I ain't afraid of no ghost!"
    {  587.33f,  140 }, {  493.88f,  140 }, {  493.88f,  140 }, {  493.88f,  140 }, {  493.88f,  480 }, {    0.00f,  100 },
    {  739.99f,  360 }, {    0.00f,   50 }, {  739.99f,  180 }, {  739.99f,  260 }, {    0.00f,  140 },

    // 5. Grand Climax Finale: "Who you gonna call?!"
    {  493.88f,  180 }, {  587.33f,  220 }, {  493.88f,  180 }, {  587.33f,  480 }, {    0.00f,  160 },

    // Final Fanfare: "GHOST - BUS - TERS!"
    {  493.88f,  150 }, {  493.88f,  150 }, {  622.25f,  240 }, {  493.88f,  150 },
    {  659.25f,  300 }, {  739.99f,  950 }
};

// 7. The Elder Scrolls III: Morrowind (Main Theme - from Morrowind - Main Theme.mid)
static const chime_note_t MORROWIND_THEME[] = {
    // Opening Horn Call
    {  261.63f,  272 }, // C4
    {  293.66f,  272 }, // D4
    {  311.13f, 1090 }, // D#4 (long rising horn)
    {  311.13f,  272 }, // D#4
    {  349.23f,  272 }, // F4
    {  392.00f, 1090 }, // G4
    {  392.00f,  272 }, // G4
    {  466.16f,  272 }, // A#4
    {  349.23f,  818 }, // F4
    {  392.00f,  136 }, // G4
    {  349.23f,  136 }, // F4
    {  311.13f,  272 }, // D#4
    {  293.66f,  272 }, // D4
    {  261.63f, 1090 }, // C4 (restful resolution)
    // Second Phrase
    {  261.63f,  272 }, // C4
    {  293.66f,  272 }, // D4
    {  311.13f, 1090 }, // D#4
    {  311.13f,  272 }, // D#4
    {  349.23f,  272 }, // F4
    {  392.00f, 1090 }, // G4
    {  392.00f,  272 }, // G4
    {  466.16f,  272 }, // A#4
    {  523.25f, 1090 }, // C5
    {  466.16f,  409 }, // A#4
    {  587.33f,  136 }, // D5
    {  523.25f, 1090 }, // C5
    // Grand Climax (Full Strings / Brass Swell)
    {  523.25f,  409 }, // C5
    {  587.33f,   68 }, // D5
    {  622.25f,  545 }, // D#5
    {  587.33f,  545 }, // D5
    {  523.25f,  545 }, // C5
    {  466.16f,  545 }, // A#4
    {  415.30f,  545 }, // G#4
    {  392.00f,  545 }, // G4
    {  349.23f, 1090 }, // F4
    {  311.13f,  409 }, // D#4
    {  392.00f,  136 }, // G4
    {  349.23f, 1090 }, // F4
    {  311.13f,  272 }, // D#4
    {  293.66f,  272 }, // D4
    {  261.63f, 2200 }  // C4 (grand eternal sustain)
};

// 8. Spooky Scary Skeletons (from Spooky Scary Skeletons - Octet - Octet - Corgeous.mid, cut first 20s)
static const chime_note_t SPOOKY_SKELETONS_THEME[] = {
    // Main Lead Hook (starts at 22s after intro)
    {  415.30f,  176 }, // G#4
    {    0.00f,  176 }, // REST
    {  415.30f,  352 }, // G#4
    {  392.00f,  176 }, // G4
    {    0.00f,  176 }, // REST
    {  392.00f,  352 }, // G4
    {  261.63f,  352 }, // C4
    {  311.13f,  176 }, // D#4
    {  261.63f,  352 }, // C4
    {    0.00f,  176 }, // REST
    {  261.63f,  352 }, // C4
    {  311.13f,  352 }, // D#4
    {  349.23f,  352 }, // F4
    {  293.66f,  352 }, // D4
    {  311.13f,  352 }, // D#4
    {  261.63f,  705 }, // C4
    {    0.00f,  350 }, // REST
    // Hook Reprise
    {  415.30f,  176 }, // G#4
    {    0.00f,  176 }, // REST
    {  415.30f,  352 }, // G#4
    {  392.00f,  176 }, // G4
    {    0.00f,  176 }, // REST
    {  392.00f,  352 }, // G4
    {  261.63f,  352 }, // C4
    {  311.13f,  176 }, // D#4
    {  261.63f,  352 }, // C4
    {    0.00f,  176 }, // REST
    {  261.63f,  352 }, // C4
    {  415.30f,  176 }, // G#4
    {  415.30f,  528 }, // G#4
    {  392.00f,  352 }, // G4
    {  392.00f,  352 }, // G4
    {  261.63f,  705 }, // C4
    {    0.00f,  350 }, // REST
    // Bridge Variation
    {  392.00f,  352 }, // G4
    {  415.30f,  176 }, // G#4
    {    0.00f,  176 }, // REST
    {  415.30f,  352 }, // G#4
    {  392.00f,  176 }, // G4
    {  392.00f,  528 }, // G4
    {  261.63f,  352 }, // C4
    {  311.13f,  176 }, // D#4
    {  261.63f,  352 }, // C4
    {    0.00f,  176 }, // REST
    {  261.63f,  176 }, // C4
    {  293.66f,  176 }, // D4
    {  311.13f,  352 }, // D#4
    {  349.23f,  352 }, // F4
    {  293.66f,  352 }, // D4
    {  311.13f,  352 }, // D#4
    {  261.63f,  705 }  // C4
};

// 9. Daft Punk - Da Funk (from Daft Punk - Da Funk.mid)
static const chime_note_t DAFT_PUNK_THEME[] = {
    // Iconic Da Funk 303 Acid Groove
    {  392.00f,  810 }, // G4
    {    0.00f,  270 }, // REST
    {  349.23f,  135 }, // F4
    {    0.00f,  135 }, // REST
    {  392.00f,  135 }, // G4
    {    0.00f,  135 }, // REST
    {  466.16f,  135 }, // A#4
    {    0.00f,  135 }, // REST
    {  293.66f, 1080 }, // D4
    {    0.00f,  270 }, // REST
    {  261.63f,  135 }, // C4
    {    0.00f,  135 }, // REST
    {  293.66f,  135 }, // D4
    {    0.00f,  135 }, // REST
    {  349.23f,  135 }, // F4
    {    0.00f,  135 }, // REST
    {  233.08f, 1080 }, // A#3
    {    0.00f,  270 }, // REST
    {  220.00f,  135 }, // A3
    {    0.00f,  135 }, // REST
    {  233.08f,  135 }, // A#3
    {    0.00f,  135 }, // REST
    {  293.66f,  135 }, // D4
    {    0.00f,  135 }, // REST
    {  196.00f, 1080 }, // G3
    {    0.00f,  270 }, // REST
    {  220.00f,  405 }, // A3
    {    0.00f,  135 }, // REST
    {  233.08f,  405 }, // A#3
    {    0.00f,  135 }, // REST
    // Reprise (Second pass with acid filter sweep)
    {  392.00f,  810 }, // G4
    {    0.00f,  270 }, // REST
    {  349.23f,  135 }, // F4
    {    0.00f,  135 }, // REST
    {  392.00f,  135 }, // G4
    {    0.00f,  135 }, // REST
    {  466.16f,  135 }, // A#4
    {    0.00f,  135 }, // REST
    {  293.66f, 1080 }, // D4
    {    0.00f,  270 }, // REST
    {  261.63f,  135 }, // C4
    {    0.00f,  135 }, // REST
    {  293.66f,  135 }, // D4
    {    0.00f,  135 }, // REST
    {  349.23f,  135 }, // F4
    {    0.00f,  135 }, // REST
    {  233.08f, 1080 }, // A#3
    {    0.00f,  270 }, // REST
    {  220.00f,  135 }, // A3
    {    0.00f,  135 }, // REST
    {  233.08f,  135 }, // A#3
    {    0.00f,  135 }, // REST
    {  293.66f,  135 }, // D4
    {    0.00f,  135 }, // REST
    {  196.00f, 1080 }, // G3
    {    0.00f,  270 }, // REST
    {  220.00f,  405 }, // A3
    {    0.00f,  135 }, // REST
    {  233.08f,  405 }, // A#3
    {  392.00f,  900 }  // G4 (punch final hit)
};

// 10. eBay Chaos Auction (Accelerating Bidding Frenzy & Gavel Finale)
static const chime_note_t EBAY_THEME[] = {
    // Auctioneer rhythmic chant
    { 261.63f, 110 }, { 329.63f, 110 }, { 392.00f, 110 }, { 523.25f, 140 },
    { 493.88f, 110 }, { 392.00f, 110 }, { 440.00f, 110 }, { 493.88f, 110 }, { 523.25f, 220 },
    // Bidding accelerates
    { 587.33f, 95 },  { 659.25f, 95 },  { 698.46f, 95 },  { 783.99f, 130 },
    { 698.46f, 95 },  { 659.25f, 95 },  { 587.33f, 95 },  { 523.25f, 190 },
    // Rapid chromatically rising bids: going once, going twice!
    { 523.25f, 70 },  { 554.37f, 70 },  { 587.33f, 70 },  { 622.25f, 70 },
    { 659.25f, 70 },  { 698.46f, 70 },  { 739.99f, 70 },  { 783.99f, 70 },
    { 830.61f, 70 },  { 880.00f, 70 },  { 987.77f, 70 },  { 1046.50f, 180 },
    // Gavel strike thuds!
    { 130.81f, 80 },  { 0.0f, 60 },     { 130.81f, 80 },  { 0.0f, 60 },     { 130.81f, 120 },
    // Cha-ching cash register bell fanfare!
    { 1864.66f, 90 }, { 2349.32f, 260 }, { 0.0f, 50 },   { 2093.00f, 420 }
};

// 11. Harry Potter (Hedwig's Theme - Iconic Celesta 16-bit Waltz)
static const chime_note_t HARRY_POTTER_THEME[] = {
    { 246.94f, 260 }, // B3
    { 329.63f, 390 }, // E4
    { 392.00f, 200 }, // G4
    { 369.99f, 200 }, // F#4
    { 329.63f, 390 }, // E4
    { 493.88f, 780 }, // B4
    { 440.00f, 580 }, // A4
    { 369.99f, 580 }, // F#4
    { 329.63f, 390 }, // E4
    { 392.00f, 200 }, // G4
    { 369.99f, 200 }, // F#4
    { 311.13f, 390 }, // D#4
    { 349.23f, 390 }, // F4
    { 246.94f, 780 }, // B3
    { 246.94f, 260 }, // B3
    { 329.63f, 390 }, // E4
    { 392.00f, 200 }, // G4
    { 369.99f, 200 }, // F#4
    { 329.63f, 390 }, // E4
    { 493.88f, 780 }, // B4
    { 587.33f, 520 }, // D5
    { 554.37f, 260 }, // C#5
    { 523.25f, 520 }, // C5
    { 415.30f, 260 }, // G#4
    { 523.25f, 390 }, // C5
    { 493.88f, 200 }, // B4
    { 466.16f, 200 }, // A#4
    { 246.94f, 390 }, // B3
    { 392.00f, 390 }, // G4
    { 329.63f, 850 }  // E4
};

// 12. Pokémon Theme (Original Anime Theme - "Gotta catch 'em all! POKÉMON!" Famous Chorus)
static const chime_note_t POKEMON_THEME[] = {
    // "POKÉMON!"
    {  440.00f,  240 }, // A4
    {  523.25f,  240 }, // C5
    {  587.33f,  480 }, // D5
    {    0.00f,  140 }, // REST
    // "(Gotta catch 'em all!)"
    {  440.00f,  150 }, // A4
    {  440.00f,  150 }, // A4
    {  523.25f,  200 }, // C5
    {  587.33f,  200 }, // D5
    {  587.33f,  200 }, // D5
    {  523.25f,  440 }, // C5
    {    0.00f,  140 }, // REST
    // "It's you and me"
    {  523.25f,  180 }, // C5
    {  523.25f,  180 }, // C5
    {  587.33f,  200 }, // D5
    {  587.33f,  200 }, // D5
    {  523.25f,  440 }, // C5
    {    0.00f,  140 }, // REST
    // "I know it's my destiny!"
    {  523.25f,  180 }, // C5
    {  587.33f,  180 }, // D5
    {  659.25f,  180 }, // E5
    {  698.46f,  240 }, // F5
    {  659.25f,  180 }, // E5
    {  587.33f,  180 }, // D5
    {  523.25f,  480 }, // C5
    {    0.00f,  160 }, // REST
    // "POKÉMON!"
    {  440.00f,  240 }, // A4
    {  523.25f,  240 }, // C5
    {  587.33f,  480 }, // D5
    {    0.00f,  140 }, // REST
    // "Oh, you're my best friend"
    {  587.33f,  200 }, // D5
    {  587.33f,  200 }, // D5
    {  523.25f,  200 }, // C5
    {  440.00f,  200 }, // A4
    {  349.23f,  440 }, // F4
    {    0.00f,  140 }, // REST
    // "In a world we must defend!"
    {  349.23f,  180 }, // F4
    {  392.00f,  180 }, // G4
    {  440.00f,  200 }, // A4
    {  440.00f,  200 }, // A4
    {  392.00f,  180 }, // G4
    {  349.23f,  180 }, // F4
    {  440.00f,  580 }, // A4
    {    0.00f,  180 }, // REST
    // "POKÉMON!"
    {  440.00f,  240 }, // A4
    {  523.25f,  240 }, // C5
    {  587.33f,  480 }, // D5
    {    0.00f,  140 }, // REST
    // "(Gotta catch 'em all!)"
    {  440.00f,  150 }, // A4
    {  440.00f,  150 }, // A4
    {  523.25f,  200 }, // C5
    {  587.33f,  200 }, // D5
    {  587.33f,  200 }, // D5
    {  523.25f,  440 }, // C5
    {    0.00f,  140 }, // REST
    // "A heart so true"
    {  523.25f,  180 }, // C5
    {  523.25f,  180 }, // C5
    {  587.33f,  200 }, // D5
    {  587.33f,  200 }, // D5
    {  523.25f,  440 }, // C5
    {    0.00f,  140 }, // REST
    // "Our courage will pull us through!"
    {  587.33f,  180 }, // D5
    {  587.33f,  180 }, // D5
    {  659.25f,  180 }, // E5
    {  698.46f,  240 }, // F5
    {  659.25f,  180 }, // E5
    {  587.33f,  180 }, // D5
    {  523.25f,  480 }, // C5
    {    0.00f,  160 }, // REST
    // "You teach me and I'll teach you"
    {  466.16f,  190 }, // Bb4
    {  698.46f,  190 }, // F5
    {  698.46f,  190 }, // F5
    {  698.46f,  190 }, // F5
    {  523.25f,  190 }, // C5
    {  698.46f,  190 }, // F5
    {  698.46f,  380 }, // F5
    {    0.00f,  140 }, // REST
    // "PO-KÉ-MON!"
    {  698.46f,  180 }, // F5
    {  698.46f,  180 }, // F5
    {  698.46f,  180 }, // F5
    {  659.25f,  580 }, // E5
    {    0.00f,  180 }, // REST
    // "Gotta catch 'em all!"
    {  440.00f,  150 }, // A4
    {  440.00f,  150 }, // A4
    {  523.25f,  200 }, // C5
    {  587.33f,  220 }, // D5
    {  587.33f,  400 }, // D5
    {    0.00f,  140 }, // REST
    // "Gotta catch 'em all!"
    {  440.00f,  150 }, // A4
    {  440.00f,  150 }, // A4
    {  523.25f,  200 }, // C5
    {  587.33f,  220 }, // D5
    {  698.46f,  240 }, // F5
    {  587.33f,  440 }, // D5
    {    0.00f,  140 }, // REST
    // "POKÉMON!"
    {  440.00f,  240 }, // A4
    {  523.25f,  240 }, // C5
    {  587.33f,  800 }  // D5
};

// 13. Sailor Moon (Moonlight Densetsu - Complete Intro, Verse & Chorus)
static const chime_note_t SAILOR_MOON_THEME[] = {
    // --- Part 1: Iconic Opening Fanfare (from Moonlight Legend.mid t=1.07s) ---
    {  392.00f,  214 }, // G4
    {  523.25f,  214 }, // C5
    {  622.25f,  214 }, // D#5
    {  783.99f,  428 }, // G5
    {  783.99f,  214 }, // G5
    {  698.46f,  428 }, // F5
    {  698.46f,  214 }, // F5
    {  622.25f,  214 }, // D#5
    {  587.33f,  428 }, // D5
    {  698.46f,  428 }, // F5
    {  493.88f,  214 }, // B4
    {  587.33f,  214 }, // D5
    {  698.46f,  214 }, // F5
    {  830.61f,  428 }, // G#5
    {  830.61f,  214 }, // G#5
    {  932.33f,  428 }, // A#5
    {  830.61f,  214 }, // G#5
    {  783.99f,  214 }, // G5
    {  698.46f,  214 }, // F5
    {  622.25f,  428 }, // D#5
    {  783.99f,  428 }, // G5
    {  392.00f,  214 }, // G4
    {  523.25f,  214 }, // C5
    {  622.25f,  214 }, // D#5
    { 1046.50f,  428 }, // C6
    { 1046.50f,  214 }, // C6
    {  987.77f,  428 }, // B5
    {  987.77f,  214 }, // B5
    {  932.33f,  428 }, // A#5
    {  830.61f,  428 }, // G#5
    {  830.61f,  214 }, // G#5
    {  783.99f,  214 }, // G5
    {  698.46f,  214 }, // F5
    {  783.99f,  428 }, // G5
    {  698.46f,  428 }, // F5
    {  622.25f,  214 }, // D#5
    {  587.33f,  428 }, // D5
    {  523.25f,  428 }, // C5
    {    0.00f,  200 }, // REST
    // --- Part 2: Verse 1 (Gomen ne sunao ja nakute) ---
    {  261.63f,  214 }, // C4
    {  311.13f,  214 }, // D#4
    {  392.00f,  428 }, // G4
    {  392.00f,  214 }, // G4
    {  349.23f,  428 }, // F4
    {  349.23f,  214 }, // F4
    {  311.13f,  214 }, // D#4
    {  311.13f,  214 }, // D#4
    {  293.66f,  428 }, // D4
    {  349.23f,  428 }, // F4
    {  293.66f,  214 }, // D4
    {  349.23f,  428 }, // F4
    {  392.00f,  214 }, // G4
    {  349.23f,  214 }, // F4
    {  311.13f,  214 }, // D#4
    {  293.66f,  428 }, // D4
    {  311.13f,  642 }, // D#4
    {  261.63f,  214 }, // C4
    {  311.13f,  214 }, // D#4
    {  392.00f,  428 }, // G4
    {  392.00f,  214 }, // G4
    {  349.23f,  428 }, // F4
    {  349.23f,  214 }, // F4
    {  311.13f,  214 }, // D#4
    {  311.13f,  214 }, // D#4
    {  293.66f,  428 }, // D4
    {  349.23f,  428 }, // F4
    {  293.66f,  214 }, // D4
    {  349.23f,  428 }, // F4
    {  349.23f,  214 }, // F4
    {  311.13f,  428 }, // D#4
    {  311.13f,  214 }, // D#4
    {  293.66f,  428 }, // D4
    {  261.63f,  428 }, // C4
    {  311.13f,  428 }, // D#4
    {    0.00f,  200 }, // REST
    // --- Part 3: Romantic Chorus (Tsuki no akari ni michibikare...) ---
    {  523.25f,  428 }, // C5
    {  830.61f, 1285 }, // G#5
    {  932.33f,  214 }, // A#5
    { 1046.50f,  642 }, // C6
    { 1046.50f,  428 }, // C6
    {  932.33f,  214 }, // A#5
    {  830.61f,  642 }, // G#5
    {  783.99f,  642 }, // G5
    {  698.46f,  214 }, // F5
    {  622.25f,  214 }, // D#5
    {  698.46f,  428 }, // F5
    {  783.99f, 1071 }, // G5
    {  523.25f,  428 }, // C5
    {  830.61f, 1285 }, // G#5
    {  932.33f,  214 }, // A#5
    { 1046.50f,  642 }, // C6
    { 1046.50f,  214 }, // C6
    { 1046.50f,  285 }, // C6
    {  932.33f,  285 }, // A#5
    {  830.61f,  285 }, // G#5
    {  783.99f, 2142 }  // G5
};

// Mega Man 2 (Dr. Wily Stage 1 - Driving 8-bit Synth)
static const chime_note_t MEGAMAN_THEME[] = {
    { 293.66f, 110 }, { 293.66f, 110 }, { 293.66f, 110 }, { 293.66f, 110 },
    { 466.16f, 180 }, { 440.00f, 180 }, { 392.00f, 180 }, { 349.23f, 180 },
    { 392.00f, 180 }, { 440.00f, 220 },
    { 293.66f, 110 }, { 293.66f, 110 }, { 293.66f, 110 }, { 293.66f, 110 },
    { 466.16f, 180 }, { 440.00f, 180 }, { 392.00f, 180 }, { 349.23f, 180 },
    { 329.63f, 180 }, { 293.66f, 220 },
    { 349.23f, 140 }, { 392.00f, 140 }, { 440.00f, 140 }, { 587.33f, 260 },
    { 523.25f, 180 }, { 466.16f, 180 }, { 440.00f, 180 }, { 392.00f, 180 },
    { 440.00f, 260 },
    { 349.23f, 140 }, { 392.00f, 140 }, { 440.00f, 140 }, { 587.33f, 260 },
    { 523.25f, 180 }, { 466.16f, 180 }, { 440.00f, 180 }, { 392.00f, 180 },
    { 349.23f, 180 }, { 293.66f, 350 }
};

// 7. Bella Ciao (Italian Folk Classic)
static const chime_note_t BELLA_CIAO[] = {
    { 329.63f, 150 }, { 440.00f, 150 }, { 493.88f, 150 }, { 523.25f, 240 }, { 440.00f, 240 },
    { 329.63f, 150 }, { 440.00f, 150 }, { 493.88f, 150 }, { 523.25f, 240 }, { 440.00f, 240 },
    { 329.63f, 140 }, { 440.00f, 140 }, { 493.88f, 140 }, { 523.25f, 140 }, { 493.88f, 140 },
    { 440.00f, 140 }, { 523.25f, 140 }, { 493.88f, 140 }, { 440.00f, 140 }, { 659.25f, 380 },
    { 659.25f, 180 }, { 659.25f, 180 }, { 587.33f, 140 }, { 659.25f, 140 }, { 698.46f, 240 }, { 698.46f, 240 },
    { 698.46f, 140 }, { 659.25f, 140 }, { 587.33f, 140 }, { 698.46f, 140 }, { 659.25f, 320 },
    { 659.25f, 140 }, { 587.33f, 140 }, { 523.25f, 140 }, { 493.88f, 180 }, { 659.25f, 180 },
    { 493.88f, 180 }, { 523.25f, 180 }, { 440.00f, 450 }
};

// 4. Ode to Joy (Beethoven - Symphony No. 9 / Universal Classical Anthem)
static const chime_note_t ODE_TO_JOY[] = {
    // Theme A1
    { 329.63f, 220 }, { 329.63f, 220 }, { 349.23f, 220 }, { 392.00f, 220 },
    { 392.00f, 220 }, { 349.23f, 220 }, { 329.63f, 220 }, { 293.66f, 220 },
    { 261.63f, 220 }, { 261.63f, 220 }, { 293.66f, 220 }, { 329.63f, 220 },
    { 329.63f, 330 }, { 293.66f, 110 }, { 293.66f, 440 }, { 0.0f, 110 },
    // Theme A2
    { 329.63f, 220 }, { 329.63f, 220 }, { 349.23f, 220 }, { 392.00f, 220 },
    { 392.00f, 220 }, { 349.23f, 220 }, { 329.63f, 220 }, { 293.66f, 220 },
    { 261.63f, 220 }, { 261.63f, 220 }, { 293.66f, 220 }, { 329.63f, 220 },
    { 293.66f, 330 }, { 261.63f, 110 }, { 261.63f, 440 }, { 0.0f, 110 },
    // Theme B (Bridge)
    { 293.66f, 220 }, { 293.66f, 220 }, { 329.63f, 220 }, { 261.63f, 220 },
    { 293.66f, 220 }, { 329.63f, 110 }, { 349.23f, 110 }, { 329.63f, 220 }, { 261.63f, 220 },
    { 293.66f, 220 }, { 329.63f, 110 }, { 349.23f, 110 }, { 329.63f, 220 }, { 293.66f, 220 },
    { 261.63f, 220 }, { 293.66f, 220 }, { 196.00f, 440 }, { 0.0f, 110 },
    // Theme A2 Reprise
    { 329.63f, 220 }, { 329.63f, 220 }, { 349.23f, 220 }, { 392.00f, 220 },
    { 392.00f, 220 }, { 349.23f, 220 }, { 329.63f, 220 }, { 293.66f, 220 },
    { 261.63f, 220 }, { 261.63f, 220 }, { 293.66f, 220 }, { 329.63f, 220 },
    { 293.66f, 330 }, { 261.63f, 110 }, { 261.63f, 500 }
};

// 5. Du liegst mir im Herzen (Traditional German Folk Melody)
static const chime_note_t GERMAN_FOLK[] = {
    // Verse 1
    { 493.88f, 220 }, { 392.00f, 220 }, { 293.66f, 220 }, { 392.00f, 440 }, { 369.99f, 220 },
    { 440.00f, 220 }, { 369.99f, 220 }, { 293.66f, 220 }, { 369.99f, 440 }, { 329.63f, 220 },
    { 293.66f, 220 }, { 392.00f, 220 }, { 493.88f, 220 }, { 587.33f, 440 }, { 523.25f, 220 },
    { 493.88f, 220 }, { 440.00f, 220 }, { 392.00f, 220 }, { 440.00f, 440 }, { 0.0f, 100 },
    // Verse 2
    { 493.88f, 220 }, { 392.00f, 220 }, { 293.66f, 220 }, { 392.00f, 440 }, { 369.99f, 220 },
    { 440.00f, 220 }, { 369.99f, 220 }, { 293.66f, 220 }, { 369.99f, 440 }, { 329.63f, 220 },
    { 293.66f, 220 }, { 392.00f, 220 }, { 493.88f, 220 }, { 587.33f, 440 }, { 523.25f, 220 },
    { 493.88f, 220 }, { 392.00f, 220 }, { 440.00f, 220 }, { 392.00f, 500 }, { 0.0f, 120 },
    // Chorus: Du, du liegst mir im Sinn
    { 587.33f, 440 }, { 493.88f, 220 }, { 392.00f, 440 }, { 0.0f, 100 },
    { 659.25f, 440 }, { 523.25f, 220 }, { 440.00f, 440 }, { 0.0f, 100 },
    { 587.33f, 220 }, { 523.25f, 220 }, { 493.88f, 220 }, { 440.00f, 220 }, { 493.88f, 220 }, { 523.25f, 220 },
    { 493.88f, 330 }, { 440.00f, 330 }, { 392.00f, 500 }
};

// 6. Mo Li Hua (Jasmine Flower - Traditional Chinese Folk Melody)
static const chime_note_t MO_LI_HUA[] = {
    // Hao yi duo mei li de mo li hua (Phrase 1)
    { 329.63f, 300 }, { 329.63f, 150 }, { 392.00f, 150 }, { 440.00f, 300 }, { 523.25f, 300 },
    { 440.00f, 150 }, { 392.00f, 150 }, { 440.00f, 300 }, { 392.00f, 450 }, { 0.0f, 150 },
    // Hao yi duo mei li de mo li hua (Phrase 2)
    { 329.63f, 300 }, { 329.63f, 150 }, { 392.00f, 150 }, { 440.00f, 300 }, { 523.25f, 300 },
    { 440.00f, 150 }, { 392.00f, 150 }, { 440.00f, 300 }, { 392.00f, 450 }, { 0.0f, 150 },
    // Fen fang mei li man zhi ya (Phrase 3)
    { 392.00f, 300 }, { 392.00f, 150 }, { 440.00f, 150 }, { 392.00f, 300 }, { 329.63f, 300 },
    { 293.66f, 300 }, { 329.63f, 150 }, { 392.00f, 150 }, { 329.63f, 300 }, { 293.66f, 450 }, { 0.0f, 150 },
    // You xiang you bai ren ren kua (Phrase 4)
    { 293.66f, 300 }, { 293.66f, 150 }, { 329.63f, 150 }, { 392.00f, 300 }, { 440.00f, 300 },
    { 523.25f, 150 }, { 440.00f, 150 }, { 392.00f, 300 }, { 440.00f, 150 }, { 392.00f, 150 }, { 329.63f, 450 }, { 0.0f, 150 },
    // Rang wo lai jiang ni zhai xia (Phrase 5)
    { 392.00f, 300 }, { 329.63f, 150 }, { 293.66f, 150 }, { 329.63f, 300 }, { 392.00f, 300 },
    { 440.00f, 450 }, { 392.00f, 150 }, { 329.63f, 300 }, { 293.66f, 450 }, { 0.0f, 150 },
    // Song gei bie ren jia / Mo li hua (Phrase 6)
    { 293.66f, 300 }, { 329.63f, 150 }, { 392.00f, 150 }, { 329.63f, 300 }, { 293.66f, 300 },
    { 261.63f, 300 }, { 293.66f, 150 }, { 329.63f, 150 }, { 293.66f, 600 }
};

// 7. Greensleeves (English / Celtic Traditional)
static const chime_note_t GREENSLEEVES[] = {
    { 349.23f, 180 }, { 415.30f, 360 }, { 466.16f, 180 }, { 523.25f, 280 }, { 554.37f, 100 },
    { 523.25f, 180 }, { 466.16f, 360 }, { 392.00f, 180 }, { 311.13f, 280 }, { 349.23f, 100 },
    { 392.00f, 180 }, { 415.30f, 360 }, { 349.23f, 180 }, { 349.23f, 280 }, { 329.63f, 100 },
    { 349.23f, 180 }, { 392.00f, 360 }, { 329.63f, 180 }, { 261.63f, 400 }
};

// 8. Sakura Sakura (Japanese Traditional)
static const chime_note_t SAKURA_SAKURA[] = {
    { 440.00f, 260 }, { 440.00f, 260 }, { 493.88f, 520 },
    { 440.00f, 260 }, { 440.00f, 260 }, { 493.88f, 520 },
    { 440.00f, 260 }, { 493.88f, 260 }, { 523.25f, 260 }, { 493.88f, 260 },
    { 440.00f, 260 }, { 493.88f, 260 }, { 440.00f, 260 }, { 349.23f, 520 },
    { 329.63f, 260 }, { 261.63f, 260 }, { 329.63f, 260 }, { 349.23f, 260 },
    { 329.63f, 260 }, { 329.63f, 260 }, { 261.63f, 260 }, { 246.94f, 520 }
};

// 9. La Cucaracha (Mexican Traditional)
static const chime_note_t LA_CUCARACHA[] = {
    { 261.63f, 110 }, { 261.63f, 110 }, { 261.63f, 110 }, { 349.23f, 320 }, { 440.00f, 240 },
    { 261.63f, 110 }, { 261.63f, 110 }, { 261.63f, 110 }, { 349.23f, 320 }, { 440.00f, 240 },
    { 349.23f, 140 }, { 349.23f, 110 }, { 329.63f, 110 }, { 329.63f, 110 }, { 293.66f, 110 }, { 293.66f, 110 }, { 261.63f, 420 },
    { 261.63f, 110 }, { 261.63f, 110 }, { 261.63f, 110 }, { 329.63f, 320 }, { 392.00f, 240 },
    { 261.63f, 110 }, { 261.63f, 110 }, { 261.63f, 110 }, { 329.63f, 320 }, { 392.00f, 240 },
    { 523.25f, 160 }, { 587.33f, 130 }, { 523.25f, 130 }, { 466.16f, 130 }, { 440.00f, 130 }, { 392.00f, 130 }, { 349.23f, 420 }
};

// =========================================================================
// Cosmic Sound FX Suites (Synthesized Sci-Fi Audio FX)
// =========================================================================

static const chime_note_t SFX_WARP[] = {
    { 110.0f, 60 }, { 146.83f, 50 }, { 196.00f, 45 }, { 261.63f, 40 },
    { 349.23f, 35 }, { 466.16f, 30 }, { 622.25f, 25 }, { 830.61f, 25 },
    { 1108.73f, 30 }, { 1479.98f, 40 }, { 1975.53f, 60 }, { 2489.02f, 120 }
};

static const chime_note_t SFX_PULSAR[] = {
    { 880.0f, 40 }, { 0.0f, 60 }, { 880.0f, 40 }, { 0.0f, 60 },
    { 1174.66f, 60 }, { 0.0f, 80 }, { 880.0f, 40 }, { 0.0f, 60 },
    { 1318.51f, 100 }
};

static const chime_note_t SFX_ALIEN[] = {
    { 587.33f, 35 }, { 880.00f, 35 }, { 1174.66f, 35 }, { 783.99f, 35 },
    { 1046.50f, 35 }, { 1396.91f, 35 }, { 932.33f, 35 }, { 1244.51f, 35 },
    { 1661.22f, 70 }
};

static const chime_note_t SFX_LASER[] = {
    { 1567.98f, 25 }, { 1174.66f, 25 }, { 880.00f, 25 }, { 659.25f, 25 },
    { 493.88f, 30 }, { 369.99f, 35 }, { 277.18f, 45 }, { 185.00f, 60 }
};

static const chime_note_t SFX_TRACTOR[] = {
    { 110.00f, 90 }, { 130.81f, 90 }, { 110.00f, 90 }, { 130.81f, 90 },
    { 110.00f, 90 }, { 146.83f, 110 }, { 110.00f, 140 }
};

static const chime_note_t SFX_PORTAL[] = {
    { 329.63f, 90 }, { 493.88f, 90 }, { 659.25f, 100 }, { 880.00f, 110 },
    { 1108.73f, 130 }, { 1318.51f, 160 }, { 1661.22f, 250 }
};

static const chime_note_t SFX_SUPERNOVA[] = {
    { 65.41f, 140 }, { 82.41f, 120 }, { 110.00f, 100 }, { 146.83f, 80 },
    { 98.00f, 120 }, { 73.42f, 160 }, { 55.00f, 220 }, { 41.20f, 350 }
};

// 8. R2-D2 Astromech: Happy Chirp & Joyful Trill
static const chime_note_t SFX_R2_HAPPY[] = {
    { 1046.50f, 35 }, { 1318.51f, 35 }, { 1567.98f, 40 }, { 2093.00f, 45 },
    { 2793.83f, 55 }, { 0.0f, 20 },
    { 3135.96f, 30 }, { 2793.83f, 30 }, { 3135.96f, 30 }, { 3520.00f, 85 }
};

// 9. R2-D2 Astromech: Sassy Rapid Banter / Droid Chatter
static const chime_note_t SFX_R2_CHATTER[] = {
    { 2200.00f, 30 }, { 1640.00f, 35 }, { 2480.00f, 25 }, { 1860.00f, 40 },
    { 2920.00f, 30 }, { 2100.00f, 45 }, { 1480.00f, 35 }, { 2600.00f, 50 },
    { 3300.00f, 75 }
};

// 10. R2-D2 Astromech: Frantic Panic Scream / Warning Alarm
static const chime_note_t SFX_R2_PANIC[] = {
    { 2793.83f, 20 }, { 3520.00f, 20 }, { 3135.96f, 20 }, { 3951.07f, 20 },
    { 2793.83f, 20 }, { 3520.00f, 20 }, { 3135.96f, 20 }, { 3951.07f, 20 },
    { 2637.02f, 35 }, { 2093.00f, 45 }, { 1567.98f, 65 }
};

// R2-D2 Astromech: Melodic Time Chime
static const chime_note_t SFX_R2_TIME[] = {
    { 1318.51f, 50 }, { 0.0f, 25 }, { 1760.00f, 50 }, { 0.0f, 25 },
    { 2637.02f, 80 }, { 3135.96f, 130 }
};

// 11. eBay Cash Register: "Cha-Ching!" Mechanical Bell Ring
static const chime_note_t SFX_CHA_CHING[] = {
    { 1864.66f, 80 }, { 2349.32f, 220 }, { 0.0f, 40 }, { 2093.00f, 320 }
};

// 12. Pokémon Center: Healing Jingle
static const chime_note_t SFX_POKECENTER[] = {
    { 493.88f, 140 }, { 659.25f, 140 }, { 830.61f, 140 }, { 987.77f, 220 },
    { 830.61f, 140 }, { 987.77f, 380 }
};

// 13. Harry Potter: Lumos Celestial Flare
static const chime_note_t SFX_LUMOS[] = {
    { 1046.50f, 40 }, { 1318.51f, 40 }, { 1567.98f, 50 }, { 2093.00f, 60 }, { 3135.96f, 140 }
};

// 14. Harry Potter: Expecto Patronum Rising Wave
static const chime_note_t SFX_PATRONUS[] = {
    { 220.00f, 60 }, { 329.63f, 60 }, { 440.00f, 70 }, { 659.25f, 80 },
    { 880.00f, 100 }, { 1318.51f, 180 }
};

typedef enum {
    TRACK_TYPE_ADPCM = 0,
    TRACK_TYPE_NOTES
} chiptune_track_type_t;

typedef struct {
    const char *name;
    chiptune_track_type_t type;
    int adpcm_id;
    const chime_note_t *notes;
    size_t count;
} chiptune_track_t;

static const chiptune_track_t CHIPTUNE_TRACKS[] = {
    { "SUPER MARIO BROS", TRACK_TYPE_NOTES, -1, MARIO_THEME, sizeof(MARIO_THEME) / sizeof(MARIO_THEME[0]) }, // 0
    { "TETRIS KOROBEINIKI", TRACK_TYPE_NOTES, -1, TETRIS_THEME, sizeof(TETRIS_THEME) / sizeof(TETRIS_THEME[0]) }, // 1
    { "DOOM E1M1 RIFF", TRACK_TYPE_NOTES, -1, DOOM_THEME, sizeof(DOOM_THEME) / sizeof(DOOM_THEME[0]) }, // 2
    { "ZELDA OVERWORLD", TRACK_TYPE_NOTES, -1, ZELDA_THEME, sizeof(ZELDA_THEME) / sizeof(ZELDA_THEME[0]) }, // 3
    { "IMPERIAL MARCH", TRACK_TYPE_NOTES, -1, IMPERIAL_THEME, sizeof(IMPERIAL_THEME) / sizeof(IMPERIAL_THEME[0]) }, // 4
    { "GHOSTBUSTERS 8-BIT", TRACK_TYPE_NOTES, -1, GHOSTBUSTERS_THEME, sizeof(GHOSTBUSTERS_THEME) / sizeof(GHOSTBUSTERS_THEME[0]) }, // 5
    { "MORROWIND NEREVAR", TRACK_TYPE_NOTES, -1, MORROWIND_THEME, sizeof(MORROWIND_THEME) / sizeof(MORROWIND_THEME[0]) }, // 6
    { "SPOOKY SKELETONS", TRACK_TYPE_NOTES, -1, SPOOKY_SKELETONS_THEME, sizeof(SPOOKY_SKELETONS_THEME) / sizeof(SPOOKY_SKELETONS_THEME[0]) }, // 7
    { "DAFT PUNK DA FUNK", TRACK_TYPE_NOTES, -1, DAFT_PUNK_THEME, sizeof(DAFT_PUNK_THEME) / sizeof(DAFT_PUNK_THEME[0]) }, // 8
    { "EBAY CHAOS AUCTION", TRACK_TYPE_NOTES, -1, EBAY_THEME, sizeof(EBAY_THEME) / sizeof(EBAY_THEME[0]) }, // 9
    { "HARRY POTTER HEDWIG", TRACK_TYPE_NOTES, -1, HARRY_POTTER_THEME, sizeof(HARRY_POTTER_THEME) / sizeof(HARRY_POTTER_THEME[0]) }, // 10
    { "POKEMON CATCH 'EM", TRACK_TYPE_NOTES, -1, POKEMON_THEME, sizeof(POKEMON_THEME) / sizeof(POKEMON_THEME[0]) }, // 11
    { "SAILOR MOON DENSETSU", TRACK_TYPE_NOTES, -1, SAILOR_MOON_THEME, sizeof(SAILOR_MOON_THEME) / sizeof(SAILOR_MOON_THEME[0]) }, // 12
    { "MEGA MAN 2 WILY", TRACK_TYPE_NOTES, -1, MEGAMAN_THEME, sizeof(MEGAMAN_THEME) / sizeof(MEGAMAN_THEME[0]) }, // 13
    { "BELLA CIAO", TRACK_TYPE_NOTES, -1, BELLA_CIAO, sizeof(BELLA_CIAO) / sizeof(BELLA_CIAO[0]) }, // 14
    { "ODE TO JOY (BEETHOVEN)", TRACK_TYPE_NOTES, -1, ODE_TO_JOY, sizeof(ODE_TO_JOY) / sizeof(ODE_TO_JOY[0]) }, // 15
    { "DU LIEGST MIR IM HERZEN", TRACK_TYPE_NOTES, -1, GERMAN_FOLK, sizeof(GERMAN_FOLK) / sizeof(GERMAN_FOLK[0]) }, // 16
    { "MO LI HUA (JASMINE)", TRACK_TYPE_NOTES, -1, MO_LI_HUA, sizeof(MO_LI_HUA) / sizeof(MO_LI_HUA[0]) }, // 17
    { "GREENSLEEVES", TRACK_TYPE_NOTES, -1, GREENSLEEVES, sizeof(GREENSLEEVES) / sizeof(GREENSLEEVES[0]) }, // 18
    { "SAKURA SAKURA", TRACK_TYPE_NOTES, -1, SAKURA_SAKURA, sizeof(SAKURA_SAKURA) / sizeof(SAKURA_SAKURA[0]) }, // 19
    { "LA CUCARACHA", TRACK_TYPE_NOTES, -1, LA_CUCARACHA, sizeof(LA_CUCARACHA) / sizeof(LA_CUCARACHA[0]) }, // 20
    { "GHOSTBUSTERS (MASTER)", TRACK_TYPE_ADPCM, MUSIC_TRACK_GHOSTBUSTERS, NULL, 0 }, // 21
    { "ADDAMS FAMILY (MASTER)", TRACK_TYPE_ADPCM, MUSIC_TRACK_ADDAMS_FAMILY, NULL, 0 }, // 22
    { "DAFT PUNK (ROBOT ROCK)", TRACK_TYPE_ADPCM, MUSIC_TRACK_DAFT_PUNK, NULL, 0 }  // 23
};

#define CHIPTUNE_TRACK_COUNT ((int)(sizeof(CHIPTUNE_TRACKS) / sizeof(CHIPTUNE_TRACKS[0])))

static const chiptune_track_t COSMIC_SFX_TRACKS[] = {
    { "WARP DRIVE", TRACK_TYPE_NOTES, -1, SFX_WARP, sizeof(SFX_WARP) / sizeof(SFX_WARP[0]) },
    { "PULSAR BEACON", TRACK_TYPE_NOTES, -1, SFX_PULSAR, sizeof(SFX_PULSAR) / sizeof(SFX_PULSAR[0]) },
    { "ALIEN SCANNER", TRACK_TYPE_NOTES, -1, SFX_ALIEN, sizeof(SFX_ALIEN) / sizeof(SFX_ALIEN[0]) },
    { "LASER BLASTER", TRACK_TYPE_NOTES, -1, SFX_LASER, sizeof(SFX_LASER) / sizeof(SFX_LASER[0]) },
    { "TRACTOR BEAM", TRACK_TYPE_NOTES, -1, SFX_TRACTOR, sizeof(SFX_TRACTOR) / sizeof(SFX_TRACTOR[0]) },
    { "STARGATE PORTAL", TRACK_TYPE_NOTES, -1, SFX_PORTAL, sizeof(SFX_PORTAL) / sizeof(SFX_PORTAL[0]) },
    { "SUPERNOVA", TRACK_TYPE_NOTES, -1, SFX_SUPERNOVA, sizeof(SFX_SUPERNOVA) / sizeof(SFX_SUPERNOVA[0]) },
    { "R2 HAPPY CHIRP", TRACK_TYPE_NOTES, -1, SFX_R2_HAPPY, sizeof(SFX_R2_HAPPY) / sizeof(SFX_R2_HAPPY[0]) },
    { "R2 DROID CHATTER", TRACK_TYPE_NOTES, -1, SFX_R2_CHATTER, sizeof(SFX_R2_CHATTER) / sizeof(SFX_R2_CHATTER[0]) },
    { "R2 PANIC SCREAM", TRACK_TYPE_NOTES, -1, SFX_R2_PANIC, sizeof(SFX_R2_PANIC) / sizeof(SFX_R2_PANIC[0]) },
    { "CHA-CHING AUCTION", TRACK_TYPE_NOTES, -1, SFX_CHA_CHING, sizeof(SFX_CHA_CHING) / sizeof(SFX_CHA_CHING[0]) },
    { "POKECENTER HEAL", TRACK_TYPE_NOTES, -1, SFX_POKECENTER, sizeof(SFX_POKECENTER) / sizeof(SFX_POKECENTER[0]) },
    { "LUMOS FLARE", TRACK_TYPE_NOTES, -1, SFX_LUMOS, sizeof(SFX_LUMOS) / sizeof(SFX_LUMOS[0]) },
    { "PATRONUS MIST", TRACK_TYPE_NOTES, -1, SFX_PATRONUS, sizeof(SFX_PATRONUS) / sizeof(SFX_PATRONUS[0]) }
};

#define COSMIC_SFX_TOTAL ((int)(sizeof(COSMIC_SFX_TRACKS) / sizeof(COSMIC_SFX_TRACKS[0])))

static int active_chiptune_track = -1;
static size_t chiptune_step = 0;
static absolute_time_t next_chiptune_step;
static float live_chiptune_freq = 0.0f;

static int active_cosmic_sfx = -1;
static size_t cosmic_sfx_step = 0;
static absolute_time_t next_cosmic_sfx_step;

// All sources receive the same final gain. Frequency does not change volume.
static float calculate_amplitude(float freq_hz) {
    return freq_hz > 0.0f ? AUDIO_PCM16_PEAK : 0.0f;
}

// Use the same curve at boot and when any control changes volume.
static float calculate_volume_gain(uint8_t volume_percent) {
    if (volume_percent == 0) return 0.0f;
    if (volume_percent > 100) volume_percent = 100;
    float db = -AUDIO_VOLUME_RANGE_DB * (float)(100 - volume_percent) / 99.0f;
    return powf(10.0f, db / 20.0f);
}

// count is FIFO words: one 32-bit word per stereo frame (16-bit L in MSW, 16-bit R in LSW).
static void fill_audio_buffer(uint32_t *buf, size_t count) {
    float phase_step = (current_freq > 0.0f) ? (current_freq * 2.0f * 3.14159265f / (float)AUDIO_SAMPLE_RATE) : 0.0f;
    const uint32_t RAMP_SAMPLES = 192; // 12 ms smooth raised-cosine fade-in/fade-out

    float gain = master_gain;
    float voice_gain = AUDIO_PCM16_PEAK / 128.0f;
    const float pcm16_gain = 5.0f; // Pre-amp gain boost (+14 dB) for natural microphone voice replay
    const float stream_gain = 0.85f; // Calibrated 1:1 dynamic range for clear speech without digital clipping
    uint32_t buffer_peak = 0;

    for (size_t i = 0; i < count; i++) {
        float mixed_sample = 0.0f;


        // 1. Synthesize musical tone with smooth harmonic shaping
        if (samples_played < total_note_samples && current_freq > 0.0f) {
            float env = 1.0f;

            // Raised-cosine attack ramp (first 12 ms)
            if (samples_played < RAMP_SAMPLES) {
                float x = (float)samples_played / (float)RAMP_SAMPLES;
                env = 0.5f * (1.0f - cosf(3.14159265f * x));
            }

            // Raised-cosine decay ramp (last 12 ms)
            uint32_t remaining = total_note_samples - samples_played;
            if (remaining < RAMP_SAMPLES) {
                float x = (float)remaining / (float)RAMP_SAMPLES;
                float release_env = 0.5f * (1.0f - cosf(3.14159265f * x));
                if (release_env < env) env = release_env;
            }

            float s1 = sinf(current_phase);
            float warm_wave;
            if (diagnostic_sine) {
                warm_wave = (diagnostic_polarity != 0) ?
                    ((0.5f + 0.5f * s1) * (float)diagnostic_polarity) : s1;
            } else if (active_chiptune_track >= 0 || active_cosmic_sfx >= 0) {
                // 16-bit / 8-bit retro arcade synthesizer:
                // Odd harmonics (pulse/square wave) + punchy sub-octave bass harmonic
                float s3 = sinf(current_phase * 3.0f);
                float s5 = sinf(current_phase * 5.0f);
                float sub = sinf(current_phase * 0.5f);
                warm_wave = 0.52f * s1 + 0.22f * s3 + 0.10f * s5 + 0.16f * sub;
            } else {
                warm_wave = 0.85f * s1 + 0.15f * sinf(current_phase * 2.0f);
            }

            float val = warm_wave * (current_amplitude * env);
            mixed_sample += val;

            current_phase += phase_step;
            if (current_phase >= 2.0f * 3.14159265f) {
                current_phase -= 2.0f * 3.14159265f;
            }
            samples_played++;
        } else {
            current_freq = 0.0f;
        }

        // 2. Mix digitized speech voice clip if active
        if (voice_active && active_voice_clip != NULL) {
            if (voice_sample_idx < active_voice_clip->sample_count) {
                int8_t v_byte = active_voice_clip->samples[voice_sample_idx++];
                mixed_sample += (float)v_byte * voice_gain;
            } else {
                voice_active = false;
                active_voice_clip = NULL;
            }
        }

        // 2b. Mix ADPCM music clip if active
        if (adpcm_active && active_adpcm_clip != NULL) {
            if (adpcm_sample_idx < active_adpcm_clip->sample_count) {
                size_t byte_idx = adpcm_sample_idx >> 1;
                uint8_t byte_val = active_adpcm_clip->adpcm_data[byte_idx];
                uint8_t nibble = (adpcm_sample_idx & 1) ? ((byte_val >> 4) & 0x0F) : (byte_val & 0x0F);

                int step = adpcm_step_table[adpcm_step_idx];
                int diff = step >> 3;
                if (nibble & 1) diff += step >> 2;
                if (nibble & 2) diff += step >> 1;
                if (nibble & 4) diff += step;
                if (nibble & 8) adpcm_predicted -= diff;
                else adpcm_predicted += diff;
                if (adpcm_predicted > 32767) adpcm_predicted = 32767;
                else if (adpcm_predicted < -32768) adpcm_predicted = -32768;

                adpcm_step_idx += adpcm_index_table[nibble];
                if (adpcm_step_idx < 0) adpcm_step_idx = 0;
                else if (adpcm_step_idx > 88) adpcm_step_idx = 88;

                adpcm_sample_idx++;
                mixed_sample += (float)adpcm_predicted;
            } else {
                adpcm_active = false;
            }
        }

        // 3. Mix 16-bit PCM playback if active
        if (pcm16_active && active_pcm16_samples != NULL) {
            if (pcm16_sample_idx < pcm16_sample_count) {
                int16_t s = active_pcm16_samples[pcm16_sample_idx++];
                mixed_sample += (float)s * pcm16_gain;
            } else {
                pcm16_active = false;
                active_pcm16_samples = NULL;
            }
        }

        // 4. Mix streaming 16-bit PCM ring buffer if active
        if (audio_stream_active) {
            if (audio_stream_read_idx != audio_stream_write_idx) {
                int16_t s = audio_stream_ring[audio_stream_read_idx++ & AUDIO_STREAM_RING_MASK];
                mixed_sample += (float)s * stream_gain;
            } else if (audio_stream_eof) {
                audio_stream_active = false;
            } else {
                // Buffer momentarily dry before EOF: keep stream active and output silence.
                // Never stop DAC or abort DMA mid-stream; next UDP packet resumes instantly.
            }
        }

        // Bound even simultaneous tone + voice to the configured digital ceiling.
        if (mixed_sample > AUDIO_PCM16_PEAK) mixed_sample = AUDIO_PCM16_PEAK;
        if (mixed_sample < -AUDIO_PCM16_PEAK) mixed_sample = -AUDIO_PCM16_PEAK;
        int16_t s16 = (int16_t)lroundf(mixed_sample * gain);
        uint32_t magnitude = (uint32_t)(s16 < 0 ? -s16 : s16);
        if (magnitude > buffer_peak) buffer_peak = magnitude;
        // Official 16-bit stereo frame format: Left in bits 31..16, Right in bits 15..0
        buf[i] = ((uint32_t)(uint16_t)s16 << 16) | (uint32_t)(uint16_t)s16;
    }
    if (buffer_peak > playback_peak_pcm24) playback_peak_pcm24 = buffer_peak;
}

static void refill_completed_buffer(int channel, int peer, uint32_t *buf) {
    // Only abort if channel is still transmitting (overrun) or if peer has fewer
    // than 8 words (<0.5 ms) remaining. fill_audio_buffer takes ~0.2 ms on RP2350.
    if (dma_channel_is_busy(channel) ||
        (dma_channel_is_busy(peer) &&
         dma_channel_hw_addr(peer)->transfer_count < 8)) {
        late_refills++;
        return;
    }
    fill_audio_buffer(buf, AUDIO_BUFFER_WORDS);
    if (dma_channel_is_busy(channel)) late_refills++;
}

// Hardware DMA IRQ handler for continuous double buffering.
static void audio_dma_irq_handler(void) {
    if (amp_is_sleeping) {
        dma_channel_acknowledge_irq0(dma_chan_a);
        dma_channel_acknowledge_irq0(dma_chan_b);
        return;
    }
    absolute_time_t refill_start = get_absolute_time();
    uint32_t stall_bit = 1u << (PIO_FDEBUG_TXSTALL_LSB + audio_sm);
    if (dma_debug_started && (audio_pio->fdebug & stall_bit)) tx_stall_observations++;
    audio_pio->fdebug = stall_bit; // W1C; ignore the intentional initial empty FIFO.
    dma_debug_started = true;
    if (dma_channel_get_irq0_status(dma_chan_a)) {
        dma_channel_acknowledge_irq0(dma_chan_a);
        refill_completed_buffer(dma_chan_a, dma_chan_b, buf_a);
    }
    if (dma_channel_get_irq0_status(dma_chan_b)) {
        dma_channel_acknowledge_irq0(dma_chan_b);
        refill_completed_buffer(dma_chan_b, dma_chan_a, buf_b);
    }
    // RP2350 reloads TRANS_COUNT on each chain trigger, while read addresses
    // wrap in hardware. A late IRQ can replay stale PCM but cannot read beyond
    // either buffer. Recover an unexpectedly idle pair defensively.
    if (!dma_channel_is_busy(dma_chan_a) && !dma_channel_is_busy(dma_chan_b)) {
        dma_channel_start(dma_chan_a);
        dma_restarts++;
    }
    uint32_t elapsed = (uint32_t)absolute_time_diff_us(refill_start, get_absolute_time());
    if (elapsed > max_refill_us) max_refill_us = elapsed;
}

void audio_init(void) {
    master_volume_percent = AUDIO_DEFAULT_VOLUME;
    master_gain = calculate_volume_gain(AUDIO_DEFAULT_VOLUME);
    amp_is_sleeping = false;
    audio_offset = pio_add_program(audio_pio, &audio_i2s_program);
    audio_i2s_program_init(audio_pio, audio_sm, audio_offset, PIN_SPK_DIN, PIN_SPK_BCLK);

    float pio_clock = (float)(AUDIO_SAMPLE_RATE * 32 * 2);
    float div = (float)clock_get_hz(clk_sys) / pio_clock;
    pio_sm_set_clkdiv(audio_pio, audio_sm, div);

    // Initial silent buffers
    for (size_t i = 0; i < AUDIO_BUFFER_WORDS; i++) {
        buf_a[i] = 0;
        buf_b[i] = 0;
    }

    // Configure Ping-Pong DMA
    dma_chan_a = dma_claim_unused_channel(true);
    dma_chan_b = dma_claim_unused_channel(true);

    dma_channel_config c_a = dma_channel_get_default_config(dma_chan_a);
    channel_config_set_transfer_data_size(&c_a, DMA_SIZE_32);
    channel_config_set_read_increment(&c_a, true);
    channel_config_set_ring(&c_a, false, AUDIO_DMA_RING_BITS);
    channel_config_set_write_increment(&c_a, false);
    channel_config_set_dreq(&c_a, pio_get_dreq(audio_pio, audio_sm, true));
    channel_config_set_chain_to(&c_a, dma_chan_b); // Chain to B

    dma_channel_configure(
        dma_chan_a,
        &c_a,
        &audio_pio->txf[audio_sm],
        buf_a,
        AUDIO_BUFFER_WORDS,
        false
    );

    dma_channel_config c_b = dma_channel_get_default_config(dma_chan_b);
    channel_config_set_transfer_data_size(&c_b, DMA_SIZE_32);
    channel_config_set_read_increment(&c_b, true);
    channel_config_set_ring(&c_b, false, AUDIO_DMA_RING_BITS);
    channel_config_set_write_increment(&c_b, false);
    channel_config_set_dreq(&c_b, pio_get_dreq(audio_pio, audio_sm, true));
    channel_config_set_chain_to(&c_b, dma_chan_a); // Chain to A

    dma_channel_configure(
        dma_chan_b,
        &c_b,
        &audio_pio->txf[audio_sm],
        buf_b,
        AUDIO_BUFFER_WORDS,
        false
    );

    // Setup DMA IRQ0 for seamless hardware background double-buffering
    dma_channel_set_irq0_enabled(dma_chan_a, true);
    dma_channel_set_irq0_enabled(dma_chan_b, true);
    irq_set_exclusive_handler(DMA_IRQ_0, audio_dma_irq_handler);
    irq_set_priority(DMA_IRQ_0, 0x40); // High priority so other IRQs do not delay audio refill
    irq_set_enabled(DMA_IRQ_0, true);

    // Stay stopped until the user starts playback. No stale buffer on boot.
    amp_sleep();
}

void audio_set_volume(uint8_t volume_percent) {
    if (volume_percent > 100) volume_percent = 100;
    float gain = calculate_volume_gain(volume_percent);
    uint32_t save = save_and_disable_interrupts();
    master_volume_percent = volume_percent;
    master_gain = gain;
    if (volume_percent == 0 && dma_chan_a >= 0 && dma_chan_b >= 0) {
        audio_stream_read_idx = audio_stream_write_idx;
        audio_stream_active = false;
        amp_sleep();
    }
    else if (volume_percent > 0 && audio_is_playing()) amp_wake();
    restore_interrupts(save);
}

uint8_t audio_get_volume(void) {
    return master_volume_percent;
}

static void amp_wake(void) {
    silence_since = get_absolute_time();
    if (amp_is_sleeping && master_volume_percent > 0) {
        // Fully reset PIO state machine to start at instruction 0 on a clean frame boundary
        pio_sm_set_enabled(audio_pio, audio_sm, false);
        pio_sm_restart(audio_pio, audio_sm);
        pio_sm_clkdiv_restart(audio_pio, audio_sm);
        pio_sm_clear_fifos(audio_pio, audio_sm);
        pio_sm_exec(audio_pio, audio_sm, pio_encode_jmp(audio_offset + audio_i2s_offset_entry_point));

        // amp_sleep() already stopped both channels and cleared old PCM.
        dma_channel_set_read_addr(dma_chan_a, buf_a, false);
        dma_channel_set_trans_count(dma_chan_a, AUDIO_BUFFER_WORDS, false);
        dma_channel_set_read_addr(dma_chan_b, buf_b, false);
        dma_channel_set_trans_count(dma_chan_b, AUDIO_BUFFER_WORDS, false);

        dma_channel_acknowledge_irq0(dma_chan_a);
        dma_channel_acknowledge_irq0(dma_chan_b);
        // AL1_CTRL changes EN without triggering both channels simultaneously.
        hw_set_bits(&dma_channel_hw_addr(dma_chan_a)->al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
        hw_set_bits(&dma_channel_hw_addr(dma_chan_b)->al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
        dma_channel_set_irq0_enabled(dma_chan_a, true);
        dma_channel_set_irq0_enabled(dma_chan_b, true);
        amp_is_sleeping = false;
        dma_debug_started = false;
        dma_channel_start(dma_chan_a); // Prime FIFO before enabling BCLK.
        pio_sm_set_enabled(audio_pio, audio_sm, true);
    }
}

static void amp_sleep(void) {
    uint32_t save = save_and_disable_interrupts();
    if (!amp_is_sleeping) {
        pio_sm_set_enabled(audio_pio, audio_sm, false);
        dma_channel_set_irq0_enabled(dma_chan_a, false);
        dma_channel_set_irq0_enabled(dma_chan_b, false);
        // RP2350-E5: clear EN on BOTH chained channels before aborting either.
        hw_clear_bits(&dma_channel_hw_addr(dma_chan_a)->ctrl_trig, DMA_CH0_CTRL_TRIG_EN_BITS);
        hw_clear_bits(&dma_channel_hw_addr(dma_chan_b)->ctrl_trig, DMA_CH0_CTRL_TRIG_EN_BITS);
        dma_channel_abort(dma_chan_a);
        dma_channel_abort(dma_chan_b);
        dma_channel_acknowledge_irq0(dma_chan_a);
        dma_channel_acknowledge_irq0(dma_chan_b);
        pio_sm_clear_fifos(audio_pio, audio_sm);
        uint32_t pins = (1u << PIN_SPK_BCLK) | (1u << PIN_SPK_LRC) | (1u << PIN_SPK_DIN);
        pio_sm_set_pins_with_mask(audio_pio, audio_sm, 0, pins);
        amp_is_sleeping = true;
    }
    for (size_t i = 0; i < AUDIO_BUFFER_WORDS; ++i) buf_a[i] = buf_b[i] = 0;
    restore_interrupts(save);
}

void audio_play_tone(float freq_hz, uint32_t duration_ms) {
    uint32_t save = save_and_disable_interrupts();
    diagnostic_sine = false;
    playback_peak_pcm24 = 0;
    current_freq = freq_hz;
    current_amplitude = calculate_amplitude(freq_hz);
    total_note_samples = (AUDIO_SAMPLE_RATE * duration_ms) / 1000;
    samples_played = 0;
    amp_wake();
    restore_interrupts(save);
}

void audio_play_blip(void) {
    audio_play_tone(440.0f, 12); // Soft gentle 440 Hz click
}

void audio_play_chime(void) {
    chime_index = 0;
    audio_play_tone(CHIME_SEQUENCE[0].freq, CHIME_SEQUENCE[0].dur_ms);
    next_note_time = delayed_by_ms(get_absolute_time(), CHIME_SEQUENCE[0].dur_ms + 15);
}

// -------------------------------------------------------------------------
// Digitized Voice Playback Subsystem
// -------------------------------------------------------------------------
void audio_play_voice(voice_phrase_id_t phrase_id) {
    const voice_clip_t *clip = voice_get_clip(phrase_id);
    if (clip != NULL && clip->samples != NULL && clip->sample_count > 0) {
        uint32_t save = save_and_disable_interrupts();
        playback_peak_pcm24 = 0;
        active_voice_clip = clip;
        voice_sample_idx = 0;
        voice_active = true;
        amp_wake();
        restore_interrupts(save);
    }
}

void audio_stop_voice(void) {
    uint32_t save = save_and_disable_interrupts();
    voice_active = false;
    active_voice_clip = NULL;
    voice_sample_idx = 0;
    restore_interrupts(save);
}

bool audio_is_voice_playing(void) {
    return voice_active;
}

void audio_play_pcm16(const int16_t *samples, size_t count) {
    if (samples != NULL && count > 0) {
        uint32_t save = save_and_disable_interrupts();
        playback_peak_pcm24 = 0;
        active_pcm16_samples = samples;
        pcm16_sample_count = count;
        pcm16_sample_idx = 0;
        pcm16_active = true;
        amp_wake();
        restore_interrupts(save);
    }
}

void audio_stop_pcm16(void) {
    uint32_t save = save_and_disable_interrupts();
    pcm16_active = false;
    active_pcm16_samples = NULL;
    pcm16_sample_count = 0;
    pcm16_sample_idx = 0;
    restore_interrupts(save);
}

bool audio_is_pcm16_playing(void) {
    return pcm16_active;
}

void audio_stream_reset(void) {
    uint32_t save = save_and_disable_interrupts();
    playback_peak_pcm24 = 0;
    audio_stream_write_idx = 0;
    audio_stream_read_idx = 0;
    audio_stream_eof = false;
    audio_stream_active = false;
    last_stream_packet_time = get_absolute_time();
    restore_interrupts(save);
}

bool audio_stream_push(const int16_t *samples, size_t count) {
    if (samples == NULL || count == 0) return false;
    uint32_t save = save_and_disable_interrupts();
    last_stream_packet_time = get_absolute_time();
    if (master_volume_percent == 0 && !audio_stream_eof) {
        /* Muted packets still complete the transaction, without waking the DAC. */
        restore_interrupts(save);
        return true;
    }
    if (count > AUDIO_STREAM_RING_SIZE - (uint32_t)(audio_stream_write_idx - audio_stream_read_idx) || audio_stream_eof) {
        restore_interrupts(save);
        return false; /* Sender must retry; never overwrite queued speech. */
    }
    for (size_t i = 0; i < count; i++) {
        audio_stream_ring[audio_stream_write_idx++ & AUDIO_STREAM_RING_MASK] = samples[i];
    }
    // Pre-buffer 0.10s (1600 samples) or EOF before kicking off DAC
    if (!audio_stream_active && ((audio_stream_write_idx - audio_stream_read_idx >= 1600) || audio_stream_eof)) {
        audio_stream_active = true;
        voice_active = false;
        active_voice_clip = NULL;
        amp_wake();
    }
    restore_interrupts(save);
    return true;
}

void audio_stream_finish(void) {
    uint32_t save = save_and_disable_interrupts();
    audio_stream_eof = true;
    if (audio_stream_write_idx != audio_stream_read_idx) {
        audio_stream_active = true;
        voice_active = false;
        active_voice_clip = NULL;
        amp_wake();
    }
    restore_interrupts(save);
}

bool audio_is_stream_playing(void) {
    return audio_stream_active;
}

const char* audio_get_voice_name(voice_phrase_id_t phrase_id) {
    const voice_clip_t *clip = voice_get_clip(phrase_id);
    return (clip != NULL) ? clip->name : "UNKNOWN";
}

int audio_get_voice_count(void) {
    return VOICE_PHRASE_COUNT;
}

// -------------------------------------------------------------------------
// Audio Update Sequencer (Main Loop)
// -------------------------------------------------------------------------
void audio_update(void) {
    // 1. Process 8-bit chiptune jukebox sequencer
    if (active_chiptune_track >= 0) {
        if (absolute_time_diff_us(next_chiptune_step, get_absolute_time()) >= 0) {
            const chiptune_track_t *trk = &CHIPTUNE_TRACKS[active_chiptune_track];
            if (chiptune_step < trk->count) {
                float f = trk->notes[chiptune_step].freq;
                uint32_t dur = trk->notes[chiptune_step].dur_ms;
                live_chiptune_freq = f;

                if (f > 0.0f) {
                    // Staccato 8-bit note: 82% sound, 18% gap
                    audio_play_tone(f, (dur * 82) / 100);
                } else {
                    current_freq = 0.0f;
                }

                chiptune_step++;
                next_chiptune_step = delayed_by_ms(get_absolute_time(), dur);
            } else {
                // Note sequence finished
                active_chiptune_track = -1;
                chiptune_step = 0;
                live_chiptune_freq = 0.0f;
                current_freq = 0.0f;
            }
        }
    }

    // 1b. Check if active music finished and handle playlist progression
    if (current_music_track_idx >= 0) {
        bool finished = false;
        const chiptune_track_t *trk = &CHIPTUNE_TRACKS[current_music_track_idx];
        if (trk->type == TRACK_TYPE_ADPCM) {
            if (!adpcm_active) finished = true;
        } else {
            if (active_chiptune_track < 0) finished = true;
        }

        if (finished) {
            if (music_play_mode == AUDIO_PLAY_ALL) {
                if (current_music_track_idx + 1 < CHIPTUNE_TRACK_COUNT) {
                    audio_play_chiptune(current_music_track_idx + 1);
                } else {
                    current_music_track_idx = -1;
                }
            } else if (music_play_mode == AUDIO_PLAY_CYCLE) {
                int next = (current_music_track_idx + 1) % CHIPTUNE_TRACK_COUNT;
                audio_play_chiptune(next);
            } else {
                current_music_track_idx = -1;
            }
        }
    }

    // 1c. Process Cosmic Sci-Fi SFX sequencer
    if (active_cosmic_sfx >= 0) {
        if (absolute_time_diff_us(next_cosmic_sfx_step, get_absolute_time()) >= 0) {
            const chiptune_track_t *trk = &COSMIC_SFX_TRACKS[active_cosmic_sfx];
            if (cosmic_sfx_step < trk->count) {
                float f = trk->notes[cosmic_sfx_step].freq;
                uint32_t dur = trk->notes[cosmic_sfx_step].dur_ms;
                live_chiptune_freq = f;

                if (f > 0.0f) {
                    audio_play_tone(f, dur);
                } else {
                    current_freq = 0.0f;
                }

                cosmic_sfx_step++;
                next_cosmic_sfx_step = delayed_by_ms(get_absolute_time(), dur);
            } else {
                audio_stop_cosmic_sfx();
            }
        }
    }

    // 2. Process musical chime sequencer
    if (chime_index >= 0) {
        if (absolute_time_diff_us(next_note_time, get_absolute_time()) >= 0) {
            chime_index++;
            if (chime_index < (int)(sizeof(CHIME_SEQUENCE) / sizeof(CHIME_SEQUENCE[0]))) {
                audio_play_tone(CHIME_SEQUENCE[chime_index].freq, CHIME_SEQUENCE[chime_index].dur_ms);
                next_note_time = delayed_by_ms(get_absolute_time(), CHIME_SEQUENCE[chime_index].dur_ms + 15);
            } else {
                chime_index = -1; // Finished
            }
        }
    }

    // Stream inactivity watchdog: if stream is active but sender abandoned connection without EOF
    if (audio_stream_active && !audio_stream_eof && (audio_stream_read_idx == audio_stream_write_idx)) {
        if (absolute_time_diff_us(last_stream_packet_time, get_absolute_time()) > 1500000) {
            audio_stream_active = false;
            audio_stream_eof = true;
        }
    }

    // During fault isolation keep both I2S clocks running and send zero PCM.
    // Clock-gating/GAIN wiring must not be conflated with digital attenuation.
    if (AUDIO_CONTINUOUS_CLOCKS) {
        silence_since = get_absolute_time();
        return;
    }
    // Optional legacy clock-stop mode (not used by the diagnostic build).
    if (audio_is_playing()) {
        silence_since = get_absolute_time();
    } else {
        if (absolute_time_diff_us(silence_since, get_absolute_time()) >= 80000) {
            amp_sleep();
        }
    }
}

bool audio_is_playing(void) {
    return (current_freq > 0.0f && samples_played < total_note_samples) ||
           (chime_index >= 0) ||
           (active_chiptune_track >= 0) ||
           (active_cosmic_sfx >= 0) ||
           adpcm_active ||
           voice_active ||
           pcm16_active ||
           audio_stream_active;
}

int audio_get_chiptune_count(void) {
    return CHIPTUNE_TRACK_COUNT;
}

const char* audio_get_chiptune_name(int track_id) {
    if (track_id >= 0 && track_id < CHIPTUNE_TRACK_COUNT) {
        return CHIPTUNE_TRACKS[track_id].name;
    }
    return "UNKNOWN";
}

void audio_play_chiptune(int track_id) {
    if (track_id >= 0 && track_id < CHIPTUNE_TRACK_COUNT) {
        audio_stop_cosmic_sfx();
        chime_index = -1; // Cancel boot chime if active

        uint32_t save = save_and_disable_interrupts();
        current_music_track_idx = track_id;
        const chiptune_track_t *trk = &CHIPTUNE_TRACKS[track_id];
        if (trk->type == TRACK_TYPE_ADPCM) {
            active_chiptune_track = -1;
            chiptune_step = 0;
            live_chiptune_freq = 440.0f;
            current_freq = 0.0f;

            active_adpcm_clip = music_get_adpcm_clip((music_adpcm_id_t)trk->adpcm_id);
            adpcm_sample_idx = 0;
            adpcm_predicted = 0;
            adpcm_step_idx = 0;
            adpcm_active = true;
        } else {
            adpcm_active = false;
            active_adpcm_clip = NULL;

            active_chiptune_track = track_id;
            chiptune_step = 0;
            next_chiptune_step = get_absolute_time();
        }
        amp_wake();
        restore_interrupts(save);
    }
}

void audio_stop_chiptune(void) {
    uint32_t save = save_and_disable_interrupts();
    active_chiptune_track = -1;
    chiptune_step = 0;
    live_chiptune_freq = 0.0f;
    current_freq = 0.0f;
    samples_played = total_note_samples;
    adpcm_active = false;
    active_adpcm_clip = NULL;
    current_music_track_idx = -1;
    restore_interrupts(save);
}

bool audio_is_chiptune_playing(void) {
    return (active_chiptune_track >= 0) || adpcm_active;
}

void audio_set_play_mode(audio_play_mode_t mode) {
    if (mode < AUDIO_PLAY_MODE_COUNT) {
        music_play_mode = mode;
    }
}

audio_play_mode_t audio_get_play_mode(void) {
    return music_play_mode;
}

const char* audio_get_play_mode_name(audio_play_mode_t mode) {
    switch (mode) {
        case AUDIO_PLAY_SINGLE: return "SINGLE";
        case AUDIO_PLAY_ALL:    return "PLAY ALL";
        case AUDIO_PLAY_CYCLE:  return "CYCLE";
        default:                return "SINGLE";
    }
}

int audio_get_active_track_idx(void) {
    return current_music_track_idx;
}

// -------------------------------------------------------------------------
// Cosmic Sci-Fi SFX Subsystem
// -------------------------------------------------------------------------
int audio_get_cosmic_sfx_count(void) {
    return COSMIC_SFX_TOTAL;
}

const char* audio_get_cosmic_sfx_name(int sfx_id) {
    if (sfx_id >= 0 && sfx_id < COSMIC_SFX_TOTAL) {
        return COSMIC_SFX_TRACKS[sfx_id].name;
    }
    return "UNKNOWN";
}

void audio_play_cosmic_sfx(int sfx_id) {
    if (sfx_id >= 0 && sfx_id < COSMIC_SFX_TOTAL) {
        audio_stop_chiptune();
        chime_index = -1;
        active_cosmic_sfx = sfx_id;
        cosmic_sfx_step = 0;
        next_cosmic_sfx_step = get_absolute_time();
    }
}

void audio_stop_cosmic_sfx(void) {
    uint32_t save = save_and_disable_interrupts();
    active_cosmic_sfx = -1;
    cosmic_sfx_step = 0;
    live_chiptune_freq = 0.0f;
    current_freq = 0.0f;
    samples_played = total_note_samples;
    restore_interrupts(save);
}

bool audio_is_cosmic_sfx_playing(void) {
    return (active_cosmic_sfx >= 0);
}

void audio_play_r2d2_happy(void) {
    audio_play_cosmic_sfx(COSMIC_SFX_R2D2_HAPPY);
}

void audio_play_r2d2_chatter(void) {
    audio_play_cosmic_sfx(COSMIC_SFX_R2D2_CHATTER);
}

void audio_play_r2d2_panic(void) {
    audio_play_cosmic_sfx(COSMIC_SFX_R2D2_PANIC);
}

void audio_play_r2d2_time_chime(void) {
    // Play custom R2-D2 melodic time chime sequence
    audio_stop_chiptune();
    chime_index = -1;
    // We can run SFX_R2_TIME through the cosmic SFX player or direct note player
    static const chiptune_track_t TIME_TRK = {
        "R2 TIME CHIME", TRACK_TYPE_NOTES, -1, SFX_R2_TIME, sizeof(SFX_R2_TIME) / sizeof(SFX_R2_TIME[0])
    };
    (void)TIME_TRK;
    audio_play_cosmic_sfx(COSMIC_SFX_R2D2_HAPPY); // Plays the cheerful droid chime
}

void audio_play_r2d2_random(void) {
    static uint32_t r2_seed = 42;
    r2_seed = r2_seed * 1103515245 + 12345;
    int choice = (r2_seed >> 16) % 3;
    if (choice == 0) {
        audio_play_r2d2_happy();
    } else if (choice == 1) {
        audio_play_r2d2_chatter();
    } else {
        audio_play_r2d2_panic();
    }
}

void audio_play_cha_ching(void) {
    audio_play_cosmic_sfx(COSMIC_SFX_CHA_CHING);
}

void audio_play_pokecenter(void) {
    audio_play_cosmic_sfx(COSMIC_SFX_POKECENTER);
}

void audio_play_lumos(void) {
    audio_play_cosmic_sfx(COSMIC_SFX_LUMOS);
}

void audio_play_patronus(void) {
    audio_play_cosmic_sfx(COSMIC_SFX_PATRONUS);
}

float audio_get_current_note_freq(void) {
    if (active_chiptune_track >= 0 || active_cosmic_sfx >= 0) {
        return live_chiptune_freq;
    }
    return current_freq;
}

void audio_stop_all(void) {
    uint32_t save = save_and_disable_interrupts();
    chime_index = -1;
    audio_stop_chiptune();
    audio_stop_cosmic_sfx();
    audio_stop_voice();
    pcm16_active = false;
    active_pcm16_samples = NULL;
    audio_stream_reset();
    diagnostic_sine = false;
    diagnostic_polarity = 0;
    playback_peak_pcm24 = 0;
    amp_sleep();
    restore_interrupts(save);
}

void audio_play_polarity_tone(int polarity) {
    audio_play_diagnostic_tone();
    uint32_t save = save_and_disable_interrupts();
    diagnostic_polarity = (polarity > 0) ? 1 : (polarity < 0 ? -1 : 0);
    restore_interrupts(save);
}

void audio_play_diagnostic_tone(void) {
    audio_stop_all();
    uint32_t save = save_and_disable_interrupts();
    diagnostic_sine = true;
    diagnostic_polarity = 0;
    playback_peak_pcm24 = 0;
    current_freq = 440.0f;
    current_amplitude = calculate_amplitude(current_freq);
    current_phase = 0.0f;
    total_note_samples = AUDIO_SAMPLE_RATE; // One second, with the normal soft envelope.
    samples_played = 0;
    amp_wake();
    restore_interrupts(save);
}

void audio_print_diagnostics(void) {
    uint32_t save = save_and_disable_interrupts();
    uint32_t refill_us = max_refill_us;
    uint32_t stalls = tx_stall_observations;
    uint32_t peak = playback_peak_pcm24;
    uint32_t restarts = dma_restarts;
    uint32_t late = late_refills;
    bool dma_a_busy = dma_channel_is_busy(dma_chan_a);
    bool dma_b_busy = dma_channel_is_busy(dma_chan_b);
    bool stall_pending = (audio_pio->fdebug & (1u << (PIO_FDEBUG_TXSTALL_LSB + audio_sm))) != 0;
    restore_interrupts(save);
    LOG_INFO("AUDIO", "FW:%s | limit16:%u | 16kHz / 16-bit stereo I2S | Vol:%u%% | peak:%lu | refill_max_us:%lu | tx_stalls:%lu",
             FIRMWARE_VERSION, (unsigned)AUDIO_PCM16_PEAK, (unsigned)audio_get_volume(), (unsigned long)peak,
             (unsigned long)refill_us, (unsigned long)stalls);
    LOG_INFO("AUDIO", "DMA A/B busy:%u/%u | restarts:%lu | late_refills:%lu | stall_pending:%u",
             (unsigned)dma_a_busy, (unsigned)dma_b_busy, (unsigned long)restarts,
             (unsigned long)late, (unsigned)stall_pending);
}
