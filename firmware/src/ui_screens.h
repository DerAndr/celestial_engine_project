#ifndef CELESTIAL_UI_SCREENS_H
#define CELESTIAL_UI_SCREENS_H

#include <stdint.h>
#include <stdbool.h>
#include "sh1106.h"
#include "staff_state.h"
#include "net_client.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MODE_STATUS = 0,    // Main utilitarian status with datetime
    MODE_ARCHITECTURE,  // Dynamic 2-Node Architecture diagram
    MODE_COSMOS,        // Unified Cosmos: Galaxy -> Solar System -> Terra
    MODE_CLOCK,         // Celestial Chronometer / Astronomical Clock
    MODE_AI_MODEL,      // Target Cloud LLM Core selector
    MODE_LANGUAGE,      // Language dialect selector
    MODE_VOLUME,        // Master volume
    MODE_LED_CORE,      // 3-ring LED animation selector
    MODE_MIC_TEST,      // Mic live VU meter
    MODE_CHIPTUNE,      // 8-bit chiptune jukebox
    MODE_COSMIC,        // Cosmic Sci-Fi & Astromech SFX Demos
    MODE_VOICE,         // Built-in speech phrases
    MODE_SYSTEM,        // Hardware diagnostics
    MODE_COUNT
} staff_mode_t;

extern const char *MODE_NAMES[MODE_COUNT];

typedef enum {
    COSMOS_ZOOM_GALAXY = 0,
    COSMOS_ZOOM_SOLAR = 1,
    COSMOS_ZOOM_TERRA = 2,
    COSMOS_ZOOM_COUNT = 3
} cosmos_zoom_t;

typedef enum {
    CLOCK_STYLE_DIAL = 0,       // 0: Directional astronomical dial with hands & second comet
    CLOCK_STYLE_HOURGLASS = 1,  // 1: Orientation-independent concentric ring accumulator
    CLOCK_STYLE_PRISMATIC = 2,  // 2: Orientation-independent full spectral color-encoded clock
    CLOCK_STYLE_ORRERY = 3,     // 3: Orientation-independent kinematic concentric triple orbital clock
    CLOCK_STYLE_COUNT
} clock_style_t;

#define CLOCK_MENU_ITEMS 5

extern const char *CLOCK_STYLE_NAMES[CLOCK_STYLE_COUNT];
extern const char *CLOCK_STYLE_SHORT[CLOCK_STYLE_COUNT];

extern const char *LANG_NAMES[LANG_COUNT];
extern const char *LANG_SHORT_TAGS[LANG_COUNT];

// OLED readiness flag
void ui_set_oled_ready(bool ready);
bool ui_is_oled_ready(void);

// Offline Oracle Mode
void ui_set_offline_oracle_mode(bool offline);
bool ui_is_offline_oracle_mode(void);
void ui_trigger_offline_oracle(int lang_idx);

// Clock time getters and setters
void ui_get_current_time(uint8_t *hours, uint8_t *minutes, uint8_t *seconds, uint16_t *millis);
void ui_set_current_time_epoch(uint32_t epoch_seconds, uint16_t millis);
void set_current_time_epoch(uint32_t epoch_seconds, uint16_t millis);
void ui_adjust_clock_time(int delta_hours, int delta_minutes);
uint32_t ui_get_clock_base_seconds(void);
void ui_set_clock_base_seconds(uint32_t base_sec);

// Standalone Modal Screens
void ui_draw_67_screen(bool flash_phase);
void ui_draw_listening_screen(uint32_t hold_ms, int active_confirmed_mode, bool flash_phase);
void ui_draw_speaking_screen(uint32_t dur_ms, bool flash_phase);
void ui_draw_processing_screen(uint32_t elapsed_ms, bool flash_phase);
void ui_draw_not_ready_screen(void);
void ui_draw_ai_model_screen(int sel_model, int act_model);
void ui_draw_language_screen(int sel_lang, int act_lang);
void ui_draw_volume_screen(uint8_t vol_percent);
void ui_draw_mic_test_screen(uint8_t level_pct, int16_t peak_val);
void ui_draw_led_screen(int anim_idx);
void ui_draw_led_color_screen(int color_idx);
void ui_draw_chiptune_screen(int sel_idx, int act_trk, bool is_playing);
void ui_draw_voice_screen(int phrase_idx, bool is_playing);
void ui_draw_cosmic_screen(int sfx_idx, bool is_playing);

// Dashboard Context
typedef struct {
    staff_state_t current_state;
    int current_mode_idx;
    int32_t encoder_pos;
    bool encoder_sw_down;
    bool click_feedback;
    bool query_btn_held;
    int active_lang_idx;
    int active_ai_model_idx;
    int active_clock_style;
    int selected_clock_style;
    bool clock_style_edit_mode;
    bool clock_time_edit_mode;
    int clock_time_edit_phase;
    int cosmos_zoom_level;
    int active_anim_idx;
    int active_track_idx;
    int active_voice_idx;
    int active_cosmic_idx;
} ui_dashboard_ctx_t;

void ui_draw_dashboard(const ui_dashboard_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* CELESTIAL_UI_SCREENS_H */
