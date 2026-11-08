#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "pico/cyw43_arch.h"
#include "hardware/i2c.h"
#include "hardware/watchdog.h"
#include "config.h"
#include "sh1106.h"
#include "encoder.h"
#include "query_button.h"
#include "audio.h"
#include "voice_data.h"
#include "mic.h"
#include "leds.h"
#include "staff_state.h"
#include "net_client.h"
#include "ui_screens.h"

#if __has_include("wifi_credentials.h")
    #include "wifi_credentials.h"
    #define HAS_WIFI_CONFIG 1
#else
    #define HAS_WIFI_CONFIG 0
#endif

#define get_current_time ui_get_current_time
#define set_current_time_epoch ui_set_current_time_epoch
#define trigger_offline_oracle ui_trigger_offline_oracle

static bool offline_oracle_mode = false;

static int cosmos_zoom_level = COSMOS_ZOOM_GALAXY;

static int active_ai_model_idx = 0; // Default index into dynamic host catalog
static int selected_ai_model_idx = 0;
static bool ai_model_edit_mode = false;


static int active_lang_idx = LANG_EN;   // Default: English [EN]
static int selected_lang_idx = LANG_EN;
static bool lang_edit_mode = false;

static bool oled_ready = false;
static bool cyw43_ready = false;
static int current_mode_idx = 0;
static int active_confirmed_mode = 0;
static staff_state_t current_state = STATE_IDLE;

static bool volume_edit_mode = false;
static bool mic_test_mode = false;
static bool led_edit_mode = false;
static bool led_color_edit_mode = false;
static bool chiptune_mode = false;
static bool voice_mode = false;
static bool cosmic_mode = false;


static int active_clock_style = CLOCK_STYLE_HOURGLASS; // Orientation-independent by default!
static int selected_clock_style = CLOCK_STYLE_HOURGLASS;
static bool clock_style_edit_mode = false;  // Style Selection menu
static bool clock_time_edit_mode = false;   // Time Setup mode
static int clock_time_edit_phase = 0;       // 0 = Hours, 1 = Minutes
static uint32_t clock_base_seconds = (19 * 3600 + 20 * 60 + 14); // 19:20:14 local initial time
static uint32_t clock_boot_ms = 0;


static int active_anim_idx = LED_ANIM_RAINBOW;
static int live_rendered_anim = LED_ANIM_RAINBOW;
static int active_track_idx = 0;
static int selected_music_idx = 1;
static int active_voice_idx = 0;
static int active_cosmic_idx = 0;
static bool easter_egg_active = false;
static int32_t prev_encoder_pos = 0;
static absolute_time_t click_feedback_until;
static absolute_time_t not_ready_feedback_until;
static uint32_t processing_start_ms = 0;
static uint32_t speaking_start_ms = 0;

static void on_mic_stream_chunk(const int16_t *samples, size_t count) {
    if (net_client_is_connected()) {
        net_client_send_audio_chunk(samples, count);
    }
}


static void sync_chiptune_thematic_leds(int track_idx) {
    const char *name = audio_get_chiptune_name(track_idx);
    if (!name) return;
    int target_anim = -1;
    if (strstr(name, "DOOM")) {
        target_anim = LED_ANIM_DOOM;
    } else if (strstr(name, "TETRIS")) {
        target_anim = LED_ANIM_TETRIS;
    } else if (strstr(name, "ZELDA")) {
        target_anim = LED_ANIM_ZELDA;
    } else if (strstr(name, "IMPERIAL")) {
        target_anim = LED_ANIM_STARWARS;
    } else if (strstr(name, "GHOSTBUSTERS")) {
        target_anim = LED_ANIM_GHOSTBUSTERS;
    } else if (strstr(name, "ADDAMS")) {
        target_anim = LED_ANIM_HALLOWEEN;
    } else if (strstr(name, "MORROWIND")) {
        target_anim = LED_ANIM_MORROWIND;
    } else if (strstr(name, "SPOOKY")) {
        target_anim = LED_ANIM_SPOOKY;
    } else if (strstr(name, "DAFT PUNK")) {
        target_anim = LED_ANIM_DAFT_PUNK;
    } else if (strstr(name, "EBAY")) {
        target_anim = LED_ANIM_EBAY;
    } else if (strstr(name, "HARRY POTTER")) {
        target_anim = LED_ANIM_HARRY_POTTER;
    } else if (strstr(name, "POKEMON")) {
        target_anim = LED_ANIM_POKEMON;
    } else if (strstr(name, "SAILOR MOON")) {
        target_anim = LED_ANIM_SAILOR_MOON;
    }
    if (target_anim >= 0) {
        leds_set_anim_mode(target_anim);
        active_anim_idx = target_anim;
    }
}

static inline void broadcast_telemetry(void) {
    net_client_send_telemetry((uint8_t)current_mode_idx, (uint8_t)current_state,
                              audio_get_volume(), (uint8_t)live_rendered_anim,
                              (uint8_t)active_clock_style, (uint8_t)active_lang_idx,
                              (uint8_t)net_client_get_host_type(),
                              (uint8_t)net_client_get_preferred_host(),
                              (uint8_t)net_client_get_model_theme(active_ai_model_idx),
                              mic_get_level_percent(),
                              (uint8_t)cosmos_zoom_level,
                              audio_is_playing() ? 1 : 0);
}

