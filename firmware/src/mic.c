#include "mic.h"
#include "config.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "pico/time.h"
#include "mic_i2s.pio.h"
#include <stdlib.h>

#define MIC_SAMPLE_RATE     16000
#define MIC_BUFFER_SAMPLES  256   // 256 samples * 4 bytes = 1024 bytes (16 ms)

static PIO mic_pio = pio0;
static uint mic_sm = 0;
static uint mic_offset = 0;
static int mic_dma_chan = -1;

// 1024-byte aligned ring buffer for zero-overhead DMA circular writing
static volatile uint32_t mic_ring_buf[MIC_BUFFER_SAMPLES] __attribute__((aligned(1024)));
static uint32_t mic_snapshot[MIC_BUFFER_SAMPLES];

static int16_t last_peak_amplitude = 0;
static uint8_t rolling_level_percent = 0;
static absolute_time_t last_metric_update;

// Voice recording RAM buffer (up to 5.0 s @ 16 kHz = 80,000 samples = 160 KB)
static int16_t mic_record_buf[MIC_RECORD_MAX_SAMPLES];
static volatile size_t mic_record_count = 0;
static volatile bool mic_record_active = false;
static size_t mic_record_drain_cursor = 0;
static int16_t record_session_peak = 0;

static mic_stream_callback_t mic_stream_cb = NULL;
static int16_t stream_buf[256];
static size_t stream_buf_count = 0;

void mic_set_stream_callback(mic_stream_callback_t callback) {
    mic_stream_cb = callback;
}

void mic_init(void) {
    mic_offset = pio_add_program(mic_pio, &mic_i2s_program);
    mic_i2s_program_init(mic_pio, mic_sm, mic_offset, PIN_MIC_SD, PIN_MIC_SCK);

    // Clock calculation:
    // 16,000 samples/s * 64 BCLK cycles/frame * 2 PIO instructions/BCLK = 2,048,000 Hz
    float pio_clock = (float)(MIC_SAMPLE_RATE * 64 * 2);
    float div = (float)clock_get_hz(clk_sys) / pio_clock;
    pio_sm_set_clkdiv(mic_pio, mic_sm, div);

    // Clear buffer initially
    for (int i = 0; i < MIC_BUFFER_SAMPLES; i++) {
        mic_ring_buf[i] = 0;
    }

    // Configure circular ring-buffer DMA
    mic_dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(mic_dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(mic_pio, mic_sm, false)); // RX DREQ
    channel_config_set_ring(&c, true, 10); // 1 << 10 = 1024 bytes write wrap

    dma_channel_configure(
        mic_dma_chan,
        &c,
        mic_ring_buf,
        &mic_pio->rxf[mic_sm],
        dma_encode_endless_transfer_count(),
        false
    );

    pio_sm_set_enabled(mic_pio, mic_sm, true);
    dma_channel_start(mic_dma_chan);

    last_metric_update = get_absolute_time();
}

static int32_t dc_filter_x = 0;
static int32_t dc_filter_y = 0;

static inline int16_t dc_block_filter(int16_t x) {
    // High-pass filter: y[n] = x[n] - x[n-1] + 0.992 * y[n-1]
    int32_t y = (int32_t)x - dc_filter_x + ((dc_filter_y * 254) >> 8);
    dc_filter_x = x;
    dc_filter_y = y;
    if (y > 32767) y = 32767;
    if (y < -32768) y = -32768;
    return (int16_t)y;
}

