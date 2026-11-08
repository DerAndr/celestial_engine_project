#include "max98357a_driver.h"
#include "config.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "pico/time.h"
#include "audio_i2s.pio.h"

_Static_assert(MAX98357A_BUFFER_WORDS * sizeof(uint32_t) == (1u << MAX98357A_DMA_RING_BITS),
               "DMA read ring must exactly cover one audio buffer");
#if AUDIO_I2S_SLOT_BITS != 16
#error "audio_i2s.pio and the PCM buffer layout require 16-bit channel slots"
#endif

static PIO s_amp_pio = NULL;
static uint s_amp_sm = 0;
static uint s_amp_offset = 0;
static uint s_amp_pin_din = 0;
static uint s_amp_pin_bclk = 0;
static uint s_amp_pin_lrc = 0;

static int s_dma_chan_a = -1;
static int s_dma_chan_b = -1;

_Alignas(1u << MAX98357A_DMA_RING_BITS) static uint32_t s_buf_a[MAX98357A_BUFFER_WORDS];
_Alignas(1u << MAX98357A_DMA_RING_BITS) static uint32_t s_buf_b[MAX98357A_BUFFER_WORDS];

static volatile bool s_amp_sleeping = true;
static max98357a_refill_cb_t s_refill_cb = NULL;

static max98357a_stats_t s_stats = {0};
static volatile bool s_dma_debug_started = false;

static void refill_buffer_internal(int channel, int peer, uint32_t *buf) {
    if (dma_channel_is_busy(channel) ||
        (dma_channel_is_busy(peer) &&
         dma_channel_hw_addr(peer)->transfer_count < 8)) {
        s_stats.late_refills++;
        return;
    }
    if (s_refill_cb) {
        s_refill_cb(buf, MAX98357A_BUFFER_WORDS);
    }
    if (dma_channel_is_busy(channel)) {
        s_stats.late_refills++;
    }
}

static void max98357a_dma_irq_handler(void) {
    if (s_amp_sleeping) {
        dma_channel_acknowledge_irq0(s_dma_chan_a);
        dma_channel_acknowledge_irq0(s_dma_chan_b);
        return;
    }

    absolute_time_t refill_start = get_absolute_time();
    uint32_t stall_bit = 1u << (PIO_FDEBUG_TXSTALL_LSB + s_amp_sm);
    if (s_dma_debug_started && (s_amp_pio->fdebug & stall_bit)) {
        s_stats.tx_stall_observations++;
    }
    s_amp_pio->fdebug = stall_bit; // W1C
    s_dma_debug_started = true;

    if (dma_channel_get_irq0_status(s_dma_chan_a)) {
        dma_channel_acknowledge_irq0(s_dma_chan_a);
        refill_buffer_internal(s_dma_chan_a, s_dma_chan_b, s_buf_a);
    }
    if (dma_channel_get_irq0_status(s_dma_chan_b)) {
        dma_channel_acknowledge_irq0(s_dma_chan_b);
        refill_buffer_internal(s_dma_chan_b, s_dma_chan_a, s_buf_b);
    }

    if (!dma_channel_is_busy(s_dma_chan_a) && !dma_channel_is_busy(s_dma_chan_b)) {
        dma_channel_start(s_dma_chan_a);
        s_stats.dma_restarts++;
    }

    uint32_t elapsed = (uint32_t)absolute_time_diff_us(refill_start, get_absolute_time());
    if (elapsed > s_stats.max_refill_us) {
        s_stats.max_refill_us = elapsed;
    }
}

bool max98357a_driver_init(PIO pio, uint sm, uint pin_din, uint pin_bclk, uint pin_lrc,
                           uint sample_rate, max98357a_refill_cb_t refill_cb) {
    s_amp_pio = pio;
    s_amp_sm = sm;
    s_amp_pin_din = pin_din;
    s_amp_pin_bclk = pin_bclk;
    s_amp_pin_lrc = pin_lrc;
    s_refill_cb = refill_cb;

    s_amp_offset = pio_add_program(s_amp_pio, &audio_i2s_program);
    audio_i2s_program_init(s_amp_pio, s_amp_sm, s_amp_offset, s_amp_pin_din, s_amp_pin_bclk);

    float pio_clock = (float)(sample_rate * 32 * 2);
    float div = (float)clock_get_hz(clk_sys) / pio_clock;
    pio_sm_set_clkdiv(s_amp_pio, s_amp_sm, div);

    for (size_t i = 0; i < MAX98357A_BUFFER_WORDS; i++) {
        s_buf_a[i] = 0;
        s_buf_b[i] = 0;
    }

    s_dma_chan_a = dma_claim_unused_channel(true);
    s_dma_chan_b = dma_claim_unused_channel(true);
    if (s_dma_chan_a < 0 || s_dma_chan_b < 0) return false;

    // Ping-pong DMA channel A
    dma_channel_config c_a = dma_channel_get_default_config(s_dma_chan_a);
    channel_config_set_transfer_data_size(&c_a, DMA_SIZE_32);
    channel_config_set_read_increment(&c_a, true);
    channel_config_set_ring(&c_a, false, MAX98357A_DMA_RING_BITS);
    channel_config_set_write_increment(&c_a, false);
    channel_config_set_dreq(&c_a, pio_get_dreq(s_amp_pio, s_amp_sm, true));
    channel_config_set_chain_to(&c_a, s_dma_chan_b);
    dma_channel_configure(s_dma_chan_a, &c_a, &s_amp_pio->txf[s_amp_sm], s_buf_a, MAX98357A_BUFFER_WORDS, false);

    // Ping-pong DMA channel B
    dma_channel_config c_b = dma_channel_get_default_config(s_dma_chan_b);
    channel_config_set_transfer_data_size(&c_b, DMA_SIZE_32);
    channel_config_set_read_increment(&c_b, true);
    channel_config_set_ring(&c_b, false, MAX98357A_DMA_RING_BITS);
    channel_config_set_write_increment(&c_b, false);
    channel_config_set_dreq(&c_b, pio_get_dreq(s_amp_pio, s_amp_sm, true));
    channel_config_set_chain_to(&c_b, s_dma_chan_a);
    dma_channel_configure(s_dma_chan_b, &c_b, &s_amp_pio->txf[s_amp_sm], s_buf_b, MAX98357A_BUFFER_WORDS, false);

    dma_channel_set_irq0_enabled(s_dma_chan_a, true);
    dma_channel_set_irq0_enabled(s_dma_chan_b, true);
    irq_set_exclusive_handler(DMA_IRQ_0, max98357a_dma_irq_handler);
    irq_set_priority(DMA_IRQ_0, 0x40);
    irq_set_enabled(DMA_IRQ_0, true);

    max98357a_driver_sleep();
    return true;
}

