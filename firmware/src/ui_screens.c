#include "ui_screens.h"
#include "audio.h"
#include "voice_data.h"
#include "mic.h"
#include "leds.h"
#include "net_client.h"
#include "config.h"
#include "pico/time.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#if __has_include("wifi_credentials.h")
    #include "wifi_credentials.h"
#endif
#ifndef BRAIN_SERVER_IP
    #define BRAIN_SERVER_IP "127.0.0.1"
#endif

const char *MODE_NAMES[MODE_COUNT] = {
    "GUIDE",
    "ARCHITECTURE",
    "COSMOS",
    "CHRONOS",
    "AI CORE",
    "LINGUA",
    "VOLUME",
    "RUNE LEDS",
    "MIC VU",
    "JUKEBOX",
    "COSMIC FX",
    "REPLICAS",
    "SYSTEM"
};

const char *CLOCK_STYLE_NAMES[CLOCK_STYLE_COUNT] = {
    "DIAL CHRONOS",
    "AETHER HOURGLASS",
    "CHROMA PRISMATIC",
    "CELESTIAL ORRERY"
};

const char *CLOCK_STYLE_SHORT[CLOCK_STYLE_COUNT] = {
    "DIAL",
    "HOURGLASS",
    "CHROMA",
    "ORRERY"
};

const char *LANG_NAMES[LANG_COUNT] = {
    "ENGLISH [EN]",
    "AUTO-DETECT [AUTO]",
    "DEUTSCH [DE]",
    "RUSSIAN [RU]"
};

const char *LANG_SHORT_TAGS[LANG_COUNT] = {
    "EN",
    "AUT",
    "DE",
    "RU"
};

static bool s_ui_oled_ready = true;

void ui_set_oled_ready(bool ready) {
    s_ui_oled_ready = ready;
}

bool ui_is_oled_ready(void) {
    return s_ui_oled_ready;
}

static uint32_t s_ui_clock_base_seconds = (19 * 3600 + 20 * 60 + 14);
static uint32_t s_ui_clock_boot_ms = 0;

void ui_get_current_time(uint8_t *hours, uint8_t *minutes, uint8_t *seconds, uint16_t *millis) {
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    uint32_t elapsed_s = (now_ms - s_ui_clock_boot_ms) / 1000;
    uint32_t total_s = (s_ui_clock_base_seconds + elapsed_s) % 86400;
    *hours = (uint8_t)((total_s / 3600) % 24);
    *minutes = (uint8_t)((total_s / 60) % 60);
    *seconds = (uint8_t)(total_s % 60);
    *millis = (uint16_t)((now_ms - s_ui_clock_boot_ms) % 1000);
}

void ui_set_current_time_epoch(uint32_t epoch_seconds, uint16_t millis) {
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    s_ui_clock_boot_ms = now_ms - (millis % 1000);
    s_ui_clock_base_seconds = (epoch_seconds % 86400);
}

void set_current_time_epoch(uint32_t epoch_seconds, uint16_t millis) {
    ui_set_current_time_epoch(epoch_seconds, millis);
}

void ui_adjust_clock_time(int delta_hours, int delta_minutes) {
    int32_t total = (int32_t)s_ui_clock_base_seconds + delta_hours * 3600 + delta_minutes * 60;
    while (total < 0) total += 86400;
    s_ui_clock_base_seconds = (uint32_t)(total % 86400);
}

uint32_t ui_get_clock_base_seconds(void) {
    return s_ui_clock_base_seconds;
}

void ui_set_clock_base_seconds(uint32_t base_sec) {
    s_ui_clock_base_seconds = base_sec % 86400;
}

#define get_current_time ui_get_current_time

static ui_dashboard_ctx_t s_default_ui_ctx = {
    .current_state = STATE_IDLE,
    .current_mode_idx = 0,
    .encoder_pos = 0,
    .encoder_sw_down = false,
    .click_feedback = false,
    .query_btn_held = false,
    .active_lang_idx = 0,
    .active_ai_model_idx = 0,
    .active_clock_style = 1,
    .selected_clock_style = 1,
    .clock_style_edit_mode = false,
    .clock_time_edit_mode = false,
    .clock_time_edit_phase = 0,
    .cosmos_zoom_level = 0,
    .active_anim_idx = 0,
    .active_track_idx = 0,
    .active_voice_idx = 0,
    .active_cosmic_idx = 0
};
static const ui_dashboard_ctx_t *s_ui_ctx = &s_default_ui_ctx;

#define active_lang_idx         (s_ui_ctx->active_lang_idx)
#define active_ai_model_idx     (s_ui_ctx->active_ai_model_idx)
#define active_clock_style      (s_ui_ctx->active_clock_style)
#define selected_clock_style    (s_ui_ctx->selected_clock_style)
#define clock_style_edit_mode   (s_ui_ctx->clock_style_edit_mode)
#define clock_time_edit_mode    (s_ui_ctx->clock_time_edit_mode)
#define clock_time_edit_phase   (s_ui_ctx->clock_time_edit_phase)
#define cosmos_zoom_level       (s_ui_ctx->cosmos_zoom_level)
#define active_anim_idx         (s_ui_ctx->active_anim_idx)
#define active_track_idx        (s_ui_ctx->active_track_idx)
#define active_voice_idx        (s_ui_ctx->active_voice_idx)
#define active_cosmic_idx       (s_ui_ctx->active_cosmic_idx)
#define query_button_is_held()  (s_ui_ctx->query_btn_held)
#define current_state           (s_ui_ctx->current_state)

void ui_draw_67_screen(bool flash_phase) {
    if (!s_ui_oled_ready) return;

    sh1106_clear();

    if (flash_phase) {
        sh1106_fill_rect(0, 0, 128, 64, true);
        sh1106_draw_string(14, 6, "=== CELESTIAL ===", false);
        sh1106_draw_string_scaled(4, 22, "!!! 67 !!!", 2, false);
        sh1106_draw_string(16, 44, "* SPECIAL UNLOCK *", false);
        sh1106_draw_string(18, 54, "PRESS SW TO EXIT", false);
    } else {
        sh1106_draw_rect(0, 0, 128, 64, true);
        sh1106_draw_rect(2, 2, 124, 60, true);
        sh1106_draw_string(14, 6, "=== CELESTIAL ===", true);
        sh1106_draw_string_scaled(4, 22, "!!! 67 !!!", 2, true);
        sh1106_draw_string(16, 44, "* SPECIAL UNLOCK *", true);
        sh1106_draw_string(18, 54, "PRESS SW TO EXIT", true);
    }

    s_ui_oled_ready = sh1106_update();
}

static void draw_marquee(int y, const char *text, uint32_t ticker_ms);

static bool s_ui_offline_oracle_mode = false;
static char offline_oracle_prophecy[256] = "";

void ui_set_offline_oracle_mode(bool offline) {
    s_ui_offline_oracle_mode = offline;
}

bool ui_is_offline_oracle_mode(void) {
    return s_ui_offline_oracle_mode;
}

static const char* const OFFLINE_PROPHECIES_EN[] = {
    "THE GUIDE SAYS: DON'T PANIC! THE ANSWER TO LIFE AND EVERYTHING IS 42.",
    "CELESTIAL SPHERE ONLINE: GRAVITSAPPA CHARGED AT MAXIMUM LUX!",
    "STELLAR SENSORS CLEAR: NO VOGON CONSTRUCTOR FLEETS IN THIS SECTOR.",
    "TOWEL IN PLACE: AETHERIC RELAY CALIBRATED AND READY FOR VOYAGE.",
    "COSMIC ANOMALY HARMONIZED: ALL RUNIC FOCUS RINGS GLOW BRIGHT.",
    "ORACLE SIGHT: PROBABILITY MATRIX BALANCED. KU! DON'T PANIC!"
};

static const char* const OFFLINE_PROPHECIES_DE[] = {
    "DER REISEFUEHRER SAGT: KEINE PANIK! DIE ANTWORT LAUTET 42.",
    "HIMMELSSPHAERE BEREIT: GRAVITSAPPA AUF MAXIMUM GELADEN!",
    "KEINE VOGONISCHE BAUFLOTTE IN SICHT. HANDTUCH BEREITHALTEN.",
    "KOSMISCHE ANOMALIE ERKANNT UND HARMONISIERT. RUNEN AKTIV.",
    "ORAKEL VERKUENDE: SUB-ETHA MATRIX STABILISIERT. KU! 42!"
};

static const char* const OFFLINE_PROPHECIES_RU[] = {
    "PUTEVODITEL: NE PANIKUY! OTVET NA GLAVNYI VOPROS BYTIYA -- 42.",
    "NEBESNAYA SFERA V SETI: GRAVITSAPPA ZARYAZHENA, TSAK MIGAYET. KU!",
    "SISTEMA GOTOVA: PEPELATS SMAZAN, LUTS YEST, SVYAZ SO ZVEZDAMI 100%.",
    "OBNARUZHENA ANOMALIYA: VEROYATNOSTNYI DVIGATEL STABILIZIROVAN.",
    "SPOKOYSTVIE I TOLKO SPOKOYSTVIE: KOSMOS V POLNOM PORYADKE.",
    "ORAKUL VESHCHAYET: ZVEZDY BLAGOPRIYATSTVUYUT PUTESHESTVIYU. KU!"
};

void ui_trigger_offline_oracle(int lang_idx) {
    s_ui_offline_oracle_mode = true;
    uint32_t seed = to_ms_since_boot(get_absolute_time());

    int phrase_roll = (int)(seed % 4);
    voice_phrase_id_t phrase_id = VOICE_ORACLE;
    if (phrase_roll == 0) phrase_id = VOICE_ONLINE;
    else if (phrase_roll == 1) phrase_id = VOICE_READY;
    else if (phrase_roll == 2) phrase_id = VOICE_WARNING;
    else phrase_id = VOICE_ORACLE;

    audio_play_voice(phrase_id);

    if (lang_idx == LANG_EN) {
        size_t count = sizeof(OFFLINE_PROPHECIES_EN) / sizeof(OFFLINE_PROPHECIES_EN[0]);
        snprintf(offline_oracle_prophecy, sizeof(offline_oracle_prophecy), "%s", OFFLINE_PROPHECIES_EN[seed % count]);
    } else if (lang_idx == LANG_DE) {
        size_t count = sizeof(OFFLINE_PROPHECIES_DE) / sizeof(OFFLINE_PROPHECIES_DE[0]);
        snprintf(offline_oracle_prophecy, sizeof(offline_oracle_prophecy), "%s", OFFLINE_PROPHECIES_DE[seed % count]);
    } else {
        size_t count = sizeof(OFFLINE_PROPHECIES_RU) / sizeof(OFFLINE_PROPHECIES_RU[0]);
        snprintf(offline_oracle_prophecy, sizeof(offline_oracle_prophecy), "%s", OFFLINE_PROPHECIES_RU[seed % count]);
    }
}