int main(void) {
    if (watchdog_caused_reboot()) {
        printf("[SYS] >>> RECOVERED FROM WATCHDOG RESET! <<<\n");
    }
    // Enable 8-second hardware watchdog (pause during SWD debug)
    watchdog_enable(8000, 1);

    stdio_init_all();

    // Delay to allow USB CDC terminal enumeration
    sleep_ms(2000);

    printf("\n========================================\n");
    printf("   %s\n", PROJECT_NAME);
    printf("   Firmware: %s\n", FIRMWARE_VERSION);
    printf("   Target:   Raspberry Pi Pico 2 W (RP2350)\n");
    printf("========================================\n\n");

    LOG_INFO("SYS", "Initializing Bourns PEC11R encoder (GP6=A, GP7=B, GP8=SW)...");
    encoder_init();

    LOG_INFO("SYS", "Initializing QUERY button (GP9, internal pull-up)...");
    query_button_init();

    LOG_INFO("SYS", "Configuring I2C1 for SH1106 OLED (GP2 SDA, GP3 SCL, 400 kHz)...");
    i2c_init(OLED_I2C_INST, OLED_I2C_BAUDRATE);
    gpio_set_function(PIN_OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_OLED_SDA);
    gpio_pull_up(PIN_OLED_SCL);

    int detected_addr = sh1106_i2c_scan(OLED_I2C_INST);
    uint8_t target_addr = (detected_addr > 0) ? (uint8_t)detected_addr : OLED_I2C_ADDR;

    LOG_INFO("OLED", "Attempting initialization at 0x%02X...", target_addr);
    if (sh1106_init(OLED_I2C_INST, target_addr)) {
        oled_ready = true; ui_set_oled_ready(true);
        sh1106_set_page_yield_callback(mic_update);
        LOG_INFO("OLED", "SH1106 OLED initialized successfully!");
    } else {
        LOG_ERROR("OLED", "SH1106 initialization failed or no response!");
    }

    LOG_INFO("SYS", "Initializing CYW43 wireless architecture...");
    if (cyw43_arch_init()) {
        LOG_ERROR("SYS", "CYW43 initialization failed!");
    } else {
        cyw43_ready = true;
        LOG_INFO("SYS", "CYW43 initialized successfully.");
        cyw43_arch_lwip_begin();
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        cyw43_arch_lwip_end();
        cyw43_arch_enable_sta_mode();
        net_client_init();
    }

    LOG_INFO("SYS", "Initializing MAX98357A I2S audio subsystem (GP16=BCLK, GP17=LRC, GP18=DIN)...");
    audio_init();

    LOG_INFO("SYS", "Initializing INMP441 I2S microphone (GP10=SCK, GP11=WS, GP12=SD)...");
    mic_init();
    mic_set_stream_callback(on_mic_stream_chunk);

    LOG_INFO("SYS", "Initializing WS2812 LED core (GP15=DATA, PIO0 SM1)...");
    leds_init();
    leds_set_ai_model(net_client_get_model_theme(active_ai_model_idx));
#if CELESTIAL_FOUR_RINGS
    LOG_INFO("LED", "FOUR-RING DMA: frames:%u (base:%u + tail:%u) | brightness_cap:%u%% | reset:%uus | boot RGB:%ums/color | address mapping unverified",
             (unsigned)LED_RING_COUNT, (unsigned)LED_BASE_CHAIN_COUNT,
             (unsigned)LED_EXPERIMENT_TAIL_FRAMES, (unsigned)LED_BRIGHTNESS_LIMIT,
             (unsigned)LED_RESET_US, (unsigned)LED_BOOT_COLOR_MS);
#if CELESTIAL_RING_EXPERIMENT
    LOG_INFO("LED", "DIAGNOSTIC: RAINBOW spare holds pure R/G/B for %ums each; other effects unchanged",
             (unsigned)LED_DIAG_COLOR_MS);
#endif
#else
    LOG_INFO("LED", "THREE-RING DMA: frames:%u (7+12+24) | brightness_cap:%u%% | reset:%uus",
             (unsigned)LED_RING_COUNT, (unsigned)LED_BRIGHTNESS_LIMIT, (unsigned)LED_RESET_US);
#endif

    bool boot_voice_pending = AUDIO_STARTUP_SOUNDS && !AUDIO_DIAGNOSTIC_BOOT;
    if (AUDIO_DIAGNOSTIC_BOOT) {
        LOG_INFO("AUDIO", "Diagnostic boot: muted, continuous I2S, startup sounds disabled. V5 then D for a 1s sine; A for stats; S to stop.");
    } else if (AUDIO_STARTUP_SOUNDS) {
        LOG_INFO("SYS", "Celestial Engine Online! Playing harmonic chime and voice announcement.");
        audio_play_chime();
    } else {
        LOG_INFO("SYS", "Celestial Engine Online! Silent startup; use VOLUME to enable audio.");
    }

    uint32_t tick_count = 0;
    bool led_state = true;
    bool flash_phase = false;
    click_feedback_until = get_absolute_time();
    absolute_time_t last_heartbeat = get_absolute_time();
    absolute_time_t last_display_refresh = get_absolute_time();
    absolute_time_t last_flash_toggle = get_absolute_time();
    bool need_display_update = true;

    while (true) {
        watchdog_update();

        // Continuous audio buffer streaming, mic processing & LED updates
        audio_update();
        if (boot_voice_pending && current_state == STATE_IDLE && !audio_is_playing()) {
            audio_play_voice(VOICE_ONLINE);
            boot_voice_pending = false;
        }
        mic_update();

        // Reactive LED state animations:
        // - LISTENING (recording): Vortex suction into center RED REC indicator, voice reactive!
        // - PROCESSING (sending/receiving): Concentric data reception radar ripples!
        // - SPEAKING: Sonic shockwaves expanding from center outward to rims!
        // - IDLE: User selected animation preset.
        int anim_to_run = active_anim_idx;
        if (current_state == STATE_LISTENING) {
            anim_to_run = LED_ANIM_VORTEX;
        } else if (current_state == STATE_PROCESSING) {
            anim_to_run = LED_ANIM_DATA_RECV;
        } else if (current_state == STATE_SPEAKING && current_mode_idx != MODE_LED_CORE) {
            anim_to_run = LED_ANIM_EXPAND_SPEECH;
        } else if (current_mode_idx == MODE_COSMOS && !led_edit_mode) {
            anim_to_run = (cosmos_zoom_level == COSMOS_ZOOM_GALAXY) ? LED_ANIM_GALAXY :
                          (cosmos_zoom_level == COSMOS_ZOOM_SOLAR)  ? LED_ANIM_SOLAR :
                                                                     LED_ANIM_EARTH;
        } else if (current_mode_idx == MODE_CLOCK && !led_edit_mode) {
            int style_to_show = (clock_style_edit_mode && selected_clock_style < CLOCK_STYLE_COUNT) ?
                                selected_clock_style : active_clock_style;
            anim_to_run = (style_to_show == CLOCK_STYLE_HOURGLASS) ? LED_ANIM_HOURGLASS :
                          (style_to_show == CLOCK_STYLE_PRISMATIC) ? LED_ANIM_PRISMATIC :
                          (style_to_show == CLOCK_STYLE_ORRERY)    ? LED_ANIM_ORRERY :
                                                                     LED_ANIM_CLOCK;
        }

        uint8_t cur_h, cur_m, cur_s;
        uint16_t cur_ms;
        get_current_time(&cur_h, &cur_m, &cur_s, &cur_ms);
        leds_set_clock_time(cur_h, cur_m, cur_s, cur_ms);

        live_rendered_anim = anim_to_run;
        leds_update(easter_egg_active, led_edit_mode, anim_to_run, mic_get_level_percent());

        // Fast sampling of controls
        encoder_update();
        query_button_update();

        // Service Wi-Fi & UDP networking
        net_client_update();

        // Periodic 100 ms telemetry broadcast during active voice/query/processing/speech states
        static absolute_time_t last_active_telemetry = {0};
        if (current_state == STATE_LISTENING || current_state == STATE_SPEAKING || current_state == STATE_PROCESSING) {
            if (absolute_time_diff_us(last_active_telemetry, get_absolute_time()) >= 100000) {
                last_active_telemetry = get_absolute_time();
                broadcast_telemetry();
            }
        }

        // Process authenticated Companion App remote commands
        remote_cmd_t rem_cmd;
        if (net_client_poll_remote_command(&rem_cmd)) {
            LOG_INFO("SECURITY", "Processed authorized companion command '%c' (param %d)",
                     rem_cmd.cmd_code, rem_cmd.param);
            switch (rem_cmd.cmd_code) {
                case 'M': // Set mode
                    if (rem_cmd.param < MODE_COUNT) {
                        current_mode_idx = rem_cmd.param;
                        active_confirmed_mode = rem_cmd.param;
                        need_display_update = true;
                    }
                    break;
                case 'V': // Set volume
                    audio_set_volume(rem_cmd.param);
                    need_display_update = true;
                    break;
                case 'L': // Set LED animation
                    if (rem_cmd.param < LED_ANIM_COUNT) {
                        active_anim_idx = rem_cmd.param;
                        current_mode_idx = MODE_LED_CORE;
                        active_confirmed_mode = MODE_LED_CORE;
                        led_edit_mode = false;
                        need_display_update = true;
                    }
                    break;
                case 'R': // Set LED color palette (Cosmic Breathe)
                    if (rem_cmd.param < leds_get_rainbow_count()) {
                        leds_set_rainbow_index(rem_cmd.param);
                        need_display_update = true;
                        LOG_INFO("LEDS", "Remote command 'R': Color set to [%s]",
                                 leds_get_rainbow_name(rem_cmd.param));
                    }
                    break;
                case 'K': // Set clock style
                    if (rem_cmd.param < CLOCK_STYLE_COUNT) {
                        active_clock_style = rem_cmd.param;
                        need_display_update = true;
                    }
                    break;
                case 'C': // Play SFX
                    audio_play_cosmic_sfx(rem_cmd.param);
                    break;
                case 'T': // Play chiptune / music
                    if (rem_cmd.param == 0xFE) {
                        // Play All
                        audio_set_play_mode(AUDIO_PLAY_ALL);
                        active_track_idx = 0;
                        selected_music_idx = 1;
                        audio_play_chiptune(0);
                        sync_chiptune_thematic_leds(0);
                    } else if (rem_cmd.param == 0xFF) {
                        // Play Cycle
                        audio_set_play_mode(AUDIO_PLAY_CYCLE);
                        active_track_idx = 0;
                        selected_music_idx = 1;
                        audio_play_chiptune(0);
                        sync_chiptune_thematic_leds(0);
                    } else {
                        // Specific track
                        audio_set_play_mode(AUDIO_PLAY_SINGLE);
                        active_track_idx = rem_cmd.param;
                        selected_music_idx = rem_cmd.param + 1;
                        audio_play_chiptune(rem_cmd.param);
                        sync_chiptune_thematic_leds(rem_cmd.param);
                    }
                    need_display_update = true;
                    break;
                case 'P': // Play voice
                    audio_play_voice((voice_phrase_id_t)rem_cmd.param);
                    break;
                case 'S': // Stop audio
                    audio_stop_all();
                    break;
                case 'X': // Time sync
                    set_current_time_epoch(rem_cmd.val32, (uint16_t)rem_cmd.val32_extra);
                    need_display_update = true;
                    break;
                case 'H': // Set preferred host: 0 = Auto, 1 = Pi 5, 2 = Phone
                    net_client_set_preferred_host((net_host_type_t)rem_cmd.param);
                    need_display_update = true;
                    LOG_INFO("NET", "Remote command 'H': Preferred Host set to %d", rem_cmd.param);
                    break;
                case 'D': // Set language dialect: 0 = EN, 1 = Auto, 2 = DE, 3 = RU
                    if (rem_cmd.param < LANG_COUNT) {
                        active_lang_idx = rem_cmd.param;
                        selected_lang_idx = rem_cmd.param;
                        need_display_update = true;
                        LOG_INFO("LANG", "Remote command 'D': Language set to [%s]", LANG_NAMES[active_lang_idx]);
                    }
                    break;
                default:
                    break;
            }
            broadcast_telemetry();
        }

        // Auto-sync active track if playlist advanced in background
        int playing_trk = audio_get_active_track_idx();
        if (playing_trk >= 0 && playing_trk != active_track_idx) {
            active_track_idx = playing_trk;
            selected_music_idx = active_track_idx + 1;
            sync_chiptune_thematic_leds(active_track_idx);
            need_display_update = true;
        }

        // Instant OLED refresh when playback starts or stops
        static bool prev_chiptune_playing = false;
        bool cur_chiptune_playing = audio_is_chiptune_playing();
        if (cur_chiptune_playing != prev_chiptune_playing) {
            prev_chiptune_playing = cur_chiptune_playing;
            need_display_update = true;
        }

        // Check for unsolicited broadcast text pushed from Companion App (e.g. "To Staff")
        if (net_client_has_unsolicited_text()) {
            net_client_clear_unsolicited_text();
            if (!audio_is_stream_playing() && !net_client_is_receiving_response()) {
                audio_play_blip(); // Gentle subtle feedback; never stutter or clash with speech stream
            }
            current_state = STATE_SPEAKING;
            speaking_start_ms = to_ms_since_boot(get_absolute_time());
            need_display_update = true;
            broadcast_telemetry();
            LOG_INFO("NET", "Displaying broadcast text from Companion on OLED: %s", net_client_get_response_text());
        }

        // Unsolicited incoming audio stream automatically switches staff to SPEAKING
        if (current_state != STATE_SPEAKING && current_state != STATE_LISTENING &&
            (audio_is_stream_playing() || net_client_is_receiving_response())) {
            current_state = STATE_SPEAKING;
            speaking_start_ms = to_ms_since_boot(get_absolute_time());
            need_display_update = true;
            broadcast_telemetry();
        }

        // Check incoming Brain response or timeout in STATE_PROCESSING
        if (current_state == STATE_PROCESSING) {
            uint32_t now_ms = to_ms_since_boot(get_absolute_time());
            if (net_client_response_failed()) {
                net_client_cancel();
                current_state = STATE_IDLE;
                audio_play_tone(220.0f, 150);
                need_display_update = true;
            } else if (audio_is_stream_playing() ||
                       (audio_get_volume() == 0 && net_client_is_receiving_response())) {
                current_state = STATE_SPEAKING;
                speaking_start_ms = now_ms;
                need_display_update = true;
            } else if (net_client_has_response_ready() && !net_client_is_receiving_response()) {
                /* No preview replay: the stream already played the full response. */
                net_client_consume_response();
                current_state = STATE_IDLE;
                need_display_update = true;
            } else if (now_ms - processing_start_ms > 180000 ||
                       now_ms - net_client_response_progress_ms() > 45000) {
                LOG_WARN("NET", "Response deadline exceeded; cancelling request and ignoring late packets.");
                net_client_cancel();
                current_state = STATE_IDLE;
                audio_play_tone(220.0f, 150);
                need_display_update = true;
            }
        }

        // Clamp AI model indices to current active catalog count
        uint8_t m_cnt = net_client_get_model_count();
        if (m_cnt > 0) {
            if (active_ai_model_idx >= m_cnt) active_ai_model_idx = 0;
            if (selected_ai_model_idx > m_cnt) selected_ai_model_idx = 0;
        }

        // 1. Check QUERY button events (Push-to-Talk or Exit sub-screen)
        if (query_button_just_pressed()) {
            if (ai_model_edit_mode) {
                ai_model_edit_mode = false;
                LOG_INFO("AI_MODEL", "Exited AI Model menu via button.");
                need_display_update = true;
            } else if (lang_edit_mode) {
                lang_edit_mode = false;
                LOG_INFO("LANG", "Exited Language menu via button.");
                need_display_update = true;
            } else if (chiptune_mode) {
                audio_stop_chiptune();
                chiptune_mode = false;
                LOG_INFO("CHIPTUNE", "Exited Jukebox via button.");
                need_display_update = true;
            } else if (voice_mode) {
                audio_stop_voice();
                voice_mode = false;
                LOG_INFO("VOICE", "Exited Voice Oracle via button.");
                need_display_update = true;
            } else if (cosmic_mode) {
                audio_stop_cosmic_sfx();
                cosmic_mode = false;
                LOG_INFO("COSMIC", "Exited Cosmic FX Demo via button.");
                need_display_update = true;
            } else if (volume_edit_mode) {
                volume_edit_mode = false;
                LOG_INFO("VOLUME", "Exited volume edit via button.");
                need_display_update = true;
            } else if (led_color_edit_mode) {
                led_color_edit_mode = false;
                LOG_INFO("LEDS", "Exited LED color menu via button.");
                need_display_update = true;
            } else if (led_edit_mode) {
                led_edit_mode = false;
                LOG_INFO("LEDS", "Exited LED menu via button.");
                need_display_update = true;
            } else if (mic_test_mode) {
                mic_test_mode = false;
                LOG_INFO("MIC", "Exited mic test via button.");
                need_display_update = true;
            } else if (easter_egg_active) {
                easter_egg_active = false;
                LOG_INFO("EASTER_EGG", "Exited easter egg screen via button.");
                need_display_update = true;
            } else if (clock_style_edit_mode) {
                clock_style_edit_mode = false;
                LOG_INFO("CLOCK", "Exited Clock Style menu via button.");
                need_display_update = true;
            } else if (clock_time_edit_mode) {
                clock_time_edit_mode = false;
                clock_time_edit_phase = 0;
                LOG_INFO("CLOCK", "Cancelled Clock Time setup via button.");
                need_display_update = true;
            } else {
                bool online = net_client_is_connected();
                offline_oracle_mode = !online;
                ui_set_offline_oracle_mode(offline_oracle_mode);
                net_client_cancel();
                audio_stop_all();
                current_state = STATE_LISTENING;
                easter_egg_active = false;
                mic_record_start();
                if (online) {
                    uint8_t mid = net_client_get_model_id(active_ai_model_idx);
                    if (!net_client_send_query_start((uint8_t)active_lang_idx, mid)) {
                        LOG_WARN("NET", "QUERY start UDP send failed; falling back to offline oracle mode.");
                        offline_oracle_mode = true;
                        ui_set_offline_oracle_mode(true);
                    }
                }
                audio_play_tone(880.0f, 40); // Crisp high prompt chirp (A5): "Ready! Speak now!"
                LOG_INFO("QUERY", ">>> QUERY PRESSED: Staff entered LISTENING state! Mode: %s (Lang: %s, Model: %s)... <<<",
                         offline_oracle_mode ? "OFFLINE ORACLE" : "LIVE AI STREAM",
                         LANG_NAMES[active_lang_idx], net_client_get_model_name(active_ai_model_idx));
                need_display_update = true;
            }
            broadcast_telemetry();
        }

        uint32_t held_dur = query_button_just_released();
        if (held_dur > 0 && current_state == STATE_LISTENING) {
            size_t recorded_samples = mic_record_stop();
            uint32_t rec_ms = mic_get_recorded_duration_ms();
            int16_t session_peak = mic_get_record_session_peak();

            LOG_INFO("QUERY", "QUERY RELEASED after %lu ms. Recorded %zu samples (%lu ms, peak: %d).",
                     (unsigned long)held_dur,
                     recorded_samples,
                     (unsigned long)rec_ms,
                     (int)session_peak);

            // Reject only tiny accidental taps (< 150 ms)
            if (rec_ms < 150) {
                LOG_INFO("QUERY", "Recording rejected (duration %lu ms < 150 ms). Query cancelled.",
                         (unsigned long)rec_ms);
                net_client_cancel();
                current_state = STATE_IDLE;
                audio_play_tone(220.0f, 60); // Short low blip: tap ignored
            } else if (offline_oracle_mode) {
                LOG_INFO("ORACLE", "Speaking prophecy for %lu ms voice recording (peak: %d)...",
                         (unsigned long)rec_ms, (int)session_peak);
                current_state = STATE_SPEAKING;
                speaking_start_ms = to_ms_since_boot(get_absolute_time());
                trigger_offline_oracle(active_lang_idx);
                audio_play_blip();
            } else {
                LOG_INFO("NET", "Finishing live Wi-Fi UDP voice stream (%zu samples, %lu ms)...",
                         recorded_samples, (unsigned long)rec_ms);
                bool submitted = net_client_send_query_end();
                current_state = submitted ? STATE_PROCESSING : STATE_IDLE;
                if (!submitted) net_client_cancel();
                processing_start_ms = to_ms_since_boot(get_absolute_time());
                audio_play_blip(); // Crisp upload blip sound!
            }
            need_display_update = true;
            broadcast_telemetry();
        }

        // Return from SPEAKING to IDLE once PCM/stream playback finishes AND at least 8s for text read
        if (current_state == STATE_SPEAKING && !audio_is_stream_playing() && !audio_is_pcm16_playing() && !audio_is_voice_playing() && !net_client_is_receiving_response()) {
            uint32_t spoken_ms = to_ms_since_boot(get_absolute_time()) - speaking_start_ms;
            if (spoken_ms >= 8000) {
                net_client_consume_response();
                current_state = STATE_IDLE;
                offline_oracle_mode = false;
                ui_set_offline_oracle_mode(false);
                if (net_client_response_failed()) {
                    LOG_WARN("NET", "Response stream failed; returned to IDLE without accepting late audio.");
                    audio_play_tone(220.0f, 150);
                } else LOG_INFO("AUDIO", "Response playback finished. Returned to IDLE.");
                need_display_update = true;
                broadcast_telemetry();
            }
        }

        // 3. Encoder rotation handling
        int32_t delta = encoder_get_delta();
        if (delta != 0 && current_state != STATE_LISTENING) {
            if (ai_model_edit_mode) {
                int total_items = 1 + net_client_get_model_count();
                selected_ai_model_idx = (selected_ai_model_idx + delta) % total_items;
                if (selected_ai_model_idx < 0) {
                    selected_ai_model_idx += total_items;
                }
                audio_play_blip();
                if (selected_ai_model_idx == 0) {
                    LOG_INFO("AI_MODEL", "Cursor on Host Selector");
                } else {
                    LOG_INFO("AI_MODEL", "AI Model Core preview: [%s]", net_client_get_model_name(selected_ai_model_idx - 1));
                }
                need_display_update = true;
            } else if (lang_edit_mode) {
                selected_lang_idx = (selected_lang_idx + delta) % LANG_COUNT;
                if (selected_lang_idx < 0) {
                    selected_lang_idx += LANG_COUNT;
                }
                audio_play_blip();
                LOG_INFO("LANG", "Language dialect preview: [%s]", LANG_NAMES[selected_lang_idx]);
                need_display_update = true;
            } else if (volume_edit_mode) {
                int cur_vol = (int)audio_get_volume();
                cur_vol += delta; // 1 percentage point for quiet-level calibration.
                if (cur_vol < 0) cur_vol = 0;
                if (cur_vol > 100) cur_vol = 100;
                audio_set_volume((uint8_t)cur_vol);

                // Play live volume preview tone
                audio_play_tone(440.0f, 150); // Compare loudness at a fixed pitch.
                LOG_INFO("VOLUME", "Volume adjusted: %d%%", cur_vol);
                need_display_update = true;
            } else if (led_color_edit_mode) {
                int c_count = leds_get_rainbow_count();
                int cur_c = leds_get_rainbow_index();
                cur_c = (cur_c + delta) % c_count;
                if (cur_c < 0) cur_c += c_count;
                leds_set_rainbow_index(cur_c);
                audio_play_blip();
                LOG_INFO("LEDS", "Breathe color selected: [%s] (%d/%d)",
                         leds_get_rainbow_name(cur_c), cur_c + 1, c_count);
                need_display_update = true;
            } else if (led_edit_mode) {
                active_anim_idx = (active_anim_idx + delta) % LED_ANIM_COUNT;
                if (active_anim_idx < 0) {
                    active_anim_idx += LED_ANIM_COUNT;
                }
                audio_play_blip();
                LOG_INFO("LEDS", "Effect preview: [%s] (%d/%d)",
                         leds_get_anim_name(active_anim_idx), active_anim_idx + 1, LED_ANIM_COUNT);
                need_display_update = true;
            } else if (chiptune_mode) {
                int total_items = audio_get_chiptune_count() + 1;
                selected_music_idx = (selected_music_idx + delta) % total_items;
                if (selected_music_idx < 0) selected_music_idx += total_items;
                audio_play_blip();
                need_display_update = true;
            } else if (voice_mode) {
                active_voice_idx = (active_voice_idx + delta) % audio_get_voice_count();
                if (active_voice_idx < 0) {
                    active_voice_idx += audio_get_voice_count();
                }
                audio_play_blip();
                LOG_INFO("VOICE", "Voice phrase selected: [%s] (%d/%d)",
                         audio_get_voice_name((voice_phrase_id_t)active_voice_idx),
                         active_voice_idx + 1, audio_get_voice_count());
                need_display_update = true;
            } else if (cosmic_mode) {
                active_cosmic_idx = (active_cosmic_idx + delta) % audio_get_cosmic_sfx_count();
                if (active_cosmic_idx < 0) {
                    active_cosmic_idx += audio_get_cosmic_sfx_count();
                }
                audio_play_blip();
                LOG_INFO("COSMIC", "Cosmic SFX selected: [%s] (%d/%d)",
                         audio_get_cosmic_sfx_name(active_cosmic_idx),
                         active_cosmic_idx + 1, audio_get_cosmic_sfx_count());
                need_display_update = true;
            } else if (clock_style_edit_mode) {
                selected_clock_style = (selected_clock_style + delta) % 5;
                if (selected_clock_style < 0) selected_clock_style += 5;
                if (selected_clock_style < CLOCK_STYLE_COUNT) {
                    active_clock_style = selected_clock_style;
                }
                audio_play_blip();
                need_display_update = true;
            } else if (clock_time_edit_mode) {
                if (clock_time_edit_phase == 0) {
                    int32_t new_sec = (int32_t)clock_base_seconds + (delta * 3600);
                    while (new_sec < 0) new_sec += 86400;
                    clock_base_seconds = (uint32_t)(new_sec % 86400);
                } else {
                    int32_t new_sec = (int32_t)clock_base_seconds + (delta * 60);
                    while (new_sec < 0) new_sec += 86400;
                    clock_base_seconds = (uint32_t)(new_sec % 86400);
                }
                audio_play_blip();
                need_display_update = true;
            } else {
                int32_t curr_pos = encoder_get_position();

                // Mode cycling
                current_mode_idx = (current_mode_idx + delta) % MODE_COUNT;
                if (current_mode_idx < 0) {
                    current_mode_idx += MODE_COUNT;
                }

                // Audio tick on each click detent
                audio_play_blip();

                // Easter egg: triggers ONLY at rotation position 67
                if (curr_pos == 67) {
                    easter_egg_active = true;
                    audio_play_chime();
                    LOG_INFO("EASTER_EGG", ">>> TRIGGERED! Encoder reached 67! Activating '!!! 67 !!!' <<<");
                }

                LOG_INFO("ENCODER", "Pos: %ld | Mode: [%s]",
                         (long)curr_pos,
                         MODE_NAMES[current_mode_idx]);

                prev_encoder_pos = curr_pos;
                broadcast_telemetry();
                need_display_update = true;
            }
        }

        // Check for remote CANCEL from Companion App
        if ((current_state == STATE_PROCESSING || current_state == STATE_SPEAKING) && net_client_check_and_clear_cancel()) {
            LOG_INFO("NET", ">>> Remote CANCEL received from Companion App! Returning to IDLE <<<");
            audio_stop_all();
            current_state = STATE_IDLE;
            offline_oracle_mode = false;
            ui_set_offline_oracle_mode(false);
            need_display_update = true;
            broadcast_telemetry();
        }

        // 4. Encoder switch handling
        if (encoder_switch_clicked()) {
            if (current_state == STATE_PROCESSING || current_state == STATE_SPEAKING) {
                LOG_INFO("INTERRUPT", ">>> EMERGENCY STOP triggered via Encoder Switch! Aborting... <<<");
                net_client_cancel();
                audio_stop_all();
                current_state = STATE_IDLE;
                offline_oracle_mode = false;
                ui_set_offline_oracle_mode(false);
                audio_play_tone(180.0f, 60);
                need_display_update = true;
                broadcast_telemetry();
            } else if (current_state == STATE_IDLE) {
            if (easter_egg_active) {
                easter_egg_active = false;
                audio_play_tone(440.0f, 40);
                LOG_INFO("EASTER_EGG", "Switch clicked: Exiting '!!! 67 !!!' screen.");
            } else if (ai_model_edit_mode) {
                if (selected_ai_model_idx == 0) {
                    // Toggle preferred host
                    net_host_type_t pref = net_client_get_preferred_host();
                    net_host_type_t next_pref = (pref == HOST_TYPE_NONE)  ? HOST_TYPE_PI5 :
                                                (pref == HOST_TYPE_PI5)   ? HOST_TYPE_PHONE :
                                                                            HOST_TYPE_NONE;
                    net_client_set_preferred_host(next_pref);
                    uint8_t m_cnt = net_client_get_model_count();
                    if (active_ai_model_idx >= m_cnt) {
                        active_ai_model_idx = 0;
                    }
                    leds_set_ai_model(net_client_get_model_theme(active_ai_model_idx));
                    audio_play_chime();
                    LOG_INFO("AI_MODEL", "Toggled preferred host to: %d (Active Core: %s)",
                             next_pref, net_client_get_model_name(active_ai_model_idx));
                    need_display_update = true;
                    broadcast_telemetry();
                } else {
                    ai_model_edit_mode = false;
                    uint8_t m_count = net_client_get_model_count();
                    int candidate = selected_ai_model_idx - 1;
                    if (candidate < 0 || (m_count > 0 && candidate >= m_count)) candidate = 0;
                    active_ai_model_idx = candidate;
                    uint8_t theme = net_client_get_model_theme(active_ai_model_idx);
                    leds_set_ai_model(theme);
                    click_feedback_until = delayed_by_ms(get_absolute_time(), 600);
                    audio_play_chime();
                    LOG_INFO("AI_MODEL", "Confirmed AI Core: [%s]. Exited menu.",
                             net_client_get_model_name(active_ai_model_idx));
                }
            } else if (lang_edit_mode) {
                lang_edit_mode = false;
                active_lang_idx = selected_lang_idx;
                click_feedback_until = delayed_by_ms(get_absolute_time(), 600);
                audio_play_chime();
                LOG_INFO("LANG", "Dialect confirmed: [%s]. Exited language menu.",
                         LANG_NAMES[active_lang_idx]);
            } else if (volume_edit_mode) {
                volume_edit_mode = false;
                click_feedback_until = delayed_by_ms(get_absolute_time(), 600);
                audio_play_chime();
                LOG_INFO("VOLUME", "Volume confirmed: %d%%. Exited volume edit.", audio_get_volume());
            } else if (mic_test_mode) {
                mic_test_mode = false;
                click_feedback_until = delayed_by_ms(get_absolute_time(), 600);
                audio_play_chime();
                LOG_INFO("MIC", "Exited mic live test screen.");
            } else if (led_color_edit_mode) {
                led_color_edit_mode = false;
                click_feedback_until = delayed_by_ms(get_absolute_time(), 600);
                audio_play_chime();
                LOG_INFO("LEDS", "Breathe color confirmed: [%s]. Exited color menu.",
                         leds_get_rainbow_name(leds_get_rainbow_index()));
                need_display_update = true;
            } else if (led_edit_mode) {
                if (active_anim_idx == LED_ANIM_BREATHE) {
                    led_edit_mode = false;
                    led_color_edit_mode = true;
                    leds_set_anim_mode(LED_ANIM_BREATHE);
                    audio_play_tone(523.25f, 25);
                    LOG_INFO("LEDS", "Selected Cosmic Breathe! Entering color selection.");
                } else {
                    led_edit_mode = false;
                    leds_set_anim_mode(active_anim_idx);
                    click_feedback_until = delayed_by_ms(get_absolute_time(), 600);
                    audio_play_chime();
                    LOG_INFO("LEDS", "Effect confirmed: [%s]. Exited LED menu.",
                             leds_get_anim_name(active_anim_idx));
                }
                need_display_update = true;
            } else if (clock_style_edit_mode) {
                if (selected_clock_style < CLOCK_STYLE_COUNT) {
                    active_clock_style = selected_clock_style;
                    clock_style_edit_mode = false;
                    click_feedback_until = delayed_by_ms(get_absolute_time(), 600);
                    audio_play_chime();
                    LOG_INFO("CLOCK", "Confirmed style: [%s]. Exited menu.",
                             CLOCK_STYLE_NAMES[active_clock_style]);
                } else {
                    // Selected item 4: ADJUST TIME
                    clock_style_edit_mode = false;
                    clock_time_edit_mode = true;
                    clock_time_edit_phase = 0;
                    audio_play_tone(523.25f, 25);
                    LOG_INFO("CLOCK", "Entering Clock Time setup (HOURS).");
                }
                need_display_update = true;
            } else if (clock_time_edit_mode) {
                if (clock_time_edit_phase == 0) {
                    clock_time_edit_phase = 1;
                    audio_play_tone(659.25f, 25);
                    LOG_INFO("CLOCK", "Switched to adjusting MINUTES.");
                } else {
                    clock_time_edit_mode = false;
                    clock_time_edit_phase = 0;
                    uint8_t h, m, s; uint16_t ms;
                    get_current_time(&h, &m, &s, &ms);
                    clock_base_seconds = (h * 3600 + m * 60);
                    clock_boot_ms = to_ms_since_boot(get_absolute_time());
                    click_feedback_until = delayed_by_ms(get_absolute_time(), 600);
                    audio_play_chime();
                    LOG_INFO("CLOCK", "Time saved: %02u:%02u:00! Exited setup.",
                             (unsigned)h, (unsigned)m);
                }
                need_display_update = true;
            } else if (chiptune_mode) {
                if (selected_music_idx == 0) {
                    if (audio_is_chiptune_playing()) {
                        audio_stop_chiptune();
                        audio_play_tone(220.0f, 30);
                        LOG_INFO("MUSIC", "Stopped playback via [STOP PLAYBACK] row.");
                    } else {
                        audio_play_mode_t next_mode = (audio_play_mode_t)((audio_get_play_mode() + 1) % AUDIO_PLAY_MODE_COUNT);
                        audio_set_play_mode(next_mode);
                        audio_play_tone(523.25f, 25);
                        LOG_INFO("MUSIC", "Play mode toggled to: [%s]", audio_get_play_mode_name(next_mode));
                    }
                } else {
                    int target_trk = selected_music_idx - 1;
                    if (audio_is_chiptune_playing() && active_track_idx == target_trk) {
                        audio_stop_chiptune();
                        audio_play_tone(220.0f, 30);
                        LOG_INFO("MUSIC", "Stopped playback of: %s", audio_get_chiptune_name(target_trk));
                    } else {
                        audio_stop_chiptune();
                        active_track_idx = target_trk;
                        audio_play_chiptune(target_trk);
                        sync_chiptune_thematic_leds(target_trk);
                        LOG_INFO("MUSIC", "Playing track [%d]: %s (Mode: %s)",
                                 target_trk + 1, audio_get_chiptune_name(target_trk),
                                 audio_get_play_mode_name(audio_get_play_mode()));
                    }
                }
                need_display_update = true;
            } else if (voice_mode) {
                // Play selected voice phrase!
                audio_play_voice((voice_phrase_id_t)active_voice_idx);
                LOG_INFO("VOICE", "Speaking phrase: %s",
                         audio_get_voice_name((voice_phrase_id_t)active_voice_idx));
                need_display_update = true;
            } else if (cosmic_mode) {
                // Trigger selected cosmic sound effect!
                audio_play_cosmic_sfx(active_cosmic_idx);
                LOG_INFO("COSMIC", "Triggering Cosmic SFX: %s",
                         audio_get_cosmic_sfx_name(active_cosmic_idx));
                need_display_update = true;
            } else if (current_mode_idx == MODE_CLOCK) {
                clock_style_edit_mode = true;
                selected_clock_style = active_clock_style;
                audio_play_tone(523.25f, 25);
                LOG_INFO("CLOCK", "Entering Clock Style menu (current: %s)",
                         CLOCK_STYLE_NAMES[active_clock_style]);
            } else if (current_mode_idx == MODE_AI_MODEL) {
                ai_model_edit_mode = true;
                selected_ai_model_idx = active_ai_model_idx + 1;
                audio_play_tone(523.25f, 25);
                LOG_INFO("AI_MODEL", "Entering AI Model Core selection (current: %s)",
                         net_client_get_model_name(active_ai_model_idx));
            } else if (current_mode_idx == MODE_LANGUAGE) {
                lang_edit_mode = true;
                selected_lang_idx = active_lang_idx;
                audio_play_tone(523.25f, 25);
                LOG_INFO("LANG", "Entering Language selection menu (current: %s)",
                         LANG_NAMES[active_lang_idx]);
            } else if (current_mode_idx == MODE_VOLUME) {
                volume_edit_mode = true;
                audio_play_tone(523.25f, 25);
                LOG_INFO("VOLUME", "Entering volume edit mode (current: %d%%)", audio_get_volume());
            } else if (current_mode_idx == MODE_MIC_TEST) {
                mic_test_mode = true;
                audio_play_tone(523.25f, 25);
                LOG_INFO("MIC", "Entering mic live test screen.");
            } else if (current_mode_idx == MODE_LED_CORE) {
                led_edit_mode = true;
                active_anim_idx = leds_get_anim_mode();
                audio_play_tone(523.25f, 25);
                LOG_INFO("LEDS", "Entering LED effects menu (current: %s)",
                         leds_get_anim_name(active_anim_idx));
            } else if (current_mode_idx == MODE_CHIPTUNE) {
                if (audio_is_chiptune_playing()) {
                    audio_stop_chiptune();
                    audio_play_tone(220.0f, 30);
                    LOG_INFO("MUSIC", "Stopped playback from dashboard.");
                } else {
                    chiptune_mode = true;
                    selected_music_idx = active_track_idx + 1;
                    audio_play_tone(523.25f, 25);
                    LOG_INFO("MUSIC", "Entering Celestial Music menu (Track: %s)",
                             audio_get_chiptune_name(active_track_idx));
                }
                need_display_update = true;
            } else if (current_mode_idx == MODE_COSMIC) {
                cosmic_mode = true;
                audio_play_cosmic_sfx(active_cosmic_idx);
                LOG_INFO("COSMIC", "Entering Cosmic FX Demo! Playing %s",
                         audio_get_cosmic_sfx_name(active_cosmic_idx));
                need_display_update = true;
            } else if (current_mode_idx == MODE_VOICE) {
                voice_mode = true;
                audio_play_voice((voice_phrase_id_t)active_voice_idx);
                LOG_INFO("VOICE", "Entering Voice Oracle! Speaking %s",
                         audio_get_voice_name((voice_phrase_id_t)active_voice_idx));
                need_display_update = true;
            } else if (current_mode_idx == MODE_COSMOS) {
                cosmos_zoom_level = (cosmos_zoom_level + 1) % COSMOS_ZOOM_COUNT;
                click_feedback_until = delayed_by_ms(get_absolute_time(), 600);
                audio_play_cosmic_sfx((cosmos_zoom_level == COSMOS_ZOOM_GALAXY) ? COSMIC_SFX_WARP :
                                      (cosmos_zoom_level == COSMOS_ZOOM_SOLAR)  ? COSMIC_SFX_PORTAL :
                                                                                 COSMIC_SFX_R2D2_HAPPY);
                LOG_INFO("COSMOS", "Zoomed to level %d (%s)!", cosmos_zoom_level,
                         (cosmos_zoom_level == COSMOS_ZOOM_GALAXY) ? "MILKY WAY GALAXY" :
                         (cosmos_zoom_level == COSMOS_ZOOM_SOLAR)  ? "SOLAR SYSTEM" : "TERRA");
            } else if (current_mode_idx == MODE_ARCHITECTURE) {
                click_feedback_until = delayed_by_ms(get_absolute_time(), 600);
                audio_play_cosmic_sfx(COSMIC_SFX_PULSAR);
                LOG_INFO("ARCH", "Probed architecture schematic!");
            } else {
                active_confirmed_mode = current_mode_idx;
                click_feedback_until = delayed_by_ms(get_absolute_time(), 600);
                audio_play_chime();
                LOG_INFO("ENCODER", ">>> SWITCH CLICKED! Active Mode Confirmed: [%s] <<<",
                         MODE_NAMES[active_confirmed_mode]);
            }
            broadcast_telemetry();
            need_display_update = true;
            }
        }

        // 5. Periodic 1 Hz heartbeat
        if (absolute_time_diff_us(last_heartbeat, get_absolute_time()) >= 1000000) {
            last_heartbeat = get_absolute_time();
            tick_count++;
            led_state = !led_state;
            if (cyw43_ready) {
                cyw43_arch_lwip_begin();
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
                cyw43_arch_lwip_end();
            }

            // Auto-recovery attempt for OLED if it was not detected at power-on
            if (!oled_ready && (tick_count % 3 == 0)) {
                int scan_addr = sh1106_i2c_scan(OLED_I2C_INST);
                if (scan_addr > 0 && sh1106_init(OLED_I2C_INST, (uint8_t)scan_addr)) {
                    oled_ready = true; ui_set_oled_ready(true);
                    LOG_INFO("OLED", ">>> OLED RECOVERED! Initialized at 0x%02X <<<", scan_addr);
                }
            }

            LOG_INFO("HEARTBEAT", "Tick #%lu | OLED: %s | State: %s | Screen: [%s] | Vol: %d%% | Net: %s (%s) | Host: %s | QRY: %s",
                     (unsigned long)tick_count,
                     oled_ready ? "ONLINE" : "OFFLINE",
                     staff_state_name(current_state),
                     MODE_NAMES[current_mode_idx],
                     (int)audio_get_volume(),
                     net_client_is_connected() ? "ONLINE" : (net_client_get_status() == NET_STATUS_CONNECTING ? "CONNECTING" : "OFFLINE"),
                     net_client_get_ip_str(),
                     net_client_get_host_name(),
                     query_button_is_held() ? "HELD" : "UP");
            if (AUDIO_DIAGNOSTIC_BOOT) audio_print_diagnostics();
            broadcast_telemetry();
            need_display_update = true;
        }

        // 6. Flash timer for animations (toggles every 400 ms)
        if (absolute_time_diff_us(last_flash_toggle, get_absolute_time()) >= 400000) {
            last_flash_toggle = get_absolute_time();
            flash_phase = !flash_phase;
            if (current_state == STATE_LISTENING || current_state == STATE_SPEAKING || easter_egg_active || volume_edit_mode ||
                ai_model_edit_mode || lang_edit_mode || mic_test_mode || led_edit_mode || led_color_edit_mode || chiptune_mode || voice_mode || cosmic_mode || clock_style_edit_mode || clock_time_edit_mode) {
                need_display_update = true;
            }
        }

        // 7. Rate-limited display update (~20 Hz / 50 ms)
        if (current_state == STATE_IDLE && !volume_edit_mode && !ai_model_edit_mode && !lang_edit_mode && !led_edit_mode && !led_color_edit_mode && !chiptune_mode && !voice_mode && !cosmic_mode && !mic_test_mode) {
            need_display_update = true; // Continuously advance smooth marquee ticker at 20 FPS
        }

        if (need_display_update && absolute_time_diff_us(last_display_refresh, get_absolute_time()) >= 50000) {
            last_display_refresh = get_absolute_time();
            need_display_update = false;

            if (absolute_time_diff_us(get_absolute_time(), not_ready_feedback_until) > 0) {
                ui_draw_not_ready_screen();
                need_display_update = true;
            } else if (current_state == STATE_LISTENING) {
                ui_draw_listening_screen(query_button_current_hold_ms(), active_confirmed_mode, flash_phase);
                need_display_update = true;
            } else if (current_state == STATE_PROCESSING) {
                ui_draw_processing_screen(to_ms_since_boot(get_absolute_time()) - processing_start_ms, flash_phase);
                need_display_update = true;
            } else if (current_state == STATE_SPEAKING) {
                ui_draw_speaking_screen(to_ms_since_boot(get_absolute_time()) - speaking_start_ms, flash_phase);
                need_display_update = true;
            } else if (easter_egg_active) {
                ui_draw_67_screen(flash_phase);
            } else if (ai_model_edit_mode) {
                ui_draw_ai_model_screen(selected_ai_model_idx, active_ai_model_idx);
                need_display_update = true;
            } else if (lang_edit_mode) {
                ui_draw_language_screen(selected_lang_idx, active_lang_idx);
                need_display_update = true;
            } else if (volume_edit_mode) {
                ui_draw_volume_screen(audio_get_volume());
            } else if (mic_test_mode) {
                ui_draw_mic_test_screen(mic_get_level_percent(), mic_get_peak_amplitude());
                need_display_update = true;
            } else if (led_color_edit_mode) {
                ui_draw_led_color_screen(leds_get_rainbow_index());
            } else if (led_edit_mode) {
                ui_draw_led_screen(active_anim_idx);
            } else if (chiptune_mode) {
                ui_draw_chiptune_screen(selected_music_idx, active_track_idx, audio_is_chiptune_playing());
                need_display_update = true;
            } else if (voice_mode) {
                ui_draw_voice_screen(active_voice_idx, audio_is_voice_playing());
                need_display_update = true;
            } else if (cosmic_mode) {
                ui_draw_cosmic_screen(active_cosmic_idx, audio_is_cosmic_sfx_playing());
                need_display_update = true;
            } else {
                bool click_feedback = absolute_time_diff_us(get_absolute_time(), click_feedback_until) > 0;
                ui_dashboard_ctx_t ctx = {
                    .current_state = current_state,
                    .current_mode_idx = current_mode_idx,
                    .encoder_pos = encoder_get_position(),
                    .encoder_sw_down = encoder_switch_is_down(),
                    .click_feedback = click_feedback,
                    .query_btn_held = query_button_is_held(),
                    .active_lang_idx = active_lang_idx,
                    .active_ai_model_idx = active_ai_model_idx,
                    .active_clock_style = active_clock_style,
                    .selected_clock_style = selected_clock_style,
                    .clock_style_edit_mode = clock_style_edit_mode,
                    .clock_time_edit_mode = clock_time_edit_mode,
                    .clock_time_edit_phase = clock_time_edit_phase,
                    .cosmos_zoom_level = cosmos_zoom_level,
                    .active_anim_idx = active_anim_idx,
                    .active_track_idx = active_track_idx,
                    .active_voice_idx = active_voice_idx,
                    .active_cosmic_idx = active_cosmic_idx
                };
                ui_draw_dashboard(&ctx);
                need_display_update = true;
            }
        }

        // 8. Process serial commands from PC dashboard / monitor
        int cmd_c = getchar_timeout_us(0);
        while (cmd_c != PICO_ERROR_TIMEOUT) {
            if (cmd_c == 'B' || cmd_c == 'b') {
                LOG_INFO("SYS", "Rebooting into BOOTSEL mode requested via serial...");
                sleep_ms(50);
                reset_usb_boot(0, 0);
            } else if (cmd_c == 'V' || cmd_c == 'v') {
                int vol = 0;
                int d = getchar_timeout_us(20000);
                while (d == ' ' || d == '\t') {
                    d = getchar_timeout_us(20000);
                }
                bool has_digits = false;
                while (d >= '0' && d <= '9') {
                    vol = vol * 10 + (d - '0');
                    has_digits = true;
                    d = getchar_timeout_us(20000);
                }
                if (has_digits && vol >= 0 && vol <= 100) {
                    audio_set_volume((uint8_t)vol);
                    LOG_INFO("VOLUME", "Volume set via serial to %d%%", vol);
                    need_display_update = true;
                }
            } else if (cmd_c == 'T' || cmd_c == 't') {
                LOG_INFO("AUDIO", "Chime triggered via serial");
                audio_play_chime();
            } else if (cmd_c == 'M' || cmd_c == 'm') {
                int trk = -1;
                int d = getchar_timeout_us(20000);
                while (d == ' ' || d == '\t') d = getchar_timeout_us(20000);
                while (d >= '0' && d <= '9') {
                    if (trk < 0) trk = 0;
                    trk = trk * 10 + (d - '0');
                    d = getchar_timeout_us(20000);
                }
                if (trk >= 0 && trk < audio_get_chiptune_count()) {
                    active_track_idx = trk;
                } else {
                    active_track_idx = (active_track_idx + 1) % audio_get_chiptune_count();
                }
                chiptune_mode = true;
                LOG_INFO("AUDIO", "Chiptune track [%s] triggered via serial",
                         audio_get_chiptune_name(active_track_idx));
                audio_play_chiptune(active_track_idx);
                sync_chiptune_thematic_leds(active_track_idx);
                need_display_update = true;
            } else if (cmd_c == 'D' || cmd_c == 'd') {
                // Instant Doom Mode!
                active_track_idx = 2; // DOOM E1M1 RIFF
                chiptune_mode = true;
                LOG_INFO("DOOM", ">>> DOOM MODE ACTIVATED VIA SERIAL! (E1M1 + HELLFIRE LEDS) <<<");
                audio_play_chiptune(active_track_idx);
                sync_chiptune_thematic_leds(active_track_idx);
                need_display_update = true;
            } else if (cmd_c == 'C' || cmd_c == 'c') {
                int sfx = -1;
                int d = getchar_timeout_us(20000);
                while (d == ' ' || d == '\t') d = getchar_timeout_us(20000);
                while (d >= '0' && d <= '9') {
                    if (sfx < 0) sfx = 0;
                    sfx = sfx * 10 + (d - '0');
                    d = getchar_timeout_us(20000);
                }
                if (sfx >= 0 && sfx < audio_get_cosmic_sfx_count()) {
                    active_cosmic_idx = sfx;
                } else {
                    active_cosmic_idx = (active_cosmic_idx + 1) % audio_get_cosmic_sfx_count();
                }
                cosmic_mode = true;
                LOG_INFO("COSMIC", "Cosmic SFX [%s] triggered via serial",
                         audio_get_cosmic_sfx_name(active_cosmic_idx));
                audio_play_cosmic_sfx(active_cosmic_idx);
                need_display_update = true;
            } else if (cmd_c == 'L' || cmd_c == 'l') {
                int anim = getchar_timeout_us(20000);
                if (anim >= '0' && anim < '0' + LED_ANIM_COUNT) {
                    leds_set_anim_mode(anim - '0');
                    LOG_INFO("LEDS", "LED mode set via serial to [%s]", leds_get_anim_name(anim - '0'));
                } else {
                    int next_anim = (leds_get_anim_mode() + 1) % LED_ANIM_COUNT;
                    leds_set_anim_mode(next_anim);
                    LOG_INFO("LEDS", "LED mode toggled via serial to [%s]", leds_get_anim_name(next_anim));
                }
            } else if (cmd_c == 'R' || cmd_c == 'r') {
                int col = -1;
                int d = getchar_timeout_us(20000);
                while (d == ' ' || d == '\t') d = getchar_timeout_us(20000);
                while (d >= '0' && d <= '9') {
                    if (col < 0) col = 0;
                    col = col * 10 + (d - '0');
                    d = getchar_timeout_us(20000);
                }
                if (col >= 0 && col < leds_get_rainbow_count()) {
                    leds_set_rainbow_index(col);
                } else {
                    int next_c = (leds_get_rainbow_index() + 1) % leds_get_rainbow_count();
                    leds_set_rainbow_index(next_c);
                }
                LOG_INFO("LEDS", "Cosmic Breathe color set via serial to [%s] (%d/%d)",
                         leds_get_rainbow_name(leds_get_rainbow_index()),
                         leds_get_rainbow_index() + 1, leds_get_rainbow_count());
                need_display_update = true;
            } else if (cmd_c == 'S' || cmd_c == 's') {
                LOG_INFO("AUDIO", "Audio playback stopped via serial");
                audio_stop_all();
                boot_voice_pending = false;
            } else if (cmd_c == 'P' || cmd_c == 'p') {
                LOG_INFO("AUDIO", "Voice phrase triggered via serial");
                audio_stop_all();
                audio_play_voice(VOICE_ONLINE);
            } else if (cmd_c == 'D' || cmd_c == 'd') {
                boot_voice_pending = false;
                LOG_INFO("AUDIO", "Diagnostic 440 Hz sine, 1 second at %u%%", (unsigned)audio_get_volume());
                audio_play_diagnostic_tone();
            } else if (cmd_c == 'U' || cmd_c == 'u' || cmd_c == 'N' || cmd_c == 'n') {
                boot_voice_pending = false;
                int polarity = (cmd_c == 'U' || cmd_c == 'u') ? 1 : -1;
                LOG_INFO("AUDIO", "Polarity test: 440 Hz, 1 s, %s samples only, at %u%%",
                         polarity > 0 ? "POSITIVE" : "NEGATIVE", (unsigned)audio_get_volume());
                audio_play_polarity_tone(polarity);
            } else if (cmd_c == 'A' || cmd_c == 'a') {
                audio_print_diagnostics();
            } else if (cmd_c == 'E' || cmd_c == 'e') {
                bool a, b, sw;
                encoder_get_raw_pins(&a, &b, &sw);
                printf("[PINS] GP6(A):%d GP7(B):%d GP8(SW):%d GP9(QRY):%d (held:%d) | pos:%ld | mode:[%s] | vol_edit:%d\n",
                       a, b, sw, query_button_get_raw(), query_button_is_held(), (long)encoder_get_position(), MODE_NAMES[current_mode_idx], volume_edit_mode);
            } else if (cmd_c == 'Q') {
                LOG_INFO("SERIAL", "Simulating QUERY button PRESS via serial command 'Q'...");
                query_button_simulate_press();
                need_display_update = true;
            } else if (cmd_c == 'q') {
                LOG_INFO("SERIAL", "Simulating QUERY button RELEASE via serial command 'q'...");
                query_button_simulate_release(2500);
                need_display_update = true;
            } else if (cmd_c == '?' || cmd_c == 'h' || cmd_c == 'H') {
                printf("[HELP] Available serial commands:\n");
                printf("  B     - Reboot into BOOTSEL flash mode\n");
                printf("  V<n>  - Set master volume to n%% (e.g. V3)\n");
                printf("  T     - Play celestial harmonic chime\n");
                printf("  M     - Play 8-bit chiptune track\n");
                printf("  C     - Play Cosmic Sci-Fi SFX demo\n");
                printf("  L<n>  - Set LED animation mode (0..%d)\n", LED_ANIM_COUNT - 1);
                printf("  R<n>  - Set Cosmic Breathe LED color (0..%d, e.g. R0=CYAN)\n", leds_get_rainbow_count() - 1);
                printf("  P     - Play voice announcement\n");
                printf("  S     - Stop all audio playback\n");
                printf("  D     - Play fixed 440 Hz diagnostic sine for 1 second\n");
                printf("  U / N - 1 s 440 Hz with only positive / only negative samples\n");
                printf("  A     - Print audio PCM / DMA / FIFO diagnostics\n");
                printf("  E     - Print encoder & query button pins status\n");
                printf("  Q / q - Simulate QUERY button press / release\n");
                printf("  > / < - Next / previous screen mode\n");
                printf("  O<n>  - Jump directly to screen mode n (0..%d)\n", MODE_COUNT - 1);
            } else if (cmd_c == '>' || cmd_c == ']') {
                current_mode_idx = (current_mode_idx + 1) % MODE_COUNT;
                need_display_update = true;
                LOG_INFO("MODE", "Navigated to mode [%d] %s", current_mode_idx, MODE_NAMES[current_mode_idx]);
            } else if (cmd_c == '<' || cmd_c == '[') {
                current_mode_idx = (current_mode_idx - 1 + MODE_COUNT) % MODE_COUNT;
                need_display_update = true;
                LOG_INFO("MODE", "Navigated to mode [%d] %s", current_mode_idx, MODE_NAMES[current_mode_idx]);
            } else if (cmd_c == 'O' || cmd_c == 'o') {
                int m = getchar_timeout_us(20000);
                if (m >= '0' && m <= '9') {
                    int mode_num = m - '0';
                    int m2 = getchar_timeout_us(20000);
                    if (m2 >= '0' && m2 <= '9') {
                        mode_num = mode_num * 10 + (m2 - '0');
                    }
                    if (mode_num >= 0 && mode_num < MODE_COUNT) {
                        current_mode_idx = mode_num;
                        need_display_update = true;
                        LOG_INFO("MODE", "Switched to mode [%d] %s", current_mode_idx, MODE_NAMES[current_mode_idx]);
                    }
                }
            } else if (cmd_c == 'W' || cmd_c == 'w') {
                int h = getchar_timeout_us(20000);
                if (h >= '0' && h <= '2') {
                    net_host_type_t new_pref = (net_host_type_t)(h - '0');
                    net_client_set_preferred_host(new_pref);
                    need_display_update = true;
                    broadcast_telemetry();
                    LOG_INFO("HOST", "Host preference set to %d via serial (%s)",
                             (int)new_pref, new_pref == HOST_TYPE_PI5 ? "PI5 LOCK" : new_pref == HOST_TYPE_PHONE ? "PHONE LOCK" : "AUTO MESH");
                }
            }
            cmd_c = getchar_timeout_us(0);
        }

        sleep_ms(2);
    }

    return 0;
}