void max98357a_driver_wake(void) {
    if (s_amp_sleeping && s_dma_chan_a >= 0 && s_dma_chan_b >= 0) {
        pio_sm_set_enabled(s_amp_pio, s_amp_sm, false);
        pio_sm_restart(s_amp_pio, s_amp_sm);
        pio_sm_clkdiv_restart(s_amp_pio, s_amp_sm);
        pio_sm_clear_fifos(s_amp_pio, s_amp_sm);
        pio_sm_exec(s_amp_pio, s_amp_sm, pio_encode_jmp(s_amp_offset + audio_i2s_offset_entry_point));

        dma_channel_set_read_addr(s_dma_chan_a, s_buf_a, false);
        dma_channel_set_trans_count(s_dma_chan_a, MAX98357A_BUFFER_WORDS, false);
        dma_channel_set_read_addr(s_dma_chan_b, s_buf_b, false);
        dma_channel_set_trans_count(s_dma_chan_b, MAX98357A_BUFFER_WORDS, false);

        dma_channel_acknowledge_irq0(s_dma_chan_a);
        dma_channel_acknowledge_irq0(s_dma_chan_b);
        hw_set_bits(&dma_channel_hw_addr(s_dma_chan_a)->al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
        hw_set_bits(&dma_channel_hw_addr(s_dma_chan_b)->al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
        dma_channel_set_irq0_enabled(s_dma_chan_a, true);
        dma_channel_set_irq0_enabled(s_dma_chan_b, true);
        s_amp_sleeping = false;
        s_dma_debug_started = false;
        dma_channel_start(s_dma_chan_a);
        pio_sm_set_enabled(s_amp_pio, s_amp_sm, true);
    }
}

void max98357a_driver_sleep(void) {
    uint32_t save = save_and_disable_interrupts();
    if (!s_amp_sleeping && s_dma_chan_a >= 0 && s_dma_chan_b >= 0) {
        pio_sm_set_enabled(s_amp_pio, s_amp_sm, false);
        dma_channel_set_irq0_enabled(s_dma_chan_a, false);
        dma_channel_set_irq0_enabled(s_dma_chan_b, false);
        // RP2350-E5: clear EN on BOTH chained channels before aborting either
        hw_clear_bits(&dma_channel_hw_addr(s_dma_chan_a)->ctrl_trig, DMA_CH0_CTRL_TRIG_EN_BITS);
        hw_clear_bits(&dma_channel_hw_addr(s_dma_chan_b)->ctrl_trig, DMA_CH0_CTRL_TRIG_EN_BITS);
        dma_channel_abort(s_dma_chan_a);
        dma_channel_abort(s_dma_chan_b);
        dma_channel_acknowledge_irq0(s_dma_chan_a);
        dma_channel_acknowledge_irq0(s_dma_chan_b);
        pio_sm_clear_fifos(s_amp_pio, s_amp_sm);
        uint32_t pins = (1u << s_amp_pin_bclk) | (1u << s_amp_pin_lrc) | (1u << s_amp_pin_din);
        pio_sm_set_pins_with_mask(s_amp_pio, s_amp_sm, 0, pins);
        s_amp_sleeping = true;
    }
    for (size_t i = 0; i < MAX98357A_BUFFER_WORDS; ++i) {
        s_buf_a[i] = s_buf_b[i] = 0;
    }
    restore_interrupts(save);
}

bool max98357a_driver_is_sleeping(void) {
    return s_amp_sleeping;
}

void max98357a_driver_get_stats(max98357a_stats_t *stats) {
    if (stats) {
        *stats = s_stats;
    }
}