void ui_draw_listening_screen(uint32_t hold_ms, int mode_idx, bool anim_blink) {
    (void)mode_idx;
    if (!s_ui_oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Header banner with clean fit
    sh1106_fill_rect(2, 2, 124, 12, true);
    char hdr_buf[32];
    if (s_ui_offline_oracle_mode) {
        snprintf(hdr_buf, sizeof(hdr_buf), "[%c] OFFLINE ORACLE [%s]",
                 anim_blink ? '*' : ' ',
                 LANG_SHORT_TAGS[active_lang_idx]);
    } else {
        snprintf(hdr_buf, sizeof(hdr_buf), "[%c] REC: LISTEN   [%s]",
                 anim_blink ? '*' : ' ',
                 LANG_SHORT_TAGS[active_lang_idx]);
    }
    sh1106_draw_string(6, 4, hdr_buf, false);

    if (s_ui_offline_oracle_mode) {
        sh1106_draw_string(6, 17, "SENSING AETHER...", true);
    } else {
        sh1106_draw_string(6, 17, "RECORDING VOICE...", true);
    }

    char timer_buf[20];
    snprintf(timer_buf, sizeof(timer_buf), "%lu.%02lu s",
             (unsigned long)(hold_ms / 1000),
             (unsigned long)((hold_ms % 1000) / 10));
    sh1106_draw_string_scaled(26, 28, timer_buf, 2, true);

    int mic_fill = (mic_get_level_percent() * 116) / 100;
    sh1106_draw_rect(6, 46, 116, 5, true);
    if (mic_fill > 0) {
        sh1106_fill_rect(6, 46, mic_fill, 5, true);
    }

    sh1106_draw_line(2, 53, 125, 53, true);
    if (s_ui_offline_oracle_mode) {
        sh1106_draw_string(6, 55, "RELEASE FOR PROPHECY", true);
    } else {
        sh1106_draw_string(6, 55, "RELEASE TO ASK GUIDE", true);
    }

    s_ui_oled_ready = sh1106_update();
}

void ui_draw_speaking_screen(uint32_t dur_ms, bool anim_blink) {
    (void)dur_ms;
    (void)anim_blink;
    if (!s_ui_oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    bool is_aud_active = audio_is_playing();

    // 1. Playback Header Banner
    sh1106_fill_rect(2, 2, 124, 12, true);
    if (s_ui_offline_oracle_mode) {
        sh1106_draw_string(6, 4, "[ OFFLINE ORACLE SPEAKS ]", false);
    } else if (is_aud_active) {
        sh1106_draw_string(8, 4, "[ THE GUIDE SPEAKS ]", false);
    } else {
        sh1106_draw_string(8, 4, "[ THE GUIDE TRANSMITS ]", false);
    }

    // 2. Oracle Response Marquee Box across screen
    const char *resp_text = s_ui_offline_oracle_mode ? offline_oracle_prophecy : net_client_get_response_text();
    if (resp_text == NULL || strlen(resp_text) == 0) {
        resp_text = "ORACLE BROADCASTING SPEECH...";
    }

    sh1106_draw_rect(4, 16, 120, 17, true);
    sh1106_draw_string(8, 18, "ORACLE:", true);
    draw_marquee(25, resp_text, now_ms);

    // 3. Animated Graphic Equalizer / Soundwave (active only when audio plays)
    if (is_aud_active) {
        for (int b = 0; b < 14; b++) {
            int h = 2 + (rand() % 8);
            sh1106_fill_rect(8 + b * 8, 46 - h, 5, h, true);
        }
    } else {
        // Calm steady transmission marker
        sh1106_draw_line(8, 43, 80, 43, true);
        sh1106_draw_string(8, 35, "[ PROPHECY ]", true);
    }
    char vol_tag[16];
    snprintf(vol_tag, sizeof(vol_tag), "V:%2d%%", (int)audio_get_volume());
    sh1106_draw_string(88, 38, vol_tag, true);

    // 4. Footer line
    sh1106_draw_line(2, 50, 125, 50, true);
    if (s_ui_offline_oracle_mode) {
        sh1106_draw_string(6, 53, "// STANDALONE ORACLE //", true);
    } else {
        sh1106_draw_string(6, 53, "// SHARE & ENJOY //", true);
    }

    s_ui_oled_ready = sh1106_update();
}


void ui_draw_not_ready_screen(void) {
    if (!s_ui_oled_ready) return;
    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);
    sh1106_fill_rect(2, 2, 124, 12, true);
    sh1106_draw_string(6, 4, "[! SUB-ETHA OFFLINE !]", false);

    if (!net_client_is_connected()) {
        sh1106_draw_string(6, 17, "WI-FI DISCONNECTED", true);
        sh1106_draw_string(6, 26, "CONNECTING TO ROUTER", true);
        sh1106_draw_line(2, 47, 125, 47, true);
        sh1106_draw_string(6, 51, "STATUS: DON'T PANIC...", true);
    } else {
        sh1106_draw_string(6, 17, "WI-FI: ONLINE (OK)", true);
        sh1106_draw_string(6, 26, "SUB-ETHA HOST UNREACHABLE", true);
        char host_buf[32];
        snprintf(host_buf, sizeof(host_buf), "HOST: %s:5005", BRAIN_SERVER_IP);
        sh1106_draw_string(6, 35, host_buf, true);
        sh1106_draw_line(2, 47, 125, 47, true);
        sh1106_draw_string(6, 51, "RUN: python ai_brain_server.py", true);
    }
    s_ui_oled_ready = sh1106_update();
}

void ui_draw_processing_screen(uint32_t elapsed_ms, bool anim_blink) {
    (void)anim_blink;
    if (!s_ui_oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Processing Header Banner
    sh1106_fill_rect(2, 2, 124, 12, true);
    char hdr_buf[32];
    snprintf(hdr_buf, sizeof(hdr_buf), "[ %-10.10s ]  [%s]",
             net_client_get_host_name(),
             net_client_get_model_tag(active_ai_model_idx));
    sh1106_draw_string(6, 4, hdr_buf, false);

    sh1106_draw_string(6, 17, "CONSULTING GUIDE...", true);

    char time_str[32];
    snprintf(time_str, sizeof(time_str), "THINKING: %lu.%01lu s",
             (unsigned long)(elapsed_ms / 1000),
             (unsigned long)((elapsed_ms % 1000) / 100));
    sh1106_draw_string(6, 27, time_str, true);

    int progress_step = (int)((elapsed_ms / 15) % 116);
    sh1106_draw_rect(6, 38, 116, 7, true);
    if (progress_step > 0) {
        sh1106_fill_rect(6, 38, progress_step, 7, true);
    }

    sh1106_draw_line(2, 49, 125, 49, true);
    sh1106_draw_string(6, 53, "HAVE A CUP OF TEA...", true);

    s_ui_oled_ready = sh1106_update();
}

void ui_draw_ai_model_screen(int sel_model, int act_model) {
    if (!s_ui_oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Header banner with host status (y=2..11)
    sh1106_fill_rect(2, 2, 124, 10, true);
    sh1106_draw_string(8, 3, "[ AI BRAIN & CORES ]", false);

    uint8_t count = net_client_get_model_count();
    if (count == 0) count = 1;
    net_host_type_t pref = net_client_get_preferred_host();
    net_host_type_t active_h = net_client_get_host_type();

    int total_items = 1 + count;
    int visible_rows = 4;
    int scroll_top = 0;
    if (sel_model >= visible_rows) {
        scroll_top = sel_model - (visible_rows - 1);
    }
    if (scroll_top > total_items - visible_rows && total_items > visible_rows) {
        scroll_top = total_items - visible_rows;
    }
    if (scroll_top < 0) scroll_top = 0;

    for (int r = 0; r < visible_rows; r++) {
        int item_idx = scroll_top + r;
        if (item_idx >= total_items) break;
        int y = 14 + r * 10;
        bool is_selected = (sel_model == item_idx);

        if (item_idx == 0) {
            // Row: Host Selector
            const char *pref_tag = (pref == HOST_TYPE_PI5) ? "< PI 5 >" :
                                   (pref == HOST_TYPE_PHONE) ? "< PHONE >" :
                                   "< AUTO >";
            char host_str[32];
            snprintf(host_str, sizeof(host_str), "HOST: %s (%s)", pref_tag,
                     active_h == HOST_TYPE_PI5 ? "PI5" : active_h == HOST_TYPE_PHONE ? "PHN" : "STB");
            if (is_selected) {
                sh1106_fill_rect(4, y - 1, 116, 9, true);
                sh1106_draw_string(6, y, host_str, false);
            } else {
                sh1106_draw_string(6, y, host_str, true);
            }
        } else {
            // Row: Model item
            int m_idx = item_idx - 1;
            bool is_active = (m_idx == act_model);
            const char *name = net_client_get_model_name(m_idx);
            char item_str[32];
            if (is_selected) {
                sh1106_fill_rect(4, y - 1, 116, 9, true);
                snprintf(item_str, sizeof(item_str), "> %s %s", name, is_active ? "*" : "");
                sh1106_draw_string(6, y, item_str, false);
            } else {
                snprintf(item_str, sizeof(item_str), "  %s %s", name, is_active ? "[ACTIVE]" : "");
                sh1106_draw_string(6, y, item_str, true);
            }
        }
    }

    // Vertical scrollbar track on right border (x: 122..124)
    if (total_items > visible_rows) {
        sh1106_draw_rect(122, 14, 3, 44, true);
        int track_h = 44 - 10;
        int thumb_y = 14 + (sel_model * track_h) / (total_items - 1);
        sh1106_fill_rect(122, thumb_y, 3, 10, true);
    }

    s_ui_oled_ready = sh1106_update();
}

void ui_draw_language_screen(int sel_lang, int act_lang) {
    if (!s_ui_oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Header banner
    sh1106_fill_rect(2, 2, 124, 12, true);
    sh1106_draw_string(10, 4, "[ VOX DIALECT / LANG ]", false);

    // List of 4 languages
    for (int i = 0; i < LANG_COUNT; i++) {
        int y = 16 + i * 9;
        bool is_selected = (i == sel_lang);
        bool is_active = (i == act_lang);

        if (is_selected) {
            sh1106_fill_rect(4, y - 1, 120, 9, true);
            char item_str[32];
            snprintf(item_str, sizeof(item_str), "> %s %s",
                     LANG_NAMES[i], is_active ? "*" : "");
            sh1106_draw_string(6, y, item_str, false);
        } else {
            char item_str[32];
            snprintf(item_str, sizeof(item_str), "  %s %s",
                     LANG_NAMES[i], is_active ? "[ACTIVE]" : "");
            sh1106_draw_string(6, y, item_str, true);
        }
    }

    // Footer divider and instruction
    sh1106_draw_line(2, 53, 125, 53, true);
    sh1106_draw_string(4, 55, "TURN:NAV  SW:CONFIRM", true);

    s_ui_oled_ready = sh1106_update();
}

void ui_draw_volume_screen(uint8_t vol_percent) {
    if (!s_ui_oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Header banner
    sh1106_fill_rect(2, 2, 124, 12, true);
    sh1106_draw_string(14, 4, "[ VOLUME SETTING ]", false);

    // Big volume percentage
    char vol_buf[16];
    snprintf(vol_buf, sizeof(vol_buf), "%3d %%", vol_percent);
    sh1106_draw_string_scaled(32, 19, vol_buf, 2, true);

    // Graphical Volume Gauge Bar: outer border x=14, y=38, w=100, h=10
    sh1106_draw_rect(14, 38, 100, 10, true);
    int fill_w = (vol_percent * 96) / 100;
    if (fill_w > 0) {
        sh1106_fill_rect(16, 40, fill_w, 6, true);
    }

    // Footer instruction
    sh1106_draw_line(2, 51, 125, 51, true);
    sh1106_draw_string(6, 54, "TURN: VOL  CLICK: OK", true);

    s_ui_oled_ready = sh1106_update();
}

void ui_draw_mic_test_screen(uint8_t level_pct, int16_t peak_val) {
    if (!s_ui_oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Header banner
    sh1106_fill_rect(2, 2, 124, 12, true);
    sh1106_draw_string(14, 4, "[ MIC LIVE TEST ]", false);

    // Live Metrics: Level % and Peak Amplitude
    char lvl_buf[24];
    snprintf(lvl_buf, sizeof(lvl_buf), "LVL:%3d%%  PK:%5d", level_pct, (int)peak_val);
    sh1106_draw_string(6, 17, lvl_buf, true);

    // Graphical VU Meter Bar (outer frame)
    sh1106_draw_rect(6, 28, 116, 11, true);
    // Inner fill bar
    int fill_w = (level_pct * 112) / 100;
    if (fill_w > 0) {
        sh1106_fill_rect(8, 30, fill_w, 7, true);
    }

    // Scale tick marks under VU meter
    sh1106_draw_string(6, 42, "0%  25%  50%  75%  MAX", true);

    // Footer instruction
    sh1106_draw_line(2, 51, 125, 51, true);
    sh1106_draw_string(4, 54, "SPEAK TO TEST | SW: OK", true);

    s_ui_oled_ready = sh1106_update();
}

void ui_draw_led_screen(int anim_idx) {
    if (!s_ui_oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Header banner
    sh1106_fill_rect(2, 2, 124, 12, true);
    sh1106_draw_string(14, 4, "[ LED EFFECTS ]", false);

    // Effect index
    char num_buf[24];
    snprintf(num_buf, sizeof(num_buf), "EFFECT %d / %d", anim_idx + 1, LED_ANIM_COUNT);
    sh1106_draw_string(20, 16, num_buf, true);

    // Effect name in bold
    sh1106_draw_string(10, 27, leds_get_anim_name(anim_idx), true);

    if (anim_idx == LED_ANIM_BREATHE) {
        char col_buf[32];
        snprintf(col_buf, sizeof(col_buf), "COLOR: [%s]", leds_get_rainbow_name(leds_get_rainbow_index()));
        sh1106_draw_string(10, 38, col_buf, true);
    } else {
        // Graphical progress bar under effect name
        sh1106_draw_rect(10, 40, 108, 5, true);
        int bar_fill = ((anim_idx + 1) * 104) / LED_ANIM_COUNT;
        if (bar_fill > 0) {
            sh1106_fill_rect(12, 41, bar_fill, 3, true);
        }
    }

    // Footer instruction
    sh1106_draw_line(2, 51, 125, 51, true);
    if (anim_idx == LED_ANIM_BREATHE) {
        sh1106_draw_string(4, 54, "TURN:EFF | SW:TUNE COLOR", true);
    } else {
        sh1106_draw_string(6, 54, "TURN: TUNE | SW: OK", true);
    }

    s_ui_oled_ready = sh1106_update();
}

void ui_draw_led_color_screen(int color_idx) {
    if (!s_ui_oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Header banner
    sh1106_fill_rect(2, 2, 124, 12, true);
    sh1106_draw_string(14, 4, "[ BREATHE COLOR ]", false);

    // Palette index
    char num_buf[24];
    snprintf(num_buf, sizeof(num_buf), "PALETTE %d / %d", color_idx + 1, leds_get_rainbow_count());
    sh1106_draw_string(20, 17, num_buf, true);

    // Color name in bold with arrows
    char name_buf[32];
    snprintf(name_buf, sizeof(name_buf), "<  %s  >", leds_get_rainbow_name(color_idx));
    sh1106_draw_string(14, 29, name_buf, true);

    // Graphical progress bar
    sh1106_draw_rect(10, 42, 108, 5, true);
    int bar_fill = ((color_idx + 1) * 104) / leds_get_rainbow_count();
    if (bar_fill > 0) {
        sh1106_fill_rect(12, 43, bar_fill, 3, true);
    }

    // Footer instruction
    sh1106_draw_line(2, 51, 125, 51, true);
    sh1106_draw_string(6, 54, "TURN: COLOR | SW: SAVE", true);

    s_ui_oled_ready = sh1106_update();
}

void ui_draw_chiptune_screen(int sel_idx, int act_trk, bool is_playing) {
    if (!s_ui_oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Header banner
    sh1106_fill_rect(2, 2, 124, 12, true);
    sh1106_draw_string(14, 4, "[ CELESTIAL MUSIC ]", false);

    int total_items = audio_get_chiptune_count() + 1; // Item 0 is MODE, 1..N are tracks
    const int visible_rows = 4;
    int scroll_offset = 0;
    if (sel_idx >= visible_rows) {
        scroll_offset = sel_idx - (visible_rows - 1);
    }
    if (scroll_offset > total_items - visible_rows) {
        scroll_offset = total_items - visible_rows;
    }
    if (scroll_offset < 0) scroll_offset = 0;

    for (int r = 0; r < visible_rows; r++) {
        int item_idx = scroll_offset + r;
        if (item_idx >= total_items) break;
        int y = 16 + r * 9;
        bool is_selected = (item_idx == sel_idx);

        if (item_idx == 0) {
            // Mode toggle row or Stop Playback row
            char mode_buf[32];
            if (is_playing) {
                snprintf(mode_buf, sizeof(mode_buf), "[ STOP PLAYBACK ]");
            } else {
                audio_play_mode_t cur_mode = audio_get_play_mode();
                const char *m_name = audio_get_play_mode_name(cur_mode);
                snprintf(mode_buf, sizeof(mode_buf), "MODE: < %s >", m_name);
            }
            if (is_selected) {
                sh1106_fill_rect(4, y - 1, 116, 9, true);
                sh1106_draw_string(6, y, mode_buf, false);
            } else {
                sh1106_draw_string(6, y, mode_buf, true);
            }
        } else {
            // Track item row
            int t_idx = item_idx - 1;
            bool is_active = (t_idx == act_trk) && is_playing;
            const char *t_name = audio_get_chiptune_name(t_idx);
            char trk_buf[32];
            if (is_selected) {
                sh1106_fill_rect(4, y - 1, 116, 9, true);
                snprintf(trk_buf, sizeof(trk_buf), "> %s %s", t_name, is_active ? "[STOP]" : "");
                sh1106_draw_string(6, y, trk_buf, false);
            } else {
                snprintf(trk_buf, sizeof(trk_buf), "  %s %s", t_name, is_active ? "[PLAY]" : "");
                sh1106_draw_string(6, y, trk_buf, true);
            }
        }
    }

    // Vertical scrollbar on right edge (x=122..124, y=14..52)
    if (total_items > visible_rows) {
        sh1106_draw_rect(122, 14, 3, 38, true);
        int track_h = 38 - 8;
        int thumb_y = 14 + (sel_idx * track_h) / (total_items - 1);
        sh1106_fill_rect(122, thumb_y, 3, 8, true);
    }

    // Footer divider and instructions
    sh1106_draw_line(2, 53, 125, 53, true);
    sh1106_draw_string(4, 55, "TURN:NAV  SW:ACT  QRY:EX", true);

    s_ui_oled_ready = sh1106_update();
}

void ui_draw_voice_screen(int phrase_idx, bool is_playing) {
    if (!s_ui_oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Header banner
    sh1106_fill_rect(2, 2, 124, 12, true);
    sh1106_draw_string(14, 4, "[ PRESET REPLICAS ]", false);

    // Phrase index & status
    char num_buf[48];
    snprintf(num_buf, sizeof(num_buf), "VOICE %d/%d  %s",
             phrase_idx + 1, audio_get_voice_count(),
             is_playing ? "[PLAYING]" : "[READY]");
    sh1106_draw_string(6, 18, num_buf, true);

    // Phrase Description
    sh1106_draw_string(6, 30, audio_get_voice_name((voice_phrase_id_t)phrase_idx), true);

    // Graphic soundwave bar
    for (int b = 0; b < 10; b++) {
        int bar_h = 2;
        if (is_playing) {
            bar_h = 3 + (rand() % 9);
        }
        sh1106_fill_rect(8 + b * 11, 48 - bar_h, 8, bar_h, true);
    }

    // Footer instruction
    sh1106_draw_line(2, 51, 125, 51, true);
    sh1106_draw_string(4, 54, "TURN:SEL SW:PLAY QRY:EXIT", true);

    s_ui_oled_ready = sh1106_update();
}

void ui_draw_cosmic_screen(int sfx_idx, bool is_playing) {
    if (!s_ui_oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Header banner
    sh1106_fill_rect(2, 2, 124, 12, true);
    sh1106_draw_string(14, 4, "[ COSMIC FX DEMO ]", false);

    // SFX index & status
    char num_buf[48];
    snprintf(num_buf, sizeof(num_buf), "SFX %d/%d  %s",
             sfx_idx + 1, audio_get_cosmic_sfx_count(),
             is_playing ? "[PLAYING]" : "[READY]");
    sh1106_draw_string(6, 17, num_buf, true);

    // SFX Track Name
    sh1106_draw_string(6, 28, audio_get_cosmic_sfx_name(sfx_idx), true);

    // Dynamic Sci-Fi frequency spectrum
    for (int b = 0; b < 10; b++) {
        int bar_h = 2;
        if (is_playing) {
            bar_h = 2 + (rand() % 9);
        }
        sh1106_fill_rect(8 + b * 11, 48 - bar_h, 8, bar_h, true);
    }

    // Footer instruction
    sh1106_draw_line(2, 51, 125, 51, true);
    sh1106_draw_string(4, 54, "TURN:SEL SW:PLAY QRY:EX", true);

    s_ui_oled_ready = sh1106_update();
}

static void draw_marquee(int y, const char *text, uint32_t ticker_ms) {
    if (text == NULL || !s_ui_oled_ready) return;
    size_t len = strlen(text);
    if (len == 0) return;

    int total_w = (int)len * 6;
    if (total_w <= 120) {
        int start_x = 4 + (120 - total_w) / 2;
        sh1106_draw_string(start_x, y, text, true);
        return;
    }

    int loop_gap = 24;
    int cycle_w = total_w + loop_gap;
    int scroll_px = (int)((ticker_ms / 15) % (uint32_t)cycle_w);

    for (int rep = 0; rep < 2; rep++) {
        int base_x = 4 - scroll_px + (rep * cycle_w);
        if (base_x > 124 || base_x + total_w < 0) continue;
        for (size_t i = 0; i < len; i++) {
            int cx = base_x + (int)(i * 6);
            if (cx >= 2 && cx <= 122) {
                sh1106_draw_char(cx, y, text[i], true);
            }
        }
    }
}

// 24x24 Techno-Fantasy Astrolabe & Runic Celestial Sigil
static const uint8_t CELESTIAL_LOGO_TECHNO_24x24[72] __attribute__((unused)) = {
    0x00, 0x18, 0x00,  // Row 0  - Northern Celestial Spire
    0x00, 0x18, 0x00,  // Row 1
    0x01, 0xFF, 0x80,  // Row 2  - Arcane Outer Ring Crown
    0x03, 0x18, 0xC0,  // Row 3
    0x0C, 0x18, 0x30,  // Row 4  - Concentric Dial Arc
    0x08, 0x7E, 0x10,  // Row 5  - Upper Runic Focus
    0x11, 0xDB, 0x88,  // Row 6  - Kinetic Energy Circuit
    0x33, 0x18, 0xCC,  // Row 7  - Quadrant Notches
    0x22, 0x18, 0x44,  // Row 8
    0x26, 0x18, 0x64,  // Row 9  - Inner Stargate Portal
    0x24, 0x3C, 0x24,  // Row 10 - Crystal Octahedron Top
    0xFF, 0xFF, 0xFF,  // Row 11 - Equatorial Cardinal Beam
    0xFF, 0xFF, 0xFF,  // Row 12 - Arc-Reactor Gravitsappa Core
    0x24, 0x3C, 0x24,  // Row 13 - Crystal Octahedron Bottom
    0x26, 0x18, 0x64,  // Row 14
    0x22, 0x18, 0x44,  // Row 15
    0x33, 0x18, 0xCC,  // Row 16 - Quadrant Notches
    0x11, 0xDB, 0x88,  // Row 17 - Kinetic Energy Circuit
    0x08, 0x7E, 0x10,  // Row 18 - Lower Runic Focus
    0x0C, 0x18, 0x30,  // Row 19 - Concentric Dial Arc
    0x03, 0x18, 0xC0,  // Row 20
    0x01, 0xFF, 0x80,  // Row 21 - Arcane Outer Ring Base
    0x00, 0x18, 0x00,  // Row 22
    0x00, 0x18, 0x00   // Row 23 - Southern Celestial Spire
};

// Milky Way Galaxy Star Map: Spiral arms with Sol (Orion Spur) location
static void draw_galaxy_card(uint32_t now_ms, int32_t rot_pos) {
    int cx = 38;
    int cy = 30;

    // Galactic center: Sagittarius A* core
    sh1106_fill_rect(cx - 2, cy - 2, 5, 5, true);
    sh1106_draw_pixel(cx - 3, cy, true);
    sh1106_draw_pixel(cx + 3, cy, true);
    sh1106_draw_pixel(cx, cy - 3, true);
    sh1106_draw_pixel(cx, cy + 3, true);

    // Dynamic rotation angle from time + encoder scrubbing
    float rot_ang = (float)now_ms * 0.0006f + (float)rot_pos * 0.05f;

    // 2 Spiral Arms (Perseus and Scutum-Centaurus)
    for (int arm = 0; arm < 2; arm++) {
        float arm_offset = (arm == 0) ? 0.0f : 3.14159f;
        for (float theta = 0.8f; theta < 4.8f; theta += 0.22f) {
            float r = 4.0f + 4.8f * theta;
            float a = theta + rot_ang + arm_offset;
            int px = cx + (int)(cosf(a) * r * 1.15f);
            int py = cy + (int)(sinf(a) * r * 0.52f);
            if (px >= 4 && px <= 74 && py >= 14 && py <= 46) {
                sh1106_draw_pixel(px, py, true);
            }
        }
    }

    // Orion Spur / Sol position (~26 kly from center)
    float sol_a = 1.35f + rot_ang;
    int sol_x = cx + (int)(cosf(sol_a) * 19.0f * 1.15f);
    int sol_y = cy + (int)(sinf(sol_a) * 19.0f * 0.52f);

    // Blinking target crosshair reticle on SOL
    if (((now_ms / 300) % 2) == 0) {
        if (sol_x >= 5 && sol_x <= 73 && sol_y >= 15 && sol_y <= 45) {
            sh1106_draw_pixel(sol_x, sol_y - 2, true);
            sh1106_draw_pixel(sol_x, sol_y + 2, true);
            sh1106_draw_pixel(sol_x - 2, sol_y, true);
            sh1106_draw_pixel(sol_x + 2, sol_y, true);
            sh1106_draw_pixel(sol_x, sol_y, true);
        }
    }

    // Divider
    sh1106_draw_line(77, 14, 77, 46, true);

    // Telemetry on right side (x = 80..125)
    sh1106_draw_string(80, 15, "MILKY WAY", true);
    sh1106_draw_string(80, 23, "ORION SPUR", true);
    sh1106_draw_string(80, 31, "R:26.4kly", true);
    if (((now_ms / 400) % 2) == 0) {
        sh1106_draw_string(80, 39, "SOL [HERE]", true);
    } else {
        sh1106_draw_string(80, 39, "YOU ARE HERE", true);
    }

    // Footer divider and marquee
    sh1106_draw_line(2, 47, 125, 47, true);
    draw_marquee(51, "// MILKY WAY GALAXY // 100,000 LY DIAMETER // SOL SYSTEM AT ORION SPUR // R: 26,400 LY TO SGR A* CORE //", now_ms);
}

// Solar System Orrery: Concentric planetary orbits with Earth targeted & coordinates
static void draw_solar_system_card(uint32_t now_ms, int32_t rot_pos) {
    // Center of planetary orbits
    int cx = 40;
    int cy = 30;

    // Time variable: advances with time and manual encoder scrubbing!
    float t = (float)now_ms * 0.0012f + (float)rot_pos * 0.08f;

    // 1. Concentric elliptical orbits (semi-major a, semi-minor b):
    // 0: Mercury (10, 4)
    // 1: Venus   (18, 7)
    // 2: Earth   (28, 11) -> TERRA
    // 3: Mars    (37, 15)
    const int orb_a[4] = { 10, 18, 28, 37 };
    const int orb_b[4] = {  4,  7, 11, 15 };

    for (int o = 0; o < 4; o++) {
        int a = orb_a[o];
        int b = orb_b[o];
        for (int deg = 0; deg < 360; deg += 12) {
            // Earth's orbit (o==2) is drawn denser so it's prominent
            if (o != 2 && (deg % 24 != 0)) continue;
            float rad = (float)deg * 0.0174533f;
            int ox = cx + (int)(cosf(rad) * (float)a);
            int oy = cy + (int)(sinf(rad) * (float)b);
            if (ox >= 4 && ox <= 77 && oy >= 14 && oy <= 46) {
                sh1106_draw_pixel(ox, oy, true);
            }
        }
    }

    // 2. The Sun (Sol) at center (cx, cy)
    int sun_pulse = (int)((now_ms / 200) % 2);
    sh1106_fill_rect(cx - 1, cy - 1, 3, 3, true);
    sh1106_draw_pixel(cx - 3 - sun_pulse, cy, true);
    sh1106_draw_pixel(cx + 3 + sun_pulse, cy, true);
    sh1106_draw_pixel(cx, cy - 3 - sun_pulse, true);
    sh1106_draw_pixel(cx, cy + 3 + sun_pulse, true);

    // 3. Planet positions & rendering (Keplerian speeds)
    float w_merc = t * 4.15f;
    float w_ven  = t * 1.62f;
    float w_ear  = t * 1.00f; // Earth: reference 1.0
    float w_mar  = t * 0.53f;

    // Mercury
    int mx = cx + (int)(cosf(w_merc) * (float)orb_a[0]);
    int my = cy + (int)(sinf(w_merc) * (float)orb_b[0]);
    if (mx >= 4 && mx <= 77 && my >= 14 && my <= 46) {
        sh1106_draw_pixel(mx, my, true);
    }

    // Venus
    int vx = cx + (int)(cosf(w_ven) * (float)orb_a[1]);
    int vy = cy + (int)(sinf(w_ven) * (float)orb_b[1]);
    if (vx >= 4 && vx <= 77 && vy >= 14 && vy <= 46) {
        sh1106_draw_pixel(vx, vy, true);
    }

    // Mars
    int rx = cx + (int)(cosf(w_mar) * (float)orb_a[3]);
    int ry = cy + (int)(sinf(w_mar) * (float)orb_b[3]);
    if (rx >= 4 && rx <= 77 && ry >= 14 && ry <= 46) {
        sh1106_draw_pixel(rx, ry, true);
    }

    // 4. EARTH (TERRA) - Focal Planet & Coordinates
    int ex = cx + (int)(cosf(w_ear) * (float)orb_a[2]);
    int ey = cy + (int)(sinf(w_ear) * (float)orb_b[2]);

    if (ex >= 4 && ex <= 77 && ey >= 14 && ey <= 46) {
        sh1106_fill_rect(ex - 1, ey - 1, 2, 2, true);

        // Moon orbiting Earth at 3.5 px radius
        float w_moon = t * 12.0f;
        int lx = ex + (int)(cosf(w_moon) * 3.5f);
        int ly = ey + (int)(sinf(w_moon) * 2.2f);
        if (lx >= 4 && lx <= 77 && ly >= 14 && ly <= 46) {
            sh1106_draw_pixel(lx, ly, true);
        }

        // 5. Dynamic Indicator Arrow pointing directly to Earth ("directional indicator arrows showing current presence")
        bool arrow_visible = ((now_ms / 250) % 3) != 0;
        if (arrow_visible) {
            if (ey >= cy) {
                // Earth is in lower half: arrow points UP to Earth
                int tip_x = ex;
                int tip_y = ey + 3;
                if (tip_y <= 45) {
                    sh1106_draw_line(tip_x, tip_y, tip_x, tip_y + 3, true);
                    sh1106_draw_pixel(tip_x - 1, tip_y + 1, true);
                    sh1106_draw_pixel(tip_x + 1, tip_y + 1, true);
                }
            } else {
                // Earth is in upper half: arrow points DOWN to Earth
                int tip_x = ex;
                int tip_y = ey - 3;
                if (tip_y >= 14) {
                    sh1106_draw_line(tip_x, tip_y, tip_x, tip_y - 3, true);
                    sh1106_draw_pixel(tip_x - 1, tip_y - 1, true);
                    sh1106_draw_pixel(tip_x + 1, tip_y - 1, true);
                }
            }
        }
    }

    // 6. HUD Telemetry Divider & Coordinates panel on right side (x = 78..125)
    sh1106_draw_line(78, 14, 78, 46, true);

    int earth_lon_deg = (int)(fmodf(w_ear * 57.2958f, 360.0f));
    if (earth_lon_deg < 0) earth_lon_deg += 360;

    sh1106_draw_string(82, 15, "TERRA", true);
    sh1106_draw_string(82, 23, "1.00AU", true);
    char lon_str[16];
    snprintf(lon_str, sizeof(lon_str), "L:%3d*", earth_lon_deg);
    sh1106_draw_string(82, 31, lon_str, true);

    // Blinking "YOU ARE HERE" tag
    if (((now_ms / 350) % 2) == 0) {
        sh1106_draw_string(82, 39, "HERE <", true);
    } else {
        sh1106_draw_string(82, 39, "SOL-3", true);
    }

    // 7. Footer Divider & Marquee Ticker
    sh1106_draw_line(2, 47, 125, 47, true);
    char solar_ticker[160];
    snprintf(solar_ticker, sizeof(solar_ticker),
             "SOLAR ORRERY * SOL-3 [TERRA] * DIST: 1.000 AU (149.6M KM) * ORBIT: 365.25d * LON: %d DEG * YOU ARE HERE *",
             earth_lon_deg);
    draw_marquee(51, solar_ticker, now_ms);
}

// Earth Planetary Zoom: Close-up globe with continents, atmosphere, and target reticle
static void draw_earth_card(uint32_t now_ms, int32_t rot_pos) {
    int cx = 38;
    int cy = 30;
    int r = 14;

    // Globe outline
    for (int deg = 0; deg < 360; deg += 10) {
        float rad = (float)deg * 0.0174533f;
        int px = cx + (int)(cosf(rad) * (float)r);
        int py = cy + (int)(sinf(rad) * (float)r);
        sh1106_draw_pixel(px, py, true);
    }

    // Equator and latitude lines
    for (int deg = 0; deg < 360; deg += 18) {
        float rad = (float)deg * 0.0174533f;
        int eq_x = cx + (int)(cosf(rad) * (float)r);
        int eq_y = cy + (int)(sinf(rad) * 4.0f);
        if (eq_x >= cx - r && eq_x <= cx + r && eq_y >= cy - r && eq_y <= cy + r) {
            sh1106_draw_pixel(eq_x, eq_y, true);
        }
    }

    // Rotating continents (landmass pixels)
    float t_rot = (float)now_ms * 0.0008f + (float)rot_pos * 0.06f;
    for (int p = 0; p < 8; p++) {
        float lon = t_rot + (float)p * 0.785f;
        float lat = ((p % 3) == 0) ? 0.35f : (((p % 2) == 0) ? -0.2f : 0.6f);
        if (cosf(lon) > 0.1f) {
            int lx = cx + (int)(sinf(lon) * (float)(r - 1) * cosf(lat));
            int ly = cy - (int)(sinf(lat) * (float)(r - 1));
            sh1106_fill_rect(lx - 1, ly - 1, 2, 2, true);
        }
    }

    // Moon orbiting at radius 21
    float m_ang = (float)now_ms * 0.0015f;
    int mx = cx + (int)(cosf(m_ang) * 21.0f);
    int my = cy + (int)(sinf(m_ang) * 9.0f);
    if (mx >= 4 && mx <= 74 && my >= 14 && my <= 46) {
        sh1106_draw_pixel(mx, my, true);
    }

    // Target crosshairs reticle on our location
    if (((now_ms / 300) % 2) == 0) {
        int tx = cx + 3;
        int ty = cy - 5;
        sh1106_draw_pixel(tx, ty - 2, true);
        sh1106_draw_pixel(tx, ty + 2, true);
        sh1106_draw_pixel(tx - 2, ty, true);
        sh1106_draw_pixel(tx + 2, ty, true);
    }

    // Divider
    sh1106_draw_line(77, 14, 77, 46, true);

    // Telemetry on right side
    sh1106_draw_string(80, 15, "PLANET", true);
    sh1106_draw_string(80, 23, "TERRA/EARTH", true);
    sh1106_draw_string(80, 31, "R: 6371km", true);
    if (((now_ms / 350) % 2) == 0) {
        sh1106_draw_string(80, 39, "HERE [x]", true);
    } else {
        sh1106_draw_string(80, 39, "CIVILIZATION", true);
    }

    // Footer marquee
    sh1106_draw_line(2, 47, 125, 47, true);
    draw_marquee(51, "// PLANET TERRA (SOL-3) // RADIUS: 6,371 KM // 1.0G GRAVITY // OXYGEN ATMOSPHERE // TARGET: YOU ARE HERE //", now_ms);
}

// Celestial Chronometer: Dual-Style Clock (Directional Dial vs. Orientation-Independent Hourglass)
static void draw_clock_card(uint32_t now_ms, int32_t rot_pos) {
    (void)rot_pos;
    uint8_t h, m, s;
    uint16_t ms;
    get_current_time(&h, &m, &s, &ms);

    int cx = 38;
    int cy = 30;

    if (active_clock_style == CLOCK_STYLE_DIAL) {
        // STYLE 0: Directional Astronomical Dial with 12 hour ticks and moving hands
        int r = 16;
        for (int deg = 0; deg < 360; deg += 15) {
            float rad = (float)deg * 0.0174533f;
            int px = cx + (int)(cosf(rad) * (float)r);
            int py = cy + (int)(sinf(rad) * (float)r);
            if (px >= 4 && px <= 74 && py >= 14 && py <= 46) {
                sh1106_draw_pixel(px, py, true);
            }
        }
        for (int tick_h = 0; tick_h < 12; tick_h++) {
            float angle = ((float)tick_h * 30.0f - 90.0f) * 0.0174533f;
            int len = (tick_h % 3 == 0) ? 3 : 2;
            int x1 = cx + (int)(cosf(angle) * (float)(r - len));
            int y1 = cy + (int)(sinf(angle) * (float)(r - len));
            int x2 = cx + (int)(cosf(angle) * (float)(r - 1));
            int y2 = cy + (int)(sinf(angle) * (float)(r - 1));
            sh1106_draw_line(x1, y1, x2, y2, true);
        }
        sh1106_fill_rect(cx - 1, cy - 1, 3, 3, true);

        // Hour Hand (~7.5 px)
        float h_angle = (((float)(h % 12) + (float)m / 60.0f) * 30.0f - 90.0f) * 0.0174533f;
        int hx = cx + (int)(cosf(h_angle) * 7.5f);
        int hy = cy + (int)(sinf(h_angle) * 7.5f);
        sh1106_draw_line(cx, cy, hx, hy, true);

        // Minute Hand (~12 px)
        float m_angle = (((float)m + (float)s / 60.0f) * 6.0f - 90.0f) * 0.0174533f;
        int mx = cx + (int)(cosf(m_angle) * 12.0f);
        int my = cy + (int)(sinf(m_angle) * 12.0f);
        sh1106_draw_line(cx, cy, mx, my, true);

        // Second Hand (~14 px)
        float s_angle = (((float)s + (float)ms / 1000.0f) * 6.0f - 90.0f) * 0.0174533f;
        int sx = cx + (int)(cosf(s_angle) * 14.0f);
        int sy = cy + (int)(sinf(s_angle) * 14.0f);
        int sx_tail = cx - (int)(cosf(s_angle) * 3.5f);
        int sy_tail = cy - (int)(sinf(s_angle) * 3.5f);
        sh1106_draw_line(sx_tail, sy_tail, sx, sy, true);
    } else if (active_clock_style == CLOCK_STYLE_HOURGLASS) {
        // STYLE 1: Orientation-Independent Concentric Liquid Arcs (Hourglass)
        sh1106_fill_rect(cx - 1, cy - 1, 3, 3, true);

        // Background concentric rings guide
        for (int deg = 0; deg < 360; deg += 30) {
            float rad = (float)deg * 0.0174533f;
            sh1106_draw_pixel(cx + (int)(cosf(rad) * 6.0f),  cy + (int)(sinf(rad) * 6.0f),  true);
            sh1106_draw_pixel(cx + (int)(cosf(rad) * 11.0f), cy + (int)(sinf(rad) * 11.0f), true);
            sh1106_draw_pixel(cx + (int)(cosf(rad) * 16.0f), cy + (int)(sinf(rad) * 16.0f), true);
        }

        // 1. Inner Ring (r=6): Seconds Decans Fill Arc (0..360 deg)
        int s_deg = (int)((((uint32_t)s * 1000 + ms) * 360) / 60000);
        for (int deg = 0; deg <= s_deg && deg < 360; deg += 10) {
            float rad = ((float)deg - 90.0f) * 0.0174533f;
            int px = cx + (int)(cosf(rad) * 6.0f);
            int py = cy + (int)(sinf(rad) * 6.0f);
            if (px >= 4 && px <= 74 && py >= 14 && py <= 46) sh1106_draw_pixel(px, py, true);
        }

        // 2. Middle Ring (r=11): Hours Count Fill Arc (1..12)
        int h12 = h % 12; if (h12 == 0) h12 = 12;
        int h_deg = (h12 * 360) / 12;
        for (int deg = 0; deg <= h_deg && deg < 360; deg += 6) {
            float rad = ((float)deg - 90.0f) * 0.0174533f;
            int px = cx + (int)(cosf(rad) * 11.0f);
            int py = cy + (int)(sinf(rad) * 11.0f);
            if (px >= 4 && px <= 74 && py >= 14 && py <= 46) {
                sh1106_draw_pixel(px, py, true);
                sh1106_draw_pixel(cx + (int)(cosf(rad) * 10.0f), cy + (int)(sinf(rad) * 10.0f), true);
            }
        }

        // 3. Outer Ring (r=16): Minutes Fill Arc (0..60m)
        int m_deg = ((int)m * 360) / 60;
        for (int deg = 0; deg <= m_deg && deg < 360; deg += 4) {
            float rad = ((float)deg - 90.0f) * 0.0174533f;
            int px = cx + (int)(cosf(rad) * 16.0f);
            int py = cy + (int)(sinf(rad) * 16.0f);
            if (px >= 4 && px <= 74 && py >= 14 && py <= 46) sh1106_draw_pixel(px, py, true);
        }
    } else if (active_clock_style == CLOCK_STYLE_PRISMATIC) {
        // STYLE 2: Orientation-Independent Chroma Prismatic (Spectral Clock)
        sh1106_fill_rect(cx - 1, cy - 1, 3, 3, true);

        // Ring guides with spectral ticks
        for (int deg = 0; deg < 360; deg += 30) {
            float rad = (float)deg * 0.0174533f;
            sh1106_draw_pixel(cx + (int)(cosf(rad) * 11.0f), cy + (int)(sinf(rad) * 11.0f), true);
            sh1106_draw_pixel(cx + (int)(cosf(rad) * 16.0f), cy + (int)(sinf(rad) * 16.0f), true);
        }

        // 6 Decan gems on inner ring (r=6)
        int s_decan = s / 10;
        for (int d = 0; d < 6; d++) {
            float rad = ((float)d * 60.0f - 90.0f) * 0.0174533f;
            int px = cx + (int)(cosf(rad) * 6.0f);
            int py = cy + (int)(sinf(rad) * 6.0f);
            if (d <= s_decan) {
                sh1106_fill_rect(px - 1, py - 1, 2, 2, true);
            } else {
                sh1106_draw_pixel(px, py, true);
            }
        }

        // Minute decade marker arc on outer ring (r=16)
        int m_decan = m / 10;
        int m_arc_deg = (m_decan + 1) * 60;
        for (int deg = 0; deg <= m_arc_deg; deg += 10) {
            float rad = ((float)deg - 90.0f) * 0.0174533f;
            sh1106_draw_pixel(cx + (int)(cosf(rad) * 16.0f), cy + (int)(sinf(rad) * 16.0f), true);
        }
    } else {
        // STYLE 3: Orientation-Independent Celestial Orrery (Planetary Gears)
        // Center Sun with 4 coronal spikes
        sh1106_fill_rect(cx - 1, cy - 1, 3, 3, true);
        sh1106_draw_line(cx - 3, cy, cx + 3, cy, true);
        sh1106_draw_line(cx, cy - 3, cx, cy + 3, true);

        // Circular orbits (r=6, 11, 16)
        for (int deg = 0; deg < 360; deg += 30) {
            float rad = (float)deg * 0.0174533f;
            sh1106_draw_pixel(cx + (int)(cosf(rad) * 6.0f),  cy + (int)(sinf(rad) * 6.0f),  true);
            sh1106_draw_pixel(cx + (int)(cosf(rad) * 11.0f), cy + (int)(sinf(rad) * 11.0f), true);
            sh1106_draw_pixel(cx + (int)(cosf(rad) * 16.0f), cy + (int)(sinf(rad) * 16.0f), true);
        }

        // Orbiting Minute Planet on middle ring (r=11)
        float m_ang = ((float)m / 60.0f) * 6.2831853f - 1.5707963f;
        int m_px = cx + (int)(cosf(m_ang) * 11.0f);
        int m_py = cy + (int)(sinf(m_ang) * 11.0f);
        sh1106_fill_rect(m_px - 1, m_py - 1, 3, 3, true);

        // Orbiting Second Comet on outer ring (r=16)
        float s_ang = (((float)s + (float)ms / 1000.0f) / 60.0f) * 6.2831853f - 1.5707963f;
        int s_px = cx + (int)(cosf(s_ang) * 16.0f);
        int s_py = cy + (int)(sinf(s_ang) * 16.0f);
        sh1106_draw_pixel(s_px, s_py, true);
        // Comet tail (3 trailing points)
        for (int t = 1; t <= 3; t++) {
            float t_ang = s_ang - (float)t * 0.12f;
            sh1106_draw_pixel(cx + (int)(cosf(t_ang) * 16.0f), cy + (int)(sinf(t_ang) * 16.0f), true);
        }
    }

    // Right Side Telemetry (x = 76..125)
    sh1106_draw_line(76, 14, 76, 46, true);

    if (active_clock_style == CLOCK_STYLE_DIAL) {
        sh1106_draw_string(79, 15, "CHRONOS", true);
    } else if (active_clock_style == CLOCK_STYLE_HOURGLASS) {
        sh1106_draw_string(79, 15, "HOURGLASS", true);
    } else if (active_clock_style == CLOCK_STYLE_PRISMATIC) {
        sh1106_draw_string(79, 15, "CHROMA", true);
    } else {
        sh1106_draw_string(79, 15, "ORRERY", true);
    }

    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
    sh1106_draw_string(79, 24, time_str, true);

    // Edit indicator or live status
    if (clock_style_edit_mode) {
        sh1106_draw_string(79, 33, "> STYLE <", true);
        if (selected_clock_style < CLOCK_STYLE_COUNT) {
            sh1106_draw_string(79, 41, CLOCK_STYLE_SHORT[selected_clock_style], true);
        } else {
            sh1106_draw_string(79, 41, "SET TIME", true);
        }
    } else if (clock_time_edit_mode) {
        if (clock_time_edit_phase == 0) {
            sh1106_draw_string(79, 33, "SET HOUR", true);
            char h_buf[16];
            snprintf(h_buf, sizeof(h_buf), "[%02u]:%02u", (unsigned)h, (unsigned)m);
            sh1106_draw_string(79, 41, h_buf, true);
        } else {
            sh1106_draw_string(79, 33, "SET MIN", true);
            char m_buf[16];
            snprintf(m_buf, sizeof(m_buf), "%02u:[%02u]", (unsigned)h, (unsigned)m);
            sh1106_draw_string(79, 41, m_buf, true);
        }
    } else {
        if (active_clock_style == CLOCK_STYLE_DIAL) {
            sh1106_draw_string(79, 33, "24H ASTR", true);
            // Live swinging pendulum indicator
            int pend_step = (int)((now_ms / 250) % 4);
            int px = 79 + ((pend_step <= 2) ? pend_step * 5 : (4 - pend_step) * 5);
            sh1106_draw_string(79, 41, "- - - -", true);
            sh1106_draw_pixel(px + 1, 44, true);
        } else if (active_clock_style == CLOCK_STYLE_HOURGLASS) {
            sh1106_draw_string(79, 33, "RADIAL ACC", true);
            int pend_step = (int)((now_ms / 250) % 4);
            int px = 79 + ((pend_step <= 2) ? pend_step * 5 : (4 - pend_step) * 5);
            sh1106_draw_string(79, 41, "- - - -", true);
            sh1106_draw_pixel(px + 1, 44, true);
        } else if (active_clock_style == CLOCK_STYLE_PRISMATIC) {
            sh1106_draw_string(79, 33, "SPECTRUM", true);
            // Dynamic 6-decade indicator tag: [R O Y G C V]
            int s_dec = s / 10; if (s_dec > 5) s_dec = 5;
            char dec_tag[16];
            snprintf(dec_tag, sizeof(dec_tag), "[%c%c%c%c%c%c]",
                     (s_dec == 0) ? '*' : 'R',
                     (s_dec == 1) ? '*' : 'O',
                     (s_dec == 2) ? '*' : 'Y',
                     (s_dec == 3) ? '*' : 'G',
                     (s_dec == 4) ? '*' : 'C',
                     (s_dec == 5) ? '*' : 'V');
            sh1106_draw_string(79, 41, dec_tag, true);
        } else {
            sh1106_draw_string(79, 33, "ASTRAL GEAR", true);
            sh1106_draw_string(79, 41, "* ORBIT *", true);
        }
    }

    // Footer Marquee Ticker
    sh1106_draw_line(2, 47, 125, 47, true);
    char clock_ticker[180];
    if (clock_style_edit_mode) {
        snprintf(clock_ticker, sizeof(clock_ticker),
                 "// CHOOSE CLOCK STYLE // TURN: ROTATE // CLICK SW: CONFIRM // QRY: CANCEL //");
    } else if (clock_time_edit_mode) {
        snprintf(clock_ticker, sizeof(clock_ticker),
                 "// ADJUST TIME // TURN: TUNE // CLICK SW: NEXT/SAVE // QRY: CANCEL //");
    } else if (active_clock_style == CLOCK_STYLE_DIAL) {
        snprintf(clock_ticker, sizeof(clock_ticker),
                 "// CELESTIAL CHRONOMETER // 12H ASTRONOMICAL DIAL // CLICK SW TO CHOOSE STYLE //");
    } else if (active_clock_style == CLOCK_STYLE_HOURGLASS) {
        snprintf(clock_ticker, sizeof(clock_ticker),
                 "// AETHERIC HOURGLASS // ORIENTATION-INDEPENDENT // CLICK SW TO CHOOSE STYLE //");
    } else if (active_clock_style == CLOCK_STYLE_PRISMATIC) {
        snprintf(clock_ticker, sizeof(clock_ticker),
                 "// CHROMA PRISMATIC // SPECTRUM TIME // CLICK SW TO CHOOSE STYLE //");
    } else {
        snprintf(clock_ticker, sizeof(clock_ticker),
                 "// CELESTIAL ORRERY // KINEMATIC PLANETARY GEARS // CLICK SW TO CHOOSE STYLE //");
    }
    draw_marquee(51, clock_ticker, now_ms);
}

static void draw_circle_outline(int cx, int cy, int r) {
    int x = r;
    int y = 0;
    int err = 0;
    while (x >= y) {
        sh1106_draw_pixel(cx + x, cy + y, true);
        sh1106_draw_pixel(cx + y, cy + x, true);
        sh1106_draw_pixel(cx - y, cy + x, true);
        sh1106_draw_pixel(cx - x, cy + y, true);
        sh1106_draw_pixel(cx - x, cy - y, true);
        sh1106_draw_pixel(cx - y, cy - x, true);
        sh1106_draw_pixel(cx + y, cy - x, true);
        sh1106_draw_pixel(cx + x, cy - y, true);
        if (err <= 0) {
            y += 1;
            err += 2*y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2*x + 1;
        }
    }
}

static void draw_architecture_card(uint32_t now_ms) {
    // -------------------------------------------------------------
    // Pure Pictogram Dynamic Architecture: STAFF <===> ACTIVE AI HOST
    // Clean cybernetic HUD layout without cluttered text
    // -------------------------------------------------------------
    net_host_type_t host_type = net_client_get_host_type();

    // 1. Futuristic HUD Frame Corner Brackets
    sh1106_draw_line(4, 15, 12, 15, true);
    sh1106_draw_line(4, 15, 4, 23, true);
    sh1106_draw_line(123, 15, 115, 15, true);
    sh1106_draw_line(123, 15, 123, 23, true);
    sh1106_draw_line(4, 59, 12, 59, true);
    sh1106_draw_line(4, 59, 4, 51, true);
    sh1106_draw_line(123, 59, 115, 59, true);
    sh1106_draw_line(123, 59, 123, 51, true);

    // 2. Staff Headpiece Pictogram (Left: centered around x=20, y=34)
    // 3 concentric rings at the top
    draw_circle_outline(20, 24, 7); // Outer ring (24 LED)
    draw_circle_outline(20, 24, 4); // Middle ring (12 LED)
    sh1106_fill_rect(19, 23, 3, 3, true); // Inner gemstone core (1+6 LED)
    // Pulsing ethereal spark on the staff pinnacle
    if ((now_ms / 180) % 2 == 0) {
        sh1106_draw_pixel(20, 15, true);
        sh1106_draw_pixel(19, 16, true);
        sh1106_draw_pixel(21, 16, true);
    }
    // Staff Shaft Rod
    sh1106_draw_line(20, 31, 20, 56, true);
    // Metallic handle brackets
    sh1106_draw_line(17, 37, 23, 37, true);
    sh1106_draw_line(18, 46, 22, 46, true);

    // 3. High-Tech Animated Wireless Data Link (Center: x=30..90)
    if (host_type == HOST_TYPE_PI5 || host_type == HOST_TYPE_PHONE) {
        // Staff radiating antenna arcs: ) )) )))
        sh1106_draw_line(30, 29, 32, 33, true);
        sh1106_draw_line(32, 33, 30, 37, true);
        sh1106_draw_line(34, 27, 37, 33, true);
        sh1106_draw_line(37, 33, 34, 39, true);

        // Host radiating antenna arcs: ((( (( (
        sh1106_draw_line(89, 27, 86, 33, true);
        sh1106_draw_line(86, 33, 89, 39, true);
        sh1106_draw_line(93, 29, 91, 33, true);
        sh1106_draw_line(91, 33, 93, 37, true);

        // Center carrier beam line
        sh1106_draw_line(40, 33, 83, 33, true);

        // Moving data packets flying in both directions
        int p_out = 40 + (int)((now_ms / 35) % 43);
        sh1106_fill_rect(p_out, 32, 2, 3, true);

        int p_in = 83 - (int)((now_ms / 42) % 43);
        sh1106_fill_rect(p_in, 32, 2, 3, true);

        // Radio signal quality dots (x: 58..66, y: 44..48)
        sh1106_fill_rect(56, 47, 2, 2, true);
        sh1106_fill_rect(60, 45, 2, 4, true);
        sh1106_fill_rect(64, 43, 2, 6, true);
        sh1106_fill_rect(68, 41, 2, 8, true);
    } else {
        // Disconnected / Scanning mode: searching radar arcs
        int scan_phase = (int)((now_ms / 300) % 3);
        if (scan_phase >= 0) {
            sh1106_draw_line(42, 31, 44, 33, true);
            sh1106_draw_line(44, 33, 42, 35, true);
        }
        if (scan_phase >= 1) {
            sh1106_draw_line(48, 29, 51, 33, true);
            sh1106_draw_line(51, 33, 48, 37, true);
        }
        if (scan_phase >= 2) {
            sh1106_draw_line(54, 27, 58, 33, true);
            sh1106_draw_line(58, 33, 54, 39, true);
        }
        // Interrupted link cross symbol
        sh1106_draw_line(68, 30, 74, 36, true);
        sh1106_draw_line(68, 36, 74, 30, true);
    }

    // 4. Host Hardware Pictogram (Right: centered around x=108, y=34)
    if (host_type == HOST_TYPE_PI5) {
        // Raspberry Pi 5 Wearable Neural Core
        // Outer SoC body
        sh1106_draw_rect(96, 21, 24, 24, true);
        // Corner mounting vias
        sh1106_draw_pixel(98, 23, true);
        sh1106_draw_pixel(117, 23, true);
        sh1106_draw_pixel(98, 42, true);
        sh1106_draw_pixel(117, 42, true);
        // Heatsink cooling fins on top
        for (int fx = 99; fx <= 117; fx += 3) {
            sh1106_draw_line(fx, 17, fx, 20, true);
        }
        // Animated spinning active cooling fan (circle with rotating 4 blades)
        draw_circle_outline(108, 33, 6);
        int fstep = (int)((now_ms / 80) % 4);
        if (fstep == 0) {
            sh1106_draw_line(103, 33, 113, 33, true);
            sh1106_draw_line(108, 28, 108, 38, true);
        } else if (fstep == 1) {
            sh1106_draw_line(104, 29, 112, 37, true);
            sh1106_draw_line(104, 37, 112, 29, true);
        } else if (fstep == 2) {
            sh1106_draw_line(108, 28, 108, 38, true);
            sh1106_draw_line(103, 33, 113, 33, true);
        } else {
            sh1106_draw_line(112, 29, 104, 37, true);
            sh1106_draw_line(104, 29, 112, 37, true);
        }
        // Activity status LED blinker
        if ((now_ms / 200) % 2 == 0) {
            sh1106_draw_pixel(116, 24, true);
        }
    } else if (host_type == HOST_TYPE_PHONE) {
        // Smartphone Companion Pictogram
        sh1106_draw_rect(99, 19, 18, 28, true);
        // Screen border
        sh1106_draw_rect(101, 22, 14, 20, true);
        // Top camera hole / speaker
        sh1106_draw_pixel(107, 20, true);
        // Home gesture bar
        sh1106_draw_line(105, 44, 110, 44, true);

        // Animated audio equalizer bars on phone screen
        int bar1 = 2 + (int)((now_ms / 120) % 8);
        int bar2 = 2 + (int)((now_ms / 90) % 11);
        int bar3 = 2 + (int)((now_ms / 150) % 7);
        sh1106_draw_line(104, 39, 104, 39 - bar1, true);
        sh1106_draw_line(107, 39, 107, 39 - bar2, true);
        sh1106_draw_line(110, 39, 110, 39 - bar3, true);
    } else {
        // Standalone / Disconnected IC Chip (dashed outline)
        sh1106_draw_rect(98, 22, 20, 22, true);
        sh1106_draw_line(96, 26, 98, 26, true);
        sh1106_draw_line(96, 32, 98, 32, true);
        sh1106_draw_line(96, 38, 98, 38, true);
        sh1106_draw_line(118, 26, 120, 26, true);
        sh1106_draw_line(118, 32, 120, 32, true);
        sh1106_draw_line(118, 38, 120, 38, true);
        sh1106_draw_pixel(107, 32, true);
    }

    // Bottom Decorative Energy Bar
    sh1106_draw_line(30, 56, 98, 56, true);
    int pulse_x = 30 + (int)((now_ms / 60) % 68);
    sh1106_draw_pixel(pulse_x, 56, false);
}

static void draw_cosmos_card(uint32_t now_ms, int32_t rot_pos) {
    if (cosmos_zoom_level == COSMOS_ZOOM_GALAXY) {
        draw_galaxy_card(now_ms, rot_pos);
    } else if (cosmos_zoom_level == COSMOS_ZOOM_SOLAR) {
        draw_solar_system_card(now_ms, rot_pos);
    } else {
        draw_earth_card(now_ms, rot_pos);
    }

    // Top Right Zoom Level Badge (x: 93..125, y: 2..10)
    sh1106_fill_rect(93, 2, 33, 9, true);
    const char *z_tag = (cosmos_zoom_level == COSMOS_ZOOM_GALAXY) ? "1x" :
                        (cosmos_zoom_level == COSMOS_ZOOM_SOLAR)  ? "1k" :
                                                                    "1M";
    char z_str[16];
    snprintf(z_str, sizeof(z_str), "[Z:%s]", z_tag);
    sh1106_draw_string(95, 3, z_str, false);
}

static void internal_draw_dashboard(int sel_idx, int32_t pos, bool sw_down, bool click_feedback);

void ui_draw_dashboard(const ui_dashboard_ctx_t *ctx) {
    if (ctx) {
        s_default_ui_ctx = *ctx;
    }
    s_ui_ctx = &s_default_ui_ctx;
    internal_draw_dashboard(s_default_ui_ctx.current_mode_idx, s_default_ui_ctx.encoder_pos,
                            s_default_ui_ctx.encoder_sw_down, s_default_ui_ctx.click_feedback);
}

static void internal_draw_dashboard(int sel_idx, int32_t pos, bool sw_down, bool click_feedback) {
    if (!s_ui_oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    uint32_t now_ms = to_ms_since_boot(get_absolute_time());

    // Corner HUD Reticle Ticks
    sh1106_draw_pixel(2, 2, true);
    sh1106_draw_pixel(3, 2, true);
    sh1106_draw_pixel(2, 3, true);
    sh1106_draw_pixel(125, 2, true);
    sh1106_draw_pixel(124, 2, true);
    sh1106_draw_pixel(125, 3, true);
    sh1106_draw_pixel(2, 61, true);
    sh1106_draw_pixel(3, 61, true);
    sh1106_draw_pixel(2, 60, true);
    sh1106_draw_pixel(125, 61, true);
    sh1106_draw_pixel(124, 61, true);
    sh1106_draw_pixel(125, 60, true);

    // 1. Clean Utilitarian Header Bar (y=2..12)
    char page_hdr[32];
    snprintf(page_hdr, sizeof(page_hdr), "[%02d] %s", sel_idx + 1, MODE_NAMES[sel_idx]);
    sh1106_draw_string(6, 3, page_hdr, true);

    char lang_badge[16];
    snprintf(lang_badge, sizeof(lang_badge), "[%s|%s]",
             LANG_SHORT_TAGS[active_lang_idx],
             net_client_get_model_tag(active_ai_model_idx));
    int badge_x = 124 - (int)(strlen(lang_badge) * 6);
    sh1106_draw_string(badge_x, 3, lang_badge, true);
    sh1106_draw_line(2, 12, 125, 12, true);

    // 2. Card Body (y=14..47)
    switch (sel_idx) {
        case MODE_STATUS: {
            uint8_t cur_h, cur_m, cur_s;
            uint16_t cur_ms;
            get_current_time(&cur_h, &cur_m, &cur_s, &cur_ms);

            // Row 1 (y=16): Digital Clock + Link Badge (clean fit, no clipping)
            char time_buf[24];
            snprintf(time_buf, sizeof(time_buf), "TIME: %02u:%02u:%02u", cur_h, cur_m, cur_s);
            sh1106_draw_string(6, 16, time_buf, true);

            net_host_type_t host = net_client_get_host_type();
            if (host == HOST_TYPE_PI5) {
                sh1106_draw_string(82, 16, "[PI 5]", true);
            } else if (host == HOST_TYPE_PHONE) {
                sh1106_draw_string(76, 16, "[PHONE]", true);
            } else if (net_client_is_connected()) {
                sh1106_draw_string(76, 16, "[ONLINE]", true);
            } else {
                sh1106_draw_string(76, 16, "[NO LINK]", true);
            }

            // Row 2 (y=26): AI Model Core
            char ai_buf[24];
            snprintf(ai_buf, sizeof(ai_buf), "CORE: %s", net_client_get_model_name(active_ai_model_idx));
            sh1106_draw_string(6, 26, ai_buf, true);

            // Row 3 (y=35): Server Host / IP
            char net_buf[24];
            if (net_client_is_connected()) {
                const char *h_tag = (host == HOST_TYPE_PI5) ? "PI5" :
                                    (host == HOST_TYPE_PHONE) ? "PHN" : "NET";
                snprintf(net_buf, sizeof(net_buf), "HOST: %s %.10s", h_tag, net_client_get_server_ip_str());
            } else {
                snprintf(net_buf, sizeof(net_buf), "HOST: DISCONNECTED");
            }
            sh1106_draw_string(6, 35, net_buf, true);

            // Row 4 (y=44): Audio Gain & Language
            char aud_buf[24];
            snprintf(aud_buf, sizeof(aud_buf), "VOL : %2d%%     VOX: [%s]",
                     (int)audio_get_volume(), LANG_SHORT_TAGS[active_lang_idx]);
            sh1106_draw_string(6, 44, aud_buf, true);

            // Footer Bar (y=52..63): Stationary status (no running ticker)
            sh1106_draw_line(2, 52, 125, 52, true);
            if (current_state == STATE_LISTENING) {
                sh1106_draw_string(6, 54, "[*] RECORDING VOICE...", true);
            } else if (current_state == STATE_PROCESSING) {
                sh1106_draw_string(6, 54, "[~] SUB-ETHA QUERYING...", true);
            } else if (current_state == STATE_SPEAKING) {
                sh1106_draw_string(6, 54, "[#] ORACLE SPEAKING...", true);
            } else {
                sh1106_draw_string(14, 54, "READY :: HOLD QUERY", true);
            }
            break;
        }

        case MODE_ARCHITECTURE: {
            draw_architecture_card(now_ms);
            break;
        }

        case MODE_COSMOS: {
            draw_cosmos_card(now_ms, pos);
            break;
        }

        case MODE_CLOCK: {
            draw_clock_card(now_ms, pos);
            break;
        }

        case MODE_AI_MODEL: {
            char cur_str[32];
            snprintf(cur_str, sizeof(cur_str), "CORE: %s", net_client_get_model_name(active_ai_model_idx));
            sh1106_draw_string(6, 17, cur_str, true);

            char host_str[32];
            net_host_type_t pref = net_client_get_preferred_host();
            const char *lock_str = (pref == HOST_TYPE_PI5) ? "[PI5 LOCK]" :
                                   (pref == HOST_TYPE_PHONE) ? "[PHN LOCK]" :
                                   "[AUTO]";
            snprintf(host_str, sizeof(host_str), "HOST: %s %s", net_client_get_host_name(), lock_str);
            sh1106_draw_string(6, 27, host_str, true);

            char badge_str[32];
            snprintf(badge_str, sizeof(badge_str), "BADGE: [%s]  ID: %d",
                     net_client_get_model_tag(active_ai_model_idx),
                     (int)net_client_get_model_id(active_ai_model_idx));
            sh1106_draw_string(6, 37, badge_str, true);

            sh1106_draw_line(2, 47, 125, 47, true);
            draw_marquee(51, "// DYNAMIC AI CATALOG // CLICK SW TO SELECT HOST OR CORE // PI 5 <-> PHONE //", now_ms);
            break;
        }

        case MODE_LANGUAGE: {
            char cur_str[32];
            snprintf(cur_str, sizeof(cur_str), "DIALECT: %s", LANG_NAMES[active_lang_idx]);
            sh1106_draw_string(6, 16, cur_str, true);

            if (active_lang_idx == LANG_EN) {
                sh1106_draw_string(6, 26, "MODE: FORCED ENGLISH", true);
                sh1106_draw_string(6, 36, "VOX : EN-US (CHRIS)", true);
            } else if (active_lang_idx == LANG_AUTO) {
                sh1106_draw_string(6, 26, "MODE: DYNAMIC SENSING", true);
                sh1106_draw_string(6, 36, "VOX : AUTO MATCH", true);
            } else if (active_lang_idx == LANG_DE) {
                sh1106_draw_string(6, 26, "MODE: FORCED GERMAN", true);
                sh1106_draw_string(6, 36, "VOX : DE-DE (CONRAD)", true);
            } else if (active_lang_idx == LANG_RU) {
                sh1106_draw_string(6, 26, "MODE: FORCED RUSSIAN", true);
                sh1106_draw_string(6, 36, "VOX : RU-RU (SVETLANA)", true);
            }

            sh1106_draw_line(2, 47, 125, 47, true);
            draw_marquee(51, "// VOX LINGUA // CLICK SW TO CHANGE DIALECT // [EN] [AUTO] [DE] [RU] //", now_ms);
            break;
        }

        case MODE_VOLUME: {
            char vol_str[16];
            snprintf(vol_str, sizeof(vol_str), "FLUX: %2d%%", (int)audio_get_volume());
            sh1106_draw_string(6, 17, vol_str, true);
            sh1106_draw_string(72, 17, "[-40..0dB]", true);
            int vol_bar = ((int)audio_get_volume() * 116) / 100;
            sh1106_draw_rect(6, 28, 116, 7, true);
            if (vol_bar > 0) sh1106_fill_rect(6, 28, vol_bar, 7, true);
            sh1106_draw_string(6, 38, "BUS : MAX98357A [OK]", true);
            sh1106_draw_line(2, 47, 125, 47, true);
            draw_marquee(51, "// AETHERIC GAIN // CLICK SW TO TUNE // ROTATE TO ADJUST // QRY TO EXIT //", now_ms);
            break;
        }

        case MODE_LED_CORE: {
            char anim_str[32];
            snprintf(anim_str, sizeof(anim_str), "> %s", leds_get_anim_name(active_anim_idx));
            sh1106_draw_string(6, 17, anim_str, true);
            if (active_anim_idx == LED_ANIM_BREATHE) {
                char col_str[32];
                snprintf(col_str, sizeof(col_str), "COLOR: [%s]", leds_get_rainbow_name(leds_get_rainbow_index()));
                sh1106_draw_string(6, 27, col_str, true);
                sh1106_draw_string(6, 37, "CLICK SW: TUNE COLOR", true);
            } else {
#if CELESTIAL_FOUR_RINGS
                sh1106_draw_string(6, 27, "RUNE: 4 RINGS", true);
                sh1106_draw_string(6, 37, "ARRAY: 59 LUMENS (4%)", true);
#else
                sh1106_draw_string(6, 27, "RUNE: 3 RINGS", true);
                sh1106_draw_string(6, 37, "ARRAY: 43 LUMENS (18%)", true);
#endif
            }
            sh1106_draw_line(2, 47, 125, 47, true);
            draw_marquee(51, "// RUNE MATRIX // 59 WS2812B LEDS // CLICK SW TO SELECT RUNE //", now_ms);
            break;
        }

        case MODE_MIC_TEST: {
            char mic_str[32];
            snprintf(mic_str, sizeof(mic_str), "VOX: %3d%%  PK:%5d", (int)mic_get_level_percent(), (int)mic_get_peak_amplitude());
            sh1106_draw_string(6, 16, mic_str, true);
            int mic_bar = ((int)mic_get_level_percent() * 116) / 100;
            sh1106_draw_rect(6, 26, 116, 8, true);
            if (mic_bar > 0) sh1106_fill_rect(6, 26, mic_bar, 8, true);
            sh1106_draw_string(6, 37, "SENS: INMP441 [OK]", true);
            sh1106_draw_line(2, 47, 125, 47, true);
            draw_marquee(51, "// VOX TRANSDUCER // SPEAK TO TEST // CLICK SW FOR FULL VU // QRY TO EXIT //", now_ms);
            break;
        }

        case MODE_CHIPTUNE: {
            char trk_str[32];
            snprintf(trk_str, sizeof(trk_str), "MUSIC: %d/%d %s",
                     active_track_idx + 1, audio_get_chiptune_count(),
                     audio_is_chiptune_playing() ? "[PLAY]" : "[IDLE]");
            sh1106_draw_string(6, 17, trk_str, true);
            sh1106_draw_string(6, 27, audio_get_chiptune_name(active_track_idx), true);
            char mode_str[32];
            snprintf(mode_str, sizeof(mode_str), "MODE: %s", audio_get_play_mode_name(audio_get_play_mode()));
            sh1106_draw_string(6, 37, mode_str, true);
            sh1106_draw_line(2, 47, 125, 47, true);
            if (audio_is_chiptune_playing()) {
                draw_marquee(51, "// CELESTIAL MUSIC PLAYING // CLICK SW TO STOP // GHOSTBUSTERS & ADDAMS //", now_ms);
            } else {
                draw_marquee(51, "// CELESTIAL MUSIC // CLICK SW TO BROWSE & PLAY // GHOSTBUSTERS & ADDAMS //", now_ms);
            }
            break;
        }

        case MODE_COSMIC: {
            char fx_str[32];
            snprintf(fx_str, sizeof(fx_str), "SFX : %d/%d %s",
                     active_cosmic_idx + 1, audio_get_cosmic_sfx_count(),
                     audio_is_cosmic_sfx_playing() ? "[PLAY]" : "[RDY]");
            sh1106_draw_string(6, 17, fx_str, true);
            sh1106_draw_string(6, 27, audio_get_cosmic_sfx_name(active_cosmic_idx), true);
            sh1106_draw_string(6, 37, "SUITE: COSMIC & R2-D2", true);
            sh1106_draw_line(2, 47, 125, 47, true);
            draw_marquee(51, "// SCI-FI FX & R2-D2 DEMO // CLICK SW TO ENTER // ROTATE TO SELECT //", now_ms);
            break;
        }

        case MODE_VOICE: {
            char v_str[32];
            snprintf(v_str, sizeof(v_str), "VOX : %d/%d %s",
                     active_voice_idx + 1, audio_get_voice_count(),
                     audio_is_voice_playing() ? "[SPK]" : "[RDY]");
            sh1106_draw_string(6, 17, v_str, true);
            sh1106_draw_string(6, 27, audio_get_voice_name((voice_phrase_id_t)active_voice_idx), true);
            sh1106_draw_string(6, 37, "BUF : FLASH AUDIO", true);
            sh1106_draw_line(2, 47, 125, 47, true);
            draw_marquee(51, "// ARCANE LITURGY // CLICK SW TO PLAY PRESET // HOLD QUERY BTN TO TALK //", now_ms);
            break;
        }

        case MODE_SYSTEM: {
            sh1106_draw_string(6, 15, "MCU  : RP2350 M33", true);
            char net_buf[32];
            snprintf(net_buf, sizeof(net_buf), "IP   : %.15s", net_client_is_connected() ? net_client_get_ip_str() : "OFFLINE");
            sh1106_draw_string(6, 23, net_buf, true);
            char ap_buf[32];
            snprintf(ap_buf, sizeof(ap_buf), "AP   : %.15s", net_client_is_connected() ? net_client_get_active_ssid() : "DISCONNECTED");
            sh1106_draw_string(6, 31, ap_buf, true);
            char brain_buf[32];
            snprintf(brain_buf, sizeof(brain_buf), "BRAIN: %s", net_client_is_server_ready() ? "READY" : "STANDALONE");
            sh1106_draw_string(6, 39, brain_buf, true);

            sh1106_draw_line(2, 47, 125, 47, true);
            char srv_ticker[160];
            snprintf(srv_ticker, sizeof(srv_ticker),
                     "// TELEMETRY // AP: %s // HOST: %s:%d // BRAIN: %s // STANDALONE ORACLE READY // BUILD: %s //",
                     net_client_get_active_ssid(), net_client_get_server_ip_str(), (int)net_client_get_server_port(),
                     net_client_is_server_ready() ? "ONLINE" : "OFFLINE", FIRMWARE_BUILD_DATE);
            draw_marquee(51, srv_ticker, now_ms);
            break;
        }

        default:
            break;
    }

    // High contrast overlay if switch was just clicked
    if (click_feedback || sw_down) {
        sh1106_fill_rect(2, 49, 124, 13, true);
        sh1106_draw_string(14, 52, ">> CLICK DETECTED! <<", false);
    } else if (query_button_is_held()) {
        sh1106_fill_rect(2, 49, 124, 13, true);
        sh1106_draw_string(14, 52, ">> QUERY HELD <<", false);
    }

    s_ui_oled_ready = sh1106_update();
}

#undef active_lang_idx
#undef active_ai_model_idx
#undef active_clock_style
#undef selected_clock_style
#undef clock_style_edit_mode
#undef clock_time_edit_mode
#undef clock_time_edit_phase
#undef cosmos_zoom_level
#undef active_anim_idx
#undef active_track_idx
#undef active_voice_idx
#undef active_cosmic_idx
#undef query_button_is_held
#undef current_state
#undef get_current_time
