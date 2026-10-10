#include "audio.h"
#include "config.h"
#include "voice_data.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include "audio_i2s.pio.h"
#include <math.h>

#define AUDIO_SAMPLE_RATE     16000
#define AUDIO_BUFFER_SAMPLES  512   // 512 samples = 32 ms per buffer

static PIO audio_pio = pio1;
static uint audio_sm = 0;
static uint audio_offset = 0;

static int dma_chan_a = -1;
static int dma_chan_b = -1;

static uint32_t buf_a[AUDIO_BUFFER_SAMPLES];
static uint32_t buf_b[AUDIO_BUFFER_SAMPLES];

// Master volume state:
// Default 12% is whisper-quiet, gentle, zero startling.
// Perceptual quadratic curve ensures fine-grained sensitivity at low volumes.
static uint8_t master_volume_percent = 12;

// Tone synthesizer state
static float current_amplitude = 0.0f;
static float current_freq = 0.0f;
static float current_phase = 0.0f;
static uint32_t total_note_samples = 0;
static uint32_t samples_played = 0;

// Voice speech player state
static const voice_clip_t *active_voice_clip = NULL;
static size_t voice_sample_idx = 0;
static bool voice_active = false;

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
    { 587.33f, 130 }, { 659.25f, 260 }, { 783.99f, 180 }, { 698.46f, 400 }
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

// 6. Mega Man 2 (Dr. Wily Stage 1 - Driving 8-bit Synth)
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

typedef struct {
    const char *name;
    const chime_note_t *notes;
    size_t count;
} chiptune_track_t;

static const chiptune_track_t CHIPTUNE_TRACKS[] = {
    { "SUPER MARIO BROS", MARIO_THEME, sizeof(MARIO_THEME) / sizeof(MARIO_THEME[0]) },
    { "TETRIS KOROBEINIKI", TETRIS_THEME, sizeof(TETRIS_THEME) / sizeof(TETRIS_THEME[0]) },
    { "DOOM E1M1 RIFF", DOOM_THEME, sizeof(DOOM_THEME) / sizeof(DOOM_THEME[0]) },
    { "ZELDA OVERWORLD", ZELDA_THEME, sizeof(ZELDA_THEME) / sizeof(ZELDA_THEME[0]) },
    { "IMPERIAL MARCH", IMPERIAL_THEME, sizeof(IMPERIAL_THEME) / sizeof(IMPERIAL_THEME[0]) },
    { "MEGA MAN 2 WILY", MEGAMAN_THEME, sizeof(MEGAMAN_THEME) / sizeof(MEGAMAN_THEME[0]) }
};

#define CHIPTUNE_TRACK_COUNT ((int)(sizeof(CHIPTUNE_TRACKS) / sizeof(CHIPTUNE_TRACKS[0])))

static int active_chiptune_track = -1;
static size_t chiptune_step = 0;
static absolute_time_t next_chiptune_step;
static float live_chiptune_freq = 0.0f;

// Calculate equalized perceptual amplitude.
// Attenuates high frequencies (>250 Hz) using Fletcher-Munson curve
// so notes at 800 Hz are soft and musical rather than shrill.
static float calculate_amplitude(float freq_hz) {
    if (master_volume_percent == 0 || freq_hz <= 0.0f) {
        return 0.0f;
    }

    // Perceptual quadratic curve (12% -> 0.0144, 25% -> 0.0625, 50% -> 0.25, 100% -> 1.0)
    float vol_norm = (float)master_volume_percent / 100.0f;
    float vol_curve = vol_norm * vol_norm;

    // Base amplitude scaled so 100% max volume is clear but safe from distortion
    float base_amp = vol_curve * 1200.0f;

    // Equalization curve: higher notes get rolled off to match speaker sensitivity
    float f_comp = 1.0f;
    if (freq_hz > 250.0f) {
        f_comp = powf(250.0f / freq_hz, 0.70f);
    }

    return base_amp * f_comp;
}

static void fill_audio_buffer(uint32_t *buf, size_t count) {
    float phase_step = (current_freq > 0.0f) ? (current_freq * 2.0f * 3.14159265f / (float)AUDIO_SAMPLE_RATE) : 0.0f;
    const uint32_t RAMP_SAMPLES = 192; // 12 ms smooth raised-cosine fade-in/fade-out

    // Pre-calculate voice volume scale
    float voice_vol_norm = (float)master_volume_percent / 100.0f;
    float voice_gain = voice_vol_norm * 28.0f;

    for (size_t i = 0; i < count; i++) {
        int32_t mixed_sample = 0;

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
                env = 0.5f * (1.0f - cosf(3.14159265f * x));
            }

            // Warm musical waveform: 85% fundamental sine + 15% 2nd harmonic
            // Eliminates harsh, piercing clinical sine whistle!
            float s1 = sinf(current_phase);
            float s2 = sinf(current_phase * 2.0f);
            float warm_wave = 0.85f * s1 + 0.15f * s2;

            float val = warm_wave * (current_amplitude * env);
            mixed_sample += (int16_t)val;

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
                mixed_sample += (int32_t)((float)v_byte * voice_gain);
            } else {
                voice_active = false;
                active_voice_clip = NULL;
            }
        }

        // Clamp mixed sample to 16-bit signed range
        if (mixed_sample > 32767) mixed_sample = 32767;
        if (mixed_sample < -32768) mixed_sample = -32768;

        int16_t final_val = (int16_t)mixed_sample;

        // Stereo output (Left in MSW, Right in LSW)
        buf[i] = ((uint32_t)(uint16_t)final_val << 16) | ((uint32_t)(uint16_t)final_val);
    }
}

