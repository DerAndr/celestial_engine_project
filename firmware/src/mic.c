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
static uint32_t mic_ring_buf[MIC_BUFFER_SAMPLES] __attribute__((aligned(1024)));

static int16_t last_peak_amplitude = 0;
static uint8_t rolling_level_percent = 0;
static absolute_time_t last_metric_update;

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
        0xFFFFFFFF,
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
    // Restart DMA transfer if it ever nears wrap limit
    if (!dma_channel_is_busy(mic_dma_chan)) {
        dma_channel_set_trans_count(mic_dma_chan, 0xFFFFFFFF, true);
    }

    // Update metrics every 20 ms (~50 Hz)
    if (absolute_time_diff_us(last_metric_update, get_absolute_time()) < 20000) {
        return;
    }
    last_metric_update = get_absolute_time();

    int16_t current_max = 0;
    for (int i = 0; i < MIC_BUFFER_SAMPLES; i++) {
        uint32_t raw = mic_ring_buf[i];
        // INMP441 24-bit audio inside 32-bit slot: bit 30 is MSB
        int16_t raw_sample = (int16_t)((int32_t)(raw << 1) >> 16);
        int16_t sample = dc_block_filter(raw_sample);
        int16_t abs_s = (sample < 0) ? -sample : sample;
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
