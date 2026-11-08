#ifndef CELESTIAL_EVENT_DISPATCHER_H
#define CELESTIAL_EVENT_DISPATCHER_H

#include <stdint.h>
#include <stdbool.h>
#include "staff_state.h"
#include "ui_screens.h"

#ifdef __cplusplus
extern "C" {
#endif

// System state and active modes
extern staff_state_t current_state;
extern int current_mode_idx;
extern int active_confirmed_mode;

// Sub-mode / edit flags
extern bool volume_edit_mode;
extern bool mic_test_mode;
extern bool led_edit_mode;
extern bool led_color_edit_mode;
extern bool chiptune_mode;
extern bool voice_mode;
extern bool cosmic_mode;
extern bool ai_model_edit_mode;
extern bool lang_edit_mode;
extern bool clock_style_edit_mode;
extern bool clock_time_edit_mode;
extern int clock_time_edit_phase;
extern bool easter_egg_active;

// Indices
extern int active_ai_model_idx;
extern int selected_ai_model_idx;
extern int active_lang_idx;
extern int selected_lang_idx;
extern int active_clock_style;
extern int selected_clock_style;
extern int cosmos_zoom_level;
extern int active_anim_idx;
extern int live_rendered_anim;
extern int active_track_idx;
extern int selected_music_idx;
extern int active_voice_idx;
extern int active_cosmic_idx;

// Readiness flags
extern bool cyw43_ready;
extern bool oled_ready;

// Lifecycle
void event_dispatcher_init(void);
void event_dispatcher_tick(void);

// Helper routines
void sync_chiptune_thematic_leds(int track_idx);
void broadcast_telemetry(void);

#ifdef __cplusplus
}
#endif

#endif /* CELESTIAL_EVENT_DISPATCHER_H */

