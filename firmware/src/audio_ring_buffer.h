#ifndef CELESTIAL_AUDIO_RING_BUFFER_H
#define CELESTIAL_AUDIO_RING_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Lock-safe power-of-two 16-bit PCM FIFO ring buffer for RP2350 audio streaming.
 * Uses monotonically wrapping uint32_t read/write indices so full capacity is usable
 * and single-producer / single-consumer operations are deterministic.
 */
typedef struct {
    int16_t          *storage;
    uint32_t          capacity;
    uint32_t          mask;
    volatile uint32_t write_idx;
    volatile uint32_t read_idx;
    volatile uint32_t overflow_count;
    volatile uint32_t underflow_count;
} audio_ring_buffer_t;

/**
 * Initialize a ring buffer with caller-provided backing storage.
 * capacity_power_of_two must be a power of 2 >= 2.
 */
bool audio_rb_init(audio_ring_buffer_t *rb,
                   int16_t *backing_storage,
                   uint32_t capacity_power_of_two);

/**
 * Reset read/write indices and clear underflow/overflow diagnostic counters.
 */
void audio_rb_reset(audio_ring_buffer_t *rb);

/**
 * Return the number of samples currently queued and available to read.
 */
uint32_t audio_rb_available_read(const audio_ring_buffer_t *rb);

/**
 * Return the number of free sample slots available for writing.
 */
uint32_t audio_rb_available_write(const audio_ring_buffer_t *rb);

/**
 * Push `count` 16-bit PCM samples atomically (all-or-nothing).
 * If insufficient space is available, increments overflow_count and returns false
 * without overwriting unread samples.
 */
bool audio_rb_push_samples(audio_ring_buffer_t *rb,
                           const int16_t *samples,
                           size_t count);

/**
 * Pop a single 16-bit PCM sample from the FIFO.
 * If the buffer is empty, increments underflow_count, sets *out_sample = 0,
 * and returns false.
 */
bool audio_rb_pop_sample(audio_ring_buffer_t *rb, int16_t *out_sample);

/**
 * Return cumulative overflow and underflow counts since last reset.
 */
uint32_t audio_rb_get_overflow_count(const audio_ring_buffer_t *rb);
uint32_t audio_rb_get_underflow_count(const audio_ring_buffer_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* CELESTIAL_AUDIO_RING_BUFFER_H */
