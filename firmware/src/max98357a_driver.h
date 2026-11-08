#ifndef CELESTIAL_MAX98357A_DRIVER_H
#define CELESTIAL_MAX98357A_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "hardware/pio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Self-contained MAX98357A Class-D I2S Audio Output Driver for RP2350.
 * Encapsulates PIO1 SM0, double-buffered Ping-Pong DMA, and hardware auto-sleep.
 */

#define MAX98357A_BUFFER_WORDS    512 // 32 ms @ 16 kHz
#define MAX98357A_DMA_RING_BITS   11  // 2048-byte ring (512 words * 4 bytes)

typedef void (*max98357a_refill_cb_t)(uint32_t *buf, size_t count);

typedef struct {
    uint32_t late_refills;
    uint32_t dma_restarts;
    uint32_t max_refill_us;
    uint32_t tx_stall_observations;
} max98357a_stats_t;

// Initialize PIO, clock divider, ping-pong DMA channels, and high-priority IRQ.
bool max98357a_driver_init(PIO pio, uint sm, uint pin_din, uint pin_bclk, uint pin_lrc,
                           uint sample_rate, max98357a_refill_cb_t refill_cb);

// Wake the amplifier, reset PIO state machine, and begin DMA transfers.
void max98357a_driver_wake(void);

// Stop DMA and PIO clocking to silence the amplifier and eliminate quiescent power drain.
void max98357a_driver_sleep(void);

// Returns true if the amplifier is currently sleeping.
bool max98357a_driver_is_sleeping(void);

// Retrieve low-level driver diagnostics.
void max98357a_driver_get_stats(max98357a_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif // CELESTIAL_MAX98357A_DRIVER_H

