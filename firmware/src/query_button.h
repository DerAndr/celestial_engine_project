#ifndef CELESTIAL_QUERY_BUTTON_H
#define CELESTIAL_QUERY_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize GP9 (Pico Pin 12) as QUERY input with internal pull-up.
 */
void query_button_init(void);

/**
 * Fast sampling & debounce update. Call in main loop.
 */
void query_button_update(void);

/**
 * Check if QUERY button is currently held down.
 */
bool query_button_is_held(void);

/**
 * Returns true once on transition from released -> pressed.
 */
bool query_button_just_pressed(void);

/**
 * Returns held duration in milliseconds once on transition from pressed -> released.
 * Returns 0 if no release occurred this cycle.
 */
uint32_t query_button_just_released(void);

/**
 * Current continuous hold duration in milliseconds while held.
 */
uint32_t query_button_current_hold_ms(void);

/**
 * Read raw GPIO state (1=open/HIGH, 0=pressed/LOW).
 */
bool query_button_get_raw(void);

/**
 * Simulation hooks for serial console debugging.
 */
void query_button_simulate_press(void);
void query_button_simulate_release(uint32_t duration_ms);

#ifdef __cplusplus
}
#endif

#endif /* CELESTIAL_QUERY_BUTTON_H */
