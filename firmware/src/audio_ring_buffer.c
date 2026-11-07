#include "audio_ring_buffer.h"

bool audio_rb_init(audio_ring_buffer_t *rb,
                   int16_t *backing_storage,
                   uint32_t capacity_power_of_two) {
    if (rb == NULL || backing_storage == NULL || capacity_power_of_two < 2u) {
        return false;
    }
    if ((capacity_power_of_two & (capacity_power_of_two - 1u)) != 0u) {
        return false;
    }
    rb->storage         = backing_storage;
    rb->capacity        = capacity_power_of_two;
    rb->mask            = capacity_power_of_two - 1u;
    rb->write_idx       = 0u;
    rb->read_idx        = 0u;
    rb->overflow_count  = 0u;
    rb->underflow_count = 0u;
    return true;
}

void audio_rb_reset(audio_ring_buffer_t *rb) {
    if (rb == NULL) {
        return;
    }
    rb->write_idx       = 0u;
    rb->read_idx        = 0u;
    rb->overflow_count  = 0u;
    rb->underflow_count = 0u;
}

uint32_t audio_rb_available_read(const audio_ring_buffer_t *rb) {
    if (rb == NULL || rb->capacity == 0u) {
        return 0u;
    }
    return (uint32_t)(rb->write_idx - rb->read_idx);
}

uint32_t audio_rb_available_write(const audio_ring_buffer_t *rb) {
    if (rb == NULL || rb->capacity == 0u) {
        return 0u;
    }
    uint32_t used = (uint32_t)(rb->write_idx - rb->read_idx);
    if (used >= rb->capacity) {
        return 0u;
    }
    return rb->capacity - used;
}

bool audio_rb_push_samples(audio_ring_buffer_t *rb,
                           const int16_t *samples,
                           size_t count) {
    if (rb == NULL || rb->storage == NULL || rb->capacity == 0u ||
        samples == NULL || count == 0u) {
        return false;
    }
    uint32_t free_slots = audio_rb_available_write(rb);
    if (count > (size_t)free_slots) {
        rb->overflow_count++;
        return false;
    }

    uint32_t w = rb->write_idx;
    uint32_t mask = rb->mask;
    for (size_t i = 0; i < count; i++) {
        rb->storage[(w + (uint32_t)i) & mask] = samples[i];
    }
    rb->write_idx = w + (uint32_t)count;
    return true;
}

bool audio_rb_pop_sample(audio_ring_buffer_t *rb, int16_t *out_sample) {
    if (rb == NULL || rb->storage == NULL || rb->capacity == 0u || out_sample == NULL) {
        return false;
    }
    uint32_t r = rb->read_idx;
    if (r == rb->write_idx) {
        rb->underflow_count++;
        *out_sample = 0;
        return false;
    }
    *out_sample = rb->storage[r & rb->mask];
    rb->read_idx = r + 1u;
    return true;
}

uint32_t audio_rb_get_overflow_count(const audio_ring_buffer_t *rb) {
    return (rb != NULL) ? rb->overflow_count : 0u;
}

uint32_t audio_rb_get_underflow_count(const audio_ring_buffer_t *rb) {
    return (rb != NULL) ? rb->underflow_count : 0u;
}
