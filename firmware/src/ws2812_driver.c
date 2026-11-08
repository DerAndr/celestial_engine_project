#include "ws2812_driver.h"
#include "hardware/dma.h"
#include "pico/time.h"
#include "ws2812.pio.h"

#define WS2812_RESET_US 300 // Standard WS2812B latch time >= 300 microseconds

static PIO s_ws_pio = NULL;
static uint s_ws_sm = 0;
static uint s_ws_offset = 0;
static int s_ws_dma_chan = -1;
static dma_channel_config s_ws_dma_config;
static bool s_ws_initialized = false;

bool ws2812_driver_init(PIO pio, uint sm, uint pin, float freq_hz) {
    if (s_ws_initialized) return true;

    s_ws_pio = pio;
    s_ws_sm = sm;
    s_ws_offset = pio_add_program(s_ws_pio, &ws2812_program);
    ws2812_program_init(s_ws_pio, s_ws_sm, s_ws_offset, pin, freq_hz, false);

    s_ws_dma_chan = dma_claim_unused_channel(true);
    if (s_ws_dma_chan < 0) return false;

    s_ws_dma_config = dma_channel_get_default_config(s_ws_dma_chan);
    channel_config_set_transfer_data_size(&s_ws_dma_config, DMA_SIZE_32);
    channel_config_set_read_increment(&s_ws_dma_config, true);
    channel_config_set_write_increment(&s_ws_dma_config, false);
    channel_config_set_dreq(&s_ws_dma_config, pio_get_dreq(s_ws_pio, s_ws_sm, true));
    channel_config_set_high_priority(&s_ws_dma_config, true);

    s_ws_initialized = true;
    return true;
}

void ws2812_driver_transmit_blocking(const uint32_t *grb_words_left_aligned, size_t count) {
    if (!s_ws_initialized || !grb_words_left_aligned || count == 0) return;

    __compiler_memory_barrier();
    dma_channel_configure(s_ws_dma_chan, &s_ws_dma_config,
                          &s_ws_pio->txf[s_ws_sm], grb_words_left_aligned, (uint)count, true);
    dma_channel_wait_for_finish_blocking(s_ws_dma_chan);

    // FIFO empty can still leave one 24-bit pixel in the output shifter.
    while (!pio_sm_is_tx_fifo_empty(s_ws_pio, s_ws_sm)) {
        tight_loop_contents();
    }
    sleep_us(35 + WS2812_RESET_US); // Finish the last 30us word, then a >=300us low latch.
}

bool ws2812_driver_is_busy(void) {
    if (!s_ws_initialized || s_ws_dma_chan < 0) return false;
    return dma_channel_is_busy(s_ws_dma_chan) || !pio_sm_is_tx_fifo_empty(s_ws_pio, s_ws_sm);
}
