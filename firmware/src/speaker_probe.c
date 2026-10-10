// Isolated MAX98357A experiment. No application, encoder, LED, mic or DMA IRQ.
#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/pio.h"
#include "config.h"
#include "audio_i2s.pio.h"
#include <math.h>
#include <stdio.h>

#define PROBE_VERSION "CELESTIAL-SPEAKER-PROBE-03"
#define PROBE_RATE 16000u
#define PROBE_FRAMES 4096u // 256 ms; 128 complete cycles of 500 Hz.
#define PROBE_TAIL 32u     // 2 ms of zero PCM at the end.
_Static_assert(AUDIO_I2S_SLOT_BITS == 16, "Probe uses the current 16-bit I2S program");

static PIO const probe_pio = pio1;
static const uint probe_sm = 0;
static uint probe_offset;
static int probe_dma;
static uint32_t probe_words[PROBE_FRAMES + PROBE_TAIL];
static bool probe_active;
static absolute_time_t probe_started;
static absolute_time_t probe_drain;
static bool probe_draining;
static int probe_min, probe_max;

static void probe_status(void) {
    printf("[PROBE] FW:%s | build:%s %s | sys_hz:%lu\n", PROBE_VERSION,
           __DATE__, __TIME__, (unsigned long)clock_get_hz(clk_sys));
    printf("[PROBE] GP%u=BCLK GP%u=LRC GP%u=DIN | I2S signed16 packed L/R | LRCLK:16000 BCLK:512000 | active:%u | min16:%d max16:%d | remaining:%lu\n",
           (unsigned)PIN_SPK_BCLK, (unsigned)PIN_SPK_LRC, (unsigned)PIN_SPK_DIN,
           (unsigned)probe_active, probe_min, probe_max,
           (unsigned long)dma_channel_hw_addr(probe_dma)->transfer_count);
}

static void probe_stop(void) {
    pio_sm_set_enabled(probe_pio, probe_sm, false);
    hw_clear_bits(&dma_channel_hw_addr(probe_dma)->ctrl_trig, DMA_CH0_CTRL_TRIG_EN_BITS);
    dma_channel_abort(probe_dma);
    pio_sm_clear_fifos(probe_pio, probe_sm);
    uint32_t pins = (1u << PIN_SPK_BCLK) | (1u << PIN_SPK_LRC) | (1u << PIN_SPK_DIN);
    pio_sm_set_pins_with_mask(probe_pio, probe_sm, 0, pins);
    probe_active = false;
    probe_draining = false;
}

static void probe_play(bool positive_only, int amplitude) {
    probe_stop();
    probe_min = 32767;
    probe_max = -32768;
    for (uint i = 0; i < PROBE_FRAMES; ++i) {
        float envelope = 1.0f;
        if (i < 160) envelope = (float)i / 160.0f;
        uint remaining = PROBE_FRAMES - 1u - i;
        if (remaining < 160 && (float)remaining / 160.0f < envelope)
            envelope = (float)remaining / 160.0f;
        float sine = sinf(6.28318530718f * (float)(i % 32u) / 32.0f);
        int16_t sample = (int16_t)lroundf((positive_only ? 1.0f + sine : sine) * amplitude * envelope);
        if (sample < probe_min) probe_min = sample;
        if (sample > probe_max) probe_max = sample;
        probe_words[i] = ((uint32_t)(uint16_t)sample << 16) | (uint32_t)(uint16_t)sample;
    }
    for (uint i = PROBE_FRAMES; i < PROBE_FRAMES + PROBE_TAIL; ++i) probe_words[i] = 0;
    printf("[PROBE] %s 500Hz, amplitude16:%d, min16:%d max16:%d, 256ms + zero tail\n",
           positive_only ? "POSITIVE" : "BIPOLAR", amplitude, probe_min, probe_max);
    pio_sm_restart(probe_pio, probe_sm);
    pio_sm_clkdiv_restart(probe_pio, probe_sm);
    pio_sm_exec(probe_pio, probe_sm, pio_encode_jmp(probe_offset + audio_i2s_offset_entry_point));
    dma_channel_set_read_addr(probe_dma, probe_words, false);
    dma_channel_set_trans_count(probe_dma, PROBE_FRAMES + PROBE_TAIL, false);
    // Prime the FIFO before clocks start. There is no interrupt refill or chaining.
    hw_set_bits(&dma_channel_hw_addr(probe_dma)->al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
    dma_channel_start(probe_dma);
    pio_sm_set_enabled(probe_pio, probe_sm, true);
    probe_started = get_absolute_time();
    probe_active = true;
}

int main(void) {
    stdio_init_all();
    pio_sm_claim(probe_pio, probe_sm);
    probe_offset = pio_add_program(probe_pio, &audio_i2s_program);
    audio_i2s_program_init(probe_pio, probe_sm, probe_offset, PIN_SPK_DIN, PIN_SPK_BCLK);
    pio_sm_set_clkdiv(probe_pio, probe_sm, (float)clock_get_hz(clk_sys) / (PROBE_RATE * 16u * 2u * 2u));
    probe_dma = dma_claim_unused_channel(true);
    dma_channel_config cfg = dma_channel_get_default_config(probe_dma);
    channel_config_set_transfer_data_size(&cfg, DMA_SIZE_32);
    channel_config_set_read_increment(&cfg, true);
    channel_config_set_write_increment(&cfg, false);
    channel_config_set_dreq(&cfg, pio_get_dreq(probe_pio, probe_sm, true));
    dma_channel_configure(probe_dma, &cfg, &probe_pio->txf[probe_sm], probe_words, 0, false);
    probe_stop(); // Boot stays silent and all three audio signals start low.
    sleep_ms(2000);
    probe_status();
    printf("[PROBE] U=positive amplitude8; B=bipolar amplitude8; 2=positive amplitude16; A=status; S=stop. No automatic playback.\n");
    while (true) {
        int c = getchar_timeout_us(0);
        if (c == 'S' || c == 's') { probe_stop(); printf("[PROBE] STOP\n"); }
        else if (c == 'A' || c == 'a' || c == '?') probe_status();
        else if (!probe_active && (c == 'U' || c == 'u')) probe_play(true, 8);
        else if (!probe_active && (c == 'B' || c == 'b')) probe_play(false, 8);
        else if (!probe_active && c == '2') probe_play(true, 16);
        if (probe_active) {
            if (!dma_channel_is_busy(probe_dma)) {
                if (!probe_draining) { probe_draining = true; probe_drain = get_absolute_time(); }
                // The joined FIFO holds at most 8 frames (500 us), plus the OSR.
                if (absolute_time_diff_us(probe_drain, get_absolute_time()) >= 1000) {
                    probe_stop(); printf("[PROBE] DONE\n"); probe_status();
                }
            }
            if (probe_active && absolute_time_diff_us(probe_started, get_absolute_time()) >= 300000) {
                probe_stop(); printf("[PROBE] DMA TIMEOUT: stopped\n"); probe_status();
            }
        }
        sleep_ms(1);
    }
}
