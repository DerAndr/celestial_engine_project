#include "audio.h"
#include "audio_ring_buffer.h"
#include "chiptune_synth.h"
#include "config.h"
#include "voice_data.h"
#include "music_data.h"
#include "max98357a_driver.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include <math.h>

#define AUDIO_SAMPLE_RATE     16000
#define AUDIO_BUFFER_SAMPLES  MAX98357A_BUFFER_WORDS
#define AUDIO_BUFFER_WORDS    MAX98357A_BUFFER_WORDS

static PIO audio_pio = pio1;
static uint audio_sm = 0;

// Percentage controls digital gain. Acoustic level must be verified on the actual assembly.
static volatile uint8_t master_volume_percent = AUDIO_DEFAULT_VOLUME;
static volatile float master_gain = 0.0f;
static absolute_time_t silence_since;
static void amp_sleep(void);
static void amp_wake(void);
static volatile bool diagnostic_sine = false;
// Fault isolation (exp. 016): 0 = normal signed sine, +1 = samples >= 0 only, -1 = samples <= 0 only.
static volatile int diagnostic_polarity = 0;
static volatile uint32_t playback_peak_pcm24 = 0;

// Tone synthesizer state (for boot chime, clicks, alerts, diagnostics)
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
static int16_t audio_stream_ring_storage[AUDIO_STREAM_RING_SIZE];
static audio_ring_buffer_t audio_stream_rb = {
    .storage         = audio_stream_ring_storage,
    .capacity        = AUDIO_STREAM_RING_SIZE,
    .mask            = AUDIO_STREAM_RING_SIZE - 1u,
    .write_idx       = 0u,
    .read_idx        = 0u,
    .overflow_count  = 0u,
    .underflow_count = 0u
};
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
    const uint32_t RAMP_SAMPLES = 192; // 12 ms smooth raised-cosine fade-in/fade-out for standalone tones

    float gain = master_gain;
    float voice_gain = AUDIO_PCM16_PEAK / 128.0f;
    const float pcm16_gain = 5.0f; // Pre-amp gain boost (+14 dB) for natural microphone voice replay
    const float stream_gain = 0.85f; // Calibrated 1:1 dynamic range for clear speech without digital clipping
    uint32_t buffer_peak = 0;

    for (size_t i = 0; i < count; i++) {
        float mixed_sample = 0.0f;

        // 1. Chiptune retro arcade & Cosmic SFX synthesis (sample-accurate hardware sequencer)
        float chiptune_sample = 0.0f;
        if (chiptune_synth_render_sample(&chiptune_sample)) {
            mixed_sample += chiptune_sample;
        }
        // 1b. Synthesize standalone musical tone (chimes, clicks, alerts)
        else if (samples_played < total_note_samples && current_freq > 0.0f) {
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
            int16_t s = 0;
            if (audio_rb_pop_sample(&audio_stream_rb, &s)) {
                mixed_sample += (float)s * stream_gain;
            } else if (audio_stream_eof) {
                audio_stream_active = false;
            } else {
                // Buffer momentarily dry before EOF: underflow recorded by audio_rb_pop_sample;
                // never stop DAC or abort DMA mid-stream; next UDP packet resumes instantly.
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

void audio_init(void) {
    master_volume_percent = AUDIO_DEFAULT_VOLUME;
    master_gain = calculate_volume_gain(AUDIO_DEFAULT_VOLUME);
    chiptune_synth_init();
    audio_rb_init(&audio_stream_rb, audio_stream_ring_storage, AUDIO_STREAM_RING_SIZE);
    max98357a_driver_init(audio_pio, audio_sm, PIN_SPK_DIN, PIN_SPK_BCLK, PIN_SPK_LRC, AUDIO_SAMPLE_RATE, fill_audio_buffer);
}

void audio_set_volume(uint8_t volume_percent) {
    if (volume_percent > 100) volume_percent = 100;
    float gain = calculate_volume_gain(volume_percent);
    uint32_t save = save_and_disable_interrupts();
    master_volume_percent = volume_percent;
    master_gain = gain;
    if (volume_percent == 0) {
        audio_rb_reset(&audio_stream_rb);
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
    if (master_volume_percent > 0) {
        max98357a_driver_wake();
    }
}

static void amp_sleep(void) {
    max98357a_driver_sleep();
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
    audio_rb_reset(&audio_stream_rb);
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
    if (audio_stream_eof || !audio_rb_push_samples(&audio_stream_rb, samples, count)) {
        restore_interrupts(save);
        return false; /* Sender must retry; never overwrite queued speech. */
    }
    // Pre-buffer 0.10s (1600 samples) or EOF before kicking off DAC
    if (!audio_stream_active && ((audio_rb_available_read(&audio_stream_rb) >= 1600u) || audio_stream_eof)) {
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
    if (audio_rb_available_read(&audio_stream_rb) > 0u) {
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
    // 1. Process chiptune / cosmic SFX engine updates
    chiptune_synth_update();

    if (current_music_track_idx >= 0) {
        bool finished = false;
        const chiptune_track_t *trk = chiptune_synth_get_track(current_music_track_idx);
        if (trk != NULL) {
            if (trk->type == TRACK_TYPE_ADPCM) {
                if (!adpcm_active) finished = true;
            } else {
                if (!chiptune_synth_is_playing()) finished = true;
            }
        } else {
            finished = true;
        }

        if (finished) {
            int track_count = chiptune_synth_get_count();
            if (music_play_mode == AUDIO_PLAY_ALL) {
                if (current_music_track_idx + 1 < track_count) {
                    audio_play_chiptune(current_music_track_idx + 1);
                } else {
                    current_music_track_idx = -1;
                }
            } else if (music_play_mode == AUDIO_PLAY_CYCLE) {
                int next = (current_music_track_idx + 1) % track_count;
                audio_play_chiptune(next);
            } else {
                current_music_track_idx = -1;
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
    if (audio_stream_active && !audio_stream_eof && (audio_rb_available_read(&audio_stream_rb) == 0u)) {
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
           chiptune_synth_is_playing() ||
           chiptune_synth_is_sfx_playing() ||
           adpcm_active ||
           voice_active ||
           pcm16_active ||
           audio_stream_active;
}

int audio_get_chiptune_count(void) {
    return chiptune_synth_get_count();
}

const char* audio_get_chiptune_name(int track_id) {
    return chiptune_synth_get_name(track_id);
}

void audio_play_chiptune(int track_id) {
    if (track_id >= 0 && track_id < chiptune_synth_get_count()) {
        audio_stop_cosmic_sfx();
        chime_index = -1; // Cancel boot chime if active

        uint32_t save = save_and_disable_interrupts();
        current_music_track_idx = track_id;
        const chiptune_track_t *trk = chiptune_synth_get_track(track_id);
        if (trk != NULL && trk->type == TRACK_TYPE_ADPCM) {
            chiptune_synth_stop();
            active_adpcm_clip = music_get_adpcm_clip((music_adpcm_id_t)trk->adpcm_id);
            adpcm_sample_idx = 0;
            adpcm_predicted = 0;
            adpcm_step_idx = 0;
            adpcm_active = true;
        } else {
            adpcm_active = false;
            active_adpcm_clip = NULL;
            chiptune_synth_play(track_id);
        }
        amp_wake();
        restore_interrupts(save);
    }
}

void audio_stop_chiptune(void) {
    uint32_t save = save_and_disable_interrupts();
    chiptune_synth_stop();
    adpcm_active = false;
    active_adpcm_clip = NULL;
    current_music_track_idx = -1;
    restore_interrupts(save);
}

bool audio_is_chiptune_playing(void) {
    return chiptune_synth_is_playing() || adpcm_active;
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
    return chiptune_synth_get_sfx_count();
}

const char* audio_get_cosmic_sfx_name(int sfx_id) {
    return chiptune_synth_get_sfx_name(sfx_id);
}

void audio_play_cosmic_sfx(int sfx_id) {
    if (sfx_id >= 0 && sfx_id < chiptune_synth_get_sfx_count()) {
        audio_stop_chiptune();
        chime_index = -1;
        chiptune_synth_play_sfx(sfx_id);
        amp_wake();
    }
}

void audio_stop_cosmic_sfx(void) {
    chiptune_synth_stop_sfx();
}

bool audio_is_cosmic_sfx_playing(void) {
    return chiptune_synth_is_sfx_playing();
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
    audio_play_cosmic_sfx(COSMIC_SFX_R2D2_HAPPY);
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
    if (chiptune_synth_is_playing() || chiptune_synth_is_sfx_playing()) {
        return chiptune_synth_get_current_freq();
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
    max98357a_stats_t stats;
    max98357a_driver_get_stats(&stats);
    uint32_t save = save_and_disable_interrupts();
    uint32_t peak = playback_peak_pcm24;
    restore_interrupts(save);
    LOG_INFO("AUDIO", "FW:%s | limit16:%u | 16kHz / 16-bit stereo I2S | Vol:%u%% | peak:%lu | refill_max_us:%lu | tx_stalls:%lu",
             FIRMWARE_VERSION, (unsigned)AUDIO_PCM16_PEAK, (unsigned)audio_get_volume(), (unsigned long)peak,
             (unsigned long)stats.max_refill_us, (unsigned long)stats.tx_stall_observations);
    LOG_INFO("AUDIO", "restarts:%lu | late_refills:%lu",
             (unsigned long)stats.dma_restarts, (unsigned long)stats.late_refills);
}
