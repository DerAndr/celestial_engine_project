#ifndef CELESTIAL_ENCODER_H
#define CELESTIAL_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize GPIOs for Bourns PEC11R rotary encoder:
 * GP6 (Channel A), GP7 (Channel B), GP8 (Push Switch).
 * Configures all pins as inputs with internal pull-ups enabled.
 */
void encoder_init(void);

/**
 * Update encoder state machine.
 * Should be called frequently in the main loop (e.g. every 1-5 ms).
 */
void encoder_update(void);

/**
 * Get cumulative rotation delta (+1 for CW detent, -1 for CCW detent).
 * Returns net steps since last call and clears delta.
 */
int32_t encoder_get_delta(void);

/**
 * Get absolute position counter.
 */
int32_t encoder_get_position(void);

/**
 * Check if the encoder switch was clicked (debounced falling edge).
 * Clears the clicked flag upon reading.
 */
bool encoder_switch_clicked(void);

/**
 * Check current raw/debounced state of the push switch.
 * Returns true if held down, false if released.
 */
bool encoder_switch_is_down(void);

/**
 * Read raw GPIO states of A, B, and SW pins for live screen diagnostics.
 */
void encoder_get_raw_pins(bool *a, bool *b, bool *sw);

#ifdef __cplusplus
}
#endif

#endif /* CELESTIAL_ENCODER_H */
