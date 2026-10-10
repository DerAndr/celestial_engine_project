#include "audio.h"
#include "config.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "pico/time.h"
#include "audio_i2s.pio.h"
#include <math.h>

#define AUDIO_SAMPLE_RATE     16000
#define AUDIO_BUFFER_SAMPLES  512   // 512 samples = 32 ms per buffer (64 ms ping-pong headroom)

static PIO audio_pio = pio1;
static uint audio_sm = 0;
static uint audio_offset = 0;

static int dma_chan_a = -1;
static int dma_chan_b = -1;

static uint32_t buf_a[AUDIO_BUFFER_SAMPLES];
static uint32_t buf_b[AUDIO_BUFFER_SAMPLES];
static bool next_buf_is_a = true;

// Tone synthesizer state with smooth attack/decay envelope
// Master amplitude scale: amplitude = master_volume_percent * 20.0f
// Default startup volume: 25% -> amplitude = 500.0f (clear, crisp, musical, zero crackle)
// Tone synthesizer state with smooth attack/decay envelope
// Master amplitude scale: amplitude = master_volume_percent * 100.0f
// Default startup volume: 25% -> amplitude = 2500.0f (rich, loud, crystal clear, zero crackle)
#define AUDIO_MAX_AMPLITUDE   10000.0f
static uint8_t master_volume_percent = 25;
static float current_amplitude = 2500.0f;
static float current_freq = 0.0f;
static float current_phase = 0.0f;
static uint32_t total_note_samples = 0;
static uint32_t samples_played = 0;

// Celestial chime sequence state (tuned to high-efficiency frequencies 784 - 1568 Hz for miniature speakers)
typedef struct {
    float freq;
    uint32_t dur_ms;
} chime_note_t;

static const chime_note_t CHIME_SEQUENCE[] = {
    { 783.99f,  80 }, // G5 - bright, celestial
    { 1046.50f, 80 }, // C6 - crystal clear
    { 1318.51f, 80 }, // E6 - soaring
    { 1567.98f, 220 } // G6 - ringing bell resolution
};
static int chime_index = -1;
static absolute_time_t next_note_time;

static void fill_audio_buffer(uint32_t *buf, size_t count) {
    float phase_step = (current_freq > 0.0f) ? (current_freq * 2.0f * 3.14159265f / (float)AUDIO_SAMPLE_RATE) : 0.0f;
    const uint32_t RAMP_SAMPLES = 80; // 5 ms smooth fade-in and fade-out to eliminate speaker crackle

    for (size_t i = 0; i < count; i++) {
        int16_t sample_val = 0;

        if (samples_played < total_note_samples && current_freq > 0.0f) {
            float env = 1.0f;
            // Smooth attack ramp (first 5 ms)
            if (samples_played < RAMP_SAMPLES) {
                env = (float)samples_played / (float)RAMP_SAMPLES;
            }
            // Smooth decay ramp (last 5 ms)
            uint32_t remaining = total_note_samples - samples_played;
            if (remaining < RAMP_SAMPLES) {
                env = (float)remaining / (float)RAMP_SAMPLES;
            }

            float val = sinf(current_phase) * (current_amplitude * env);
            sample_val = (int16_t)val;

            current_phase += phase_step;
            if (current_phase >= 2.0f * 3.14159265f) {
                current_phase -= 2.0f * 3.14159265f;
            }
            samples_played++;
        } else {
            current_freq = 0.0f;
            current_phase = 0.0f;
        }

        // Stereo output (Left in MSW, Right in LSW)
        buf[i] = ((uint32_t)(uint16_t)sample_val << 16) | ((uint32_t)(uint16_t)sample_val);
    }
}

static float get_volume_amplitude(void) {
    if (master_volume_percent == 0) return 0.0f;
    return (float)master_volume_percent * 100.0f;
}

// Hardware DMA IRQ handler for zero-jitter, glitch-free continuous double buffering
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

    next_buf_is_a = false;
    current_amplitude = get_volume_amplitude();

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
    current_amplitude = get_volume_amplitude();
}

uint8_t audio_get_volume(void) {
    return master_volume_percent;
}

void audio_play_tone(float freq_hz, uint32_t duration_ms) {
    current_freq = freq_hz;
    current_amplitude = get_volume_amplitude();
    total_note_samples = (AUDIO_SAMPLE_RATE * duration_ms) / 1000;
    samples_played = 0;
}

void audio_play_blip(void) {
    audio_play_tone(1046.50f, 15); // Short crisp C6 chime click with envelope
}

void audio_play_chime(void) {
    chime_index = 0;
    audio_play_tone(CHIME_SEQUENCE[0].freq, CHIME_SEQUENCE[0].dur_ms);
    next_note_time = delayed_by_ms(get_absolute_time(), CHIME_SEQUENCE[0].dur_ms + 15);
}

void audio_update(void) {
    // Process musical chime sequencer in main loop
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
    return (current_freq > 0.0f && samples_played < total_note_samples) || (chime_index >= 0);
}
