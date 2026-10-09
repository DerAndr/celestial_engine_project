#include "audio.h"
#include "config.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "pico/time.h"
#include "audio_i2s.pio.h"
#include <math.h>

#define AUDIO_SAMPLE_RATE     16000
#define AUDIO_BUFFER_SAMPLES  256   // 256 samples = 16 ms per buffer

static PIO audio_pio = pio1;
static uint audio_sm = 0;
static uint audio_offset = 0;

static int dma_chan_a = -1;
static int dma_chan_b = -1;

static uint32_t buf_a[AUDIO_BUFFER_SAMPLES];
static uint32_t buf_b[AUDIO_BUFFER_SAMPLES];
static bool next_buf_is_a = true;

// Tone synthesizer state
#define AUDIO_MAX_AMPLITUDE   7000.0f
static uint8_t master_volume_percent = 25; // Default 25% for gentle comfortable volume
static float current_amplitude = 1750.0f;
static float current_freq = 0.0f;
static float current_phase = 0.0f;
static uint32_t samples_remaining = 0;

// Chime sequence state
typedef struct {
    float freq;
    uint32_t dur_ms;
} chime_note_t;

static const chime_note_t CHIME_SEQUENCE[] = {
    { 523.25f,  70 }, // C5
    { 659.25f,  70 }, // E5
    { 783.99f,  70 }, // G5
    { 1046.50f, 180 } // C6
};
static int chime_index = -1;
static absolute_time_t next_note_time;

static void fill_audio_buffer(uint32_t *buf, size_t count) {
    float phase_step = (current_freq > 0.0f) ? (current_freq * 2.0f * 3.14159265f / (float)AUDIO_SAMPLE_RATE) : 0.0f;

    for (size_t i = 0; i < count; i++) {
        int16_t sample_val = 0;

        if (samples_remaining > 0 && current_freq > 0.0f) {
            float val = sinf(current_phase) * current_amplitude;
            sample_val = (int16_t)val;
            current_phase += phase_step;
            if (current_phase >= 2.0f * 3.14159265f) {
                current_phase -= 2.0f * 3.14159265f;
            }
            samples_remaining--;
        } else if (samples_remaining == 0) {
            current_freq = 0.0f;
        }

        // Stereo output (Left in MSW, Right in LSW)
        buf[i] = ((uint32_t)(uint16_t)sample_val << 16) | ((uint32_t)(uint16_t)sample_val);
    }
}

void audio_init(void) {
    audio_offset = pio_add_program(audio_pio, &audio_i2s_program);
    audio_i2s_program_init(audio_pio, audio_sm, audio_offset, PIN_SPK_DIN, PIN_SPK_BCLK);

    // Clock calculation:
    // 16,000 samples/s * 32 BCLK cycles/sample * 2 PIO cycles/BCLK = 1,024,000 Hz
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

    // Enable state machine and start streaming
    pio_sm_set_enabled(audio_pio, audio_sm, true);
    dma_channel_start(dma_chan_a);
}

void audio_set_volume(uint8_t volume_percent) {
    if (volume_percent > 100) volume_percent = 100;
    master_volume_percent = volume_percent;
    current_amplitude = (AUDIO_MAX_AMPLITUDE * (float)master_volume_percent) / 100.0f;
}

uint8_t audio_get_volume(void) {
    return master_volume_percent;
}

void audio_play_tone(float freq_hz, uint32_t duration_ms) {
    current_freq = freq_hz;
    current_amplitude = (AUDIO_MAX_AMPLITUDE * (float)master_volume_percent) / 100.0f;
    samples_remaining = (AUDIO_SAMPLE_RATE * duration_ms) / 1000;
}

void audio_play_blip(void) {
    audio_play_tone(880.0f, 15); // Short subtle click
}

void audio_play_chime(void) {
    chime_index = 0;
    audio_play_tone(CHIME_SEQUENCE[0].freq, CHIME_SEQUENCE[0].dur_ms);
    next_note_time = delayed_by_ms(get_absolute_time(), CHIME_SEQUENCE[0].dur_ms + 10);
}

void audio_update(void) {
    // 1. Process musical chime sequencer
    if (chime_index >= 0) {
        if (absolute_time_diff_us(next_note_time, get_absolute_time()) >= 0) {
            chime_index++;
            if (chime_index < (int)(sizeof(CHIME_SEQUENCE) / sizeof(CHIME_SEQUENCE[0]))) {
                audio_play_tone(CHIME_SEQUENCE[chime_index].freq, CHIME_SEQUENCE[chime_index].dur_ms);
                next_note_time = delayed_by_ms(get_absolute_time(), CHIME_SEQUENCE[chime_index].dur_ms + 10);
            } else {
                chime_index = -1; // Finished
            }
        }
    }

    // 2. Replenish DMA buffers when inactive
    if (!dma_channel_is_busy(dma_chan_a) && next_buf_is_a) {
        fill_audio_buffer(buf_a, AUDIO_BUFFER_SAMPLES);
        dma_channel_set_read_addr(dma_chan_a, buf_a, false);
        next_buf_is_a = false;
    } else if (!dma_channel_is_busy(dma_chan_b) && !next_buf_is_a) {
        fill_audio_buffer(buf_b, AUDIO_BUFFER_SAMPLES);
        dma_channel_set_read_addr(dma_chan_b, buf_b, false);
        next_buf_is_a = true;
    }
}

bool audio_is_playing(void) {
    return (current_freq > 0.0f && samples_remaining > 0) || (chime_index >= 0);
}