void mic_update(void) {
    // RP2350 endless mode does not decrement a finite transfer count.
    // Restart only if the channel was stopped.
    if (mic_dma_chan >= 0 && !dma_channel_is_busy(mic_dma_chan)) {
        dma_channel_set_trans_count(mic_dma_chan, dma_encode_endless_transfer_count(), true);
    }

    if (mic_dma_chan >= 0) {
        uintptr_t cur_write_addr = dma_channel_hw_addr(mic_dma_chan)->write_addr;
        size_t current_hw_idx = ((cur_write_addr - (uintptr_t)mic_ring_buf) / sizeof(mic_ring_buf[0])) % MIC_BUFFER_SAMPLES;

        // 1. Drain new samples into the recording buffer and streaming callback if recording
        if (mic_record_active) {
            int16_t drain_peak = 0;
            while (mic_record_drain_cursor != current_hw_idx) {
                uint32_t raw = mic_ring_buf[mic_record_drain_cursor];
                int16_t raw_sample = (int16_t)((int32_t)(raw << 1) >> 16);
                int16_t sample = dc_block_filter(raw_sample);

                // Store in local RAM buffer for offline replay (up to MIC_RECORD_MAX_SAMPLES)
                if (mic_record_count < MIC_RECORD_MAX_SAMPLES) {
                    mic_record_buf[mic_record_count] = sample;
                }
                mic_record_count++; // Total session sample count

                // Stream in 256-sample chunks (16 ms) over network callback
                stream_buf[stream_buf_count++] = sample;
                if (stream_buf_count >= 256) {
                    if (mic_stream_cb != NULL) {
                        mic_stream_cb(stream_buf, stream_buf_count);
                    }
                    stream_buf_count = 0;
                }

                int32_t abs_s = (sample < 0) ? -(int32_t)sample : sample;
                if (abs_s > drain_peak) drain_peak = (int16_t)abs_s;
                mic_record_drain_cursor = (mic_record_drain_cursor + 1) % MIC_BUFFER_SAMPLES;

                // Enforce maximum query session limit of 60 seconds (960,000 samples)
                if (mic_record_count >= (size_t)MIC_SAMPLE_RATE * MIC_QUERY_MAX_SECONDS) {
                    if (stream_buf_count > 0 && mic_stream_cb != NULL) {
                        mic_stream_cb(stream_buf, stream_buf_count);
                        stream_buf_count = 0;
                    }
                    mic_record_active = false;
                    break;
                }
            }
            if (drain_peak > last_peak_amplitude) {
                last_peak_amplitude = drain_peak;
            }
            if (drain_peak > record_session_peak) {
                record_session_peak = drain_peak;
            }
        } else {
            mic_record_drain_cursor = current_hw_idx;
        }
    }

    // Update metrics every 20 ms (~50 Hz)
    if (absolute_time_diff_us(last_metric_update, get_absolute_time()) < 20000) {
        return;
    }
    last_metric_update = get_absolute_time();

    int16_t current_max = 0;
    // DMA can overwrite the oldest slot while we read it. Accept a copy only
    // when its cursor stays still; bound retries so controls remain responsive.
    uintptr_t write_addr = 0;
    bool snapshot_ready = false;
    for (int attempt = 0; attempt < 5; attempt++) {
        absolute_time_t copy_start = get_absolute_time();
        write_addr = dma_channel_hw_addr(mic_dma_chan)->write_addr;
        for (int i = 0; i < MIC_BUFFER_SAMPLES; i++) {
            mic_snapshot[i] = mic_ring_buf[i];
        }
        uintptr_t after = dma_channel_hw_addr(mic_dma_chan)->write_addr;
        // Accept if cursor was stable, or on final attempt if copy was sub-millisecond (no ring wrap)
        if ((write_addr == after || attempt == 4) && absolute_time_diff_us(copy_start, get_absolute_time()) < 16000) {
            snapshot_ready = true;
            break;
        }
    }
    if (!snapshot_ready) return;

    // DMA's next write position is the oldest sample in the full ring.
    size_t oldest = ((write_addr - (uintptr_t)mic_ring_buf) / sizeof(mic_ring_buf[0])) % MIC_BUFFER_SAMPLES;
    for (int i = 0; i < MIC_BUFFER_SAMPLES; i++) {
        uint32_t raw = mic_snapshot[(oldest + i) % MIC_BUFFER_SAMPLES];
        // INMP441 24-bit audio inside 32-bit slot: bit 30 is MSB
        int16_t raw_sample = (int16_t)((int32_t)(raw << 1) >> 16);
        int16_t sample = dc_block_filter(raw_sample);
        int32_t abs_s = (sample < 0) ? -(int32_t)sample : sample;
        if (abs_s > 32767) abs_s = 32767;
        if (abs_s > current_max) {
            current_max = abs_s;
        }
    }

    last_peak_amplitude = current_max;

    // Noise floor gate (ignore ambient hum below 500)
    int instantaneous_pct = 0;
    if (current_max > 500) {
        instantaneous_pct = ((current_max - 500) * 100) / 10000;
        if (instantaneous_pct > 100) instantaneous_pct = 100;
    }

    // Fast attack, smooth decay
    if ((uint8_t)instantaneous_pct > rolling_level_percent) {
        rolling_level_percent = (uint8_t)instantaneous_pct;
    } else {
        if (rolling_level_percent > 3) {
            rolling_level_percent -= 3;
        } else {
            rolling_level_percent = 0;
        }
    }
}

uint8_t mic_get_level_percent(void) {
    return rolling_level_percent;
}

int16_t mic_get_peak_amplitude(void) {
    return last_peak_amplitude;
}

void mic_record_start(void) {
    if (mic_dma_chan >= 0) {
        uintptr_t write_addr = dma_channel_hw_addr(mic_dma_chan)->write_addr;
        mic_record_drain_cursor = ((write_addr - (uintptr_t)mic_ring_buf) / sizeof(mic_ring_buf[0])) % MIC_BUFFER_SAMPLES;
    } else {
        mic_record_drain_cursor = 0;
    }
    mic_record_count = 0;
    stream_buf_count = 0;
    record_session_peak = 0;
    mic_record_active = true;
}

size_t mic_record_stop(void) {
    if (mic_record_active) {
        mic_record_active = false;
        if (stream_buf_count > 0 && mic_stream_cb != NULL) {
            mic_stream_cb(stream_buf, stream_buf_count);
            stream_buf_count = 0;
        }
    }
    return mic_record_count;
}

bool mic_is_recording(void) {
    return mic_record_active;
}

const int16_t *mic_get_recorded_samples(size_t *out_sample_count) {
    if (out_sample_count) {
        *out_sample_count = (mic_record_count > MIC_RECORD_MAX_SAMPLES) ?
                            MIC_RECORD_MAX_SAMPLES : mic_record_count;
    }
    return mic_record_buf;
}

size_t mic_get_recorded_count(void) {
    return mic_record_count;
}

uint32_t mic_get_recorded_duration_ms(void) {
    return (uint32_t)((mic_record_count * 1000ULL) / MIC_SAMPLE_RATE);
}

void mic_record_clear(void) {
    mic_record_active = false;
    mic_record_count = 0;
    mic_record_drain_cursor = 0;
    record_session_peak = 0;
}

int16_t mic_get_record_session_peak(void) {
    return record_session_peak;
}