// Hardware DMA IRQ handler for seamless continuous double buffering
static void audio_dma_irq_handler(void) {
    if (dma_channel_get_irq0_status(dma_chan_a)) {
        dma_channel_acknowledge_irq0(dma_chan_a);
        fill_audio_buffer(buf_a, AUDIO_BUFFER_SAMPLES);
        dma_channel_set_trans_count(dma_chan_a, AUDIO_BUFFER_SAMPLES, false);
        dma_channel_set_read_addr(dma_chan_a, buf_a, false);
    }
    if (dma_channel_get_irq0_status(dma_chan_b)) {
        dma_channel_acknowledge_irq0(dma_chan_b);
        fill_audio_buffer(buf_b, AUDIO_BUFFER_SAMPLES);
        dma_channel_set_trans_count(dma_chan_b, AUDIO_BUFFER_SAMPLES, false);
        dma_channel_set_read_addr(dma_chan_b, buf_b, false);
    }
}

void audio_init(void) {
    audio_offset = pio_add_program(audio_pio, &audio_i2s_program);
    audio_i2s_program_init(audio_pio, audio_sm, audio_offset, PIN_SPK_DIN, PIN_SPK_BCLK);

    float pio_clock = (float)(AUDIO_SAMPLE_RATE * 32 * 2);
    float div = (float)clock_get_hz(clk_sys) / pio_clock;
    pio_sm_set_clkdiv(audio_pio, audio_sm, div);

    // Initial silent buffers
    for (size_t i = 0; i < AUDIO_BUFFER_SAMPLES; i++) {
        buf_a[i] = 0;
        buf_b[i] = 0;
    }

    // Configure Ping-Pong DMA
    dma_chan_a = dma_claim_unused_channel(true);
    dma_chan_b = dma_claim_unused_channel(true);

    dma_channel_config c_a = dma_channel_get_default_config(dma_chan_a);
    channel_config_set_transfer_data_size(&c_a, DMA_SIZE_32);
    channel_config_set_read_increment(&c_a, true);
    channel_config_set_write_increment(&c_a, false);
    channel_config_set_dreq(&c_a, pio_get_dreq(audio_pio, audio_sm, true));
    channel_config_set_chain_to(&c_a, dma_chan_b); // Chain to B

    dma_channel_configure(
        dma_chan_a,
        &c_a,
        &audio_pio->txf[audio_sm],
        buf_a,
        AUDIO_BUFFER_SAMPLES,
        false
    );

    dma_channel_config c_b = dma_channel_get_default_config(dma_chan_b);
    channel_config_set_transfer_data_size(&c_b, DMA_SIZE_32);
    channel_config_set_read_increment(&c_b, true);
    channel_config_set_write_increment(&c_b, false);
    channel_config_set_dreq(&c_b, pio_get_dreq(audio_pio, audio_sm, true));
    channel_config_set_chain_to(&c_b, dma_chan_a); // Chain to A

    dma_channel_configure(
        dma_chan_b,
        &c_b,
        &audio_pio->txf[audio_sm],
        buf_b,
        AUDIO_BUFFER_SAMPLES,
        false
    );

    // Setup DMA IRQ0 for seamless hardware background double-buffering
    dma_channel_set_irq0_enabled(dma_chan_a, true);
    dma_channel_set_irq0_enabled(dma_chan_b, true);
    irq_set_exclusive_handler(DMA_IRQ_0, audio_dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    // Enable PIO and start continuous double-buffered DMA stream
    pio_sm_set_enabled(audio_pio, audio_sm, true);
    dma_channel_start(dma_chan_a);
}

void audio_set_volume(uint8_t volume_percent) {
    if (volume_percent > 100) volume_percent = 100;
    master_volume_percent = volume_percent;
    current_amplitude = calculate_amplitude(current_freq);
}

uint8_t audio_get_volume(void) {
    return master_volume_percent;
}

void audio_play_tone(float freq_hz, uint32_t duration_ms) {
    uint32_t save = save_and_disable_interrupts();
    current_freq = freq_hz;
    current_amplitude = calculate_amplitude(freq_hz);
    total_note_samples = (AUDIO_SAMPLE_RATE * duration_ms) / 1000;
    samples_played = 0;
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
        active_voice_clip = clip;
        voice_sample_idx = 0;
        voice_active = true;
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
                // Loop or track finished
                audio_stop_chiptune();
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
}

bool audio_is_playing(void) {
    return (current_freq > 0.0f && samples_played < total_note_samples) ||
           (chime_index >= 0) ||
           (active_chiptune_track >= 0) ||
           voice_active;
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
        chime_index = -1; // Cancel boot chime if active
        active_chiptune_track = track_id;
        chiptune_step = 0;
        next_chiptune_step = get_absolute_time();
    }
}

void audio_stop_chiptune(void) {
    active_chiptune_track = -1;
    chiptune_step = 0;
    live_chiptune_freq = 0.0f;
    current_freq = 0.0f;
    samples_played = total_note_samples;
}

bool audio_is_chiptune_playing(void) {
    return (active_chiptune_track >= 0);
}

float audio_get_current_note_freq(void) {
    if (active_chiptune_track >= 0) {
        return live_chiptune_freq;
    }
    return current_freq;
}
