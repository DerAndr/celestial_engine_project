#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "pico/cyw43_arch.h"
#include "hardware/i2c.h"
#include "config.h"
#include "sh1106.h"
#include "encoder.h"
#include "query_button.h"
#include "audio.h"
#include "voice_data.h"
#include "mic.h"
#include "leds.h"
#include "staff_state.h"

#if __has_include("wifi_credentials.h")
    #include "wifi_credentials.h"
    #define HAS_WIFI_CONFIG 1
#else
    #define HAS_WIFI_CONFIG 0
#endif

typedef enum {
    MODE_ORACLE = 0,
    MODE_GUIDE,
    MODE_SCANNER,
    MODE_CANDY_DETECTOR,
    MODE_VOLUME,
    MODE_MIC_TEST,
    MODE_LED_CORE,
    MODE_CHIPTUNE,
    MODE_VOICE,
    MODE_COSMIC_FX,
    MODE_SYSTEM,
    MODE_COUNT
} staff_mode_t;

static const char *MODE_NAMES[MODE_COUNT] = {
    "ORACLE",
    "GUIDE",
    "SCANNER",
    "CANDY DETECTOR",
    "VOLUME",
    "MIC TEST",
    "LED EFFECTS",
    "8-BIT CHIPTUNE",
    "VOICE ORACLE",
    "COSMIC FX DEMO",
    "SYSTEM"
};

static bool oled_ready = false;
static bool cyw43_ready = false;
static int current_mode_idx = 0;
static int active_confirmed_mode = 0;
static staff_state_t current_state = STATE_IDLE;

static bool volume_edit_mode = false;
static bool mic_test_mode = false;
static bool led_edit_mode = false;
static bool chiptune_mode = false;
static bool voice_mode = false;
static bool cosmic_mode = false;
static int active_anim_idx = 0;
static int active_track_idx = 0;
static int active_voice_idx = 0;
static int active_cosmic_idx = 0;
static bool easter_egg_active = false;
static int32_t prev_encoder_pos = 0;
static absolute_time_t click_feedback_until;

static void draw_67_screen(bool flash_phase) {
    if (!oled_ready) return;

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

    oled_ready = sh1106_update();
}

static void draw_listening_screen(uint32_t hold_ms, int mode_idx, bool anim_blink) {
    if (!oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Recording Header Banner
    sh1106_fill_rect(2, 2, 124, 12, true);
    if (anim_blink) {
        sh1106_draw_string(14, 4, "[ MIC ] LISTENING", false);
    } else {
        sh1106_draw_string(14, 4, "[  *  ] LISTENING", false);
    }

    // Active mode line
    char mode_buf[24];
    snprintf(mode_buf, sizeof(mode_buf), "MODE: %s", MODE_NAMES[mode_idx]);
    sh1106_draw_string(6, 17, mode_buf, true);

    // Hold Duration Header
    sh1106_draw_string(6, 27, "HOLD TIME:", true);

    // Elapsed Recording Timer (large scale 2)
    char timer_buf[20];
    snprintf(timer_buf, sizeof(timer_buf), "%lu.%02lu s",
             (unsigned long)(hold_ms / 1000),
             (unsigned long)((hold_ms % 1000) / 10));
    sh1106_draw_string_scaled(20, 37, timer_buf, 2, true);

    // Live audio level bar during recording (y=49..52)
    int mic_fill = (mic_get_level_percent() * 116) / 100;
    sh1106_draw_rect(6, 48, 116, 4, true);
    if (mic_fill > 0) {
        sh1106_fill_rect(6, 48, mic_fill, 4, true);
    }

    // Footer instruction
    sh1106_draw_line(2, 53, 125, 53, true);
    sh1106_draw_string(4, 55, "RELEASE TO FINISH", true);

    oled_ready = sh1106_update();
}

static void draw_speaking_screen(uint32_t dur_ms, bool anim_blink) {
    if (!oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Playback Header Banner
    sh1106_fill_rect(2, 2, 124, 12, true);
    if (anim_blink) {
        sh1106_draw_string(14, 4, "[ SPK ] PLAYBACK", false);
    } else {
        sh1106_draw_string(14, 4, "[ >>> ] PLAYBACK", false);
    }

    sh1106_draw_string(6, 17, "REPLAYING VOICE", true);
    sh1106_draw_string(6, 27, "DURATION:", true);

    char timer_buf[20];
    snprintf(timer_buf, sizeof(timer_buf), "%lu.%02lu s",
             (unsigned long)(dur_ms / 1000),
             (unsigned long)((dur_ms % 1000) / 10));
    sh1106_draw_string_scaled(20, 37, timer_buf, 2, true);

    // Volume level indicator bar
    int vol_fill = ((int)audio_get_volume() * 116) / 100;
    sh1106_draw_rect(6, 48, 116, 4, true);
    if (vol_fill > 0) {
        sh1106_fill_rect(6, 48, vol_fill, 4, true);
    }

    sh1106_draw_line(2, 53, 125, 53, true);
    sh1106_draw_string(4, 55, "PLAYING RECORDING", true);

    oled_ready = sh1106_update();
}

static void draw_volume_screen(uint8_t vol_percent) {
    if (!oled_ready) return;

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

    oled_ready = sh1106_update();
}

static void draw_mic_test_screen(uint8_t level_pct, int16_t peak_val) {
    if (!oled_ready) return;

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

    oled_ready = sh1106_update();
}

static void draw_led_screen(int anim_idx) {
    if (!oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Header banner
    sh1106_fill_rect(2, 2, 124, 12, true);
    sh1106_draw_string(14, 4, "[ LED EFFECTS ]", false);

    // Effect index
    char num_buf[24];
    snprintf(num_buf, sizeof(num_buf), "EFFECT %d / %d", anim_idx + 1, LED_ANIM_COUNT);
    sh1106_draw_string(20, 18, num_buf, true);

    // Effect name in bold
    sh1106_draw_string(10, 31, leds_get_anim_name(anim_idx), true);

    // Graphical progress bar under effect name
    sh1106_draw_rect(10, 42, 108, 5, true);
    int bar_fill = ((anim_idx + 1) * 104) / LED_ANIM_COUNT;
    if (bar_fill > 0) {
        sh1106_fill_rect(12, 43, bar_fill, 3, true);
    }

    // Footer instruction
    sh1106_draw_line(2, 51, 125, 51, true);
    sh1106_draw_string(6, 54, "TURN: TUNE | SW: OK", true);

    oled_ready = sh1106_update();
}

static void draw_chiptune_screen(int track_idx, bool is_playing) {
    if (!oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Header banner
    sh1106_fill_rect(2, 2, 124, 12, true);
    sh1106_draw_string(14, 4, "[ 8-BIT JUKEBOX ]", false);

    // Track index & status
    char num_buf[48];
    snprintf(num_buf, sizeof(num_buf), "TRK %d/%d  %s",
             track_idx + 1, audio_get_chiptune_count(),
             is_playing ? "[PLAYING]" : "[PAUSED]");
    sh1106_draw_string(6, 17, num_buf, true);

    // Track Name
    sh1106_draw_string(6, 26, audio_get_chiptune_name(track_idx), true);

    // Real-time Volume Gauge (x=6, y=36, w=116, h=5)
    int cur_vol = (int)audio_get_volume();
    char vol_buf[16];
    snprintf(vol_buf, sizeof(vol_buf), "V:%2d%%", cur_vol);
    sh1106_draw_string(6, 36, vol_buf, true);
    sh1106_draw_rect(42, 37, 80, 5, true);
    int v_fill = (cur_vol * 76) / 100;
    if (v_fill > 0) {
        sh1106_fill_rect(44, 38, v_fill, 3, true);
    }

    // Retro Equalizer Bars (animated while playing!)
    for (int b = 0; b < 10; b++) {
        int bar_h = 2;
        if (is_playing) {
            bar_h = 2 + (rand() % 6);
        }
        sh1106_fill_rect(8 + b * 11, 51 - bar_h, 8, bar_h, true);
    }

    // Footer instruction
    sh1106_draw_line(2, 53, 125, 53, true);
    sh1106_draw_string(4, 55, "TURN:VOL SW:NEXT QRY:EX", true);

    oled_ready = sh1106_update();
}

static void draw_voice_screen(int phrase_idx, bool is_playing) {
    if (!oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Header banner
    sh1106_fill_rect(2, 2, 124, 12, true);
    sh1106_draw_string(18, 4, "[ VOICE ORACLE ]", false);

    // Phrase index & status
    char num_buf[48];
    snprintf(num_buf, sizeof(num_buf), "VOICE %d/%d  %s",
             phrase_idx + 1, audio_get_voice_count(),
             is_playing ? "[SPEAKING]" : "[READY]");
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
    sh1106_draw_string(4, 54, "TURN:SEL SW:SPEAK QRY:EX", true);

    oled_ready = sh1106_update();
}

static void draw_cosmic_screen(int sfx_idx, bool is_playing) {
    if (!oled_ready) return;

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

    oled_ready = sh1106_update();
}

static void draw_dashboard(int sel_idx, int32_t pos, bool sw_down, bool click_feedback) {
    if (!oled_ready) return;

    sh1106_clear();

    // Outer decorative border
    sh1106_draw_rect(0, 0, 128, 64, true);

    // 1. Header (y=3..10)
    sh1106_draw_string(6, 3, "CELESTIAL ENGINE", true);
    sh1106_draw_string(106, 3, "P4", true);
    sh1106_draw_line(2, 12, 125, 12, true);

    // 2. Selected Mode (y=15..22)
    char mode_buf[24];
    snprintf(mode_buf, sizeof(mode_buf), "MODE: > %s", MODE_NAMES[sel_idx]);
    sh1106_draw_string(6, 15, mode_buf, true);

    // 3. Volume indicator & Rotation counter (y=24..38)
    char vol_buf[16];
    snprintf(vol_buf, sizeof(vol_buf), "V:%2d%%", (int)audio_get_volume());
    sh1106_draw_string_scaled(6, 24, vol_buf, 2, true);

    char pos_buf[14];
    snprintf(pos_buf, sizeof(pos_buf), "ROT:%+ld", (long)pos);
    sh1106_draw_string(74, 24, pos_buf, true);
    sh1106_draw_string(74, 33, "[IDLE]", true);

    // Divider (y=41)
    sh1106_draw_line(2, 41, 125, 41, true);

    // 4. Live Pin Diagnostics (y=43..50)
    bool raw_a = false, raw_b = false, raw_sw = false;
    encoder_get_raw_pins(&raw_a, &raw_b, &raw_sw);
    bool raw_qry = query_button_get_raw();

    char diag_buf[26];
    snprintf(diag_buf, sizeof(diag_buf), "ENC(SW=%d) QRY=%s",
             raw_sw ? 1 : 0, raw_qry ? "UP" : "DN");
    sh1106_draw_string(6, 43, diag_buf, true);

    // 5. Action status banner (y=53..61)
    if (!raw_qry) {
        // QUERY actively held down
        sh1106_fill_rect(2, 52, 124, 10, true);
        sh1106_draw_string(8, 53, ">> QUERY MIC TEST <<", false);
    } else if (click_feedback || sw_down) {
        // High-contrast inverted bar when encoder switch clicked
        sh1106_fill_rect(2, 52, 124, 10, true);
        sh1106_draw_string(14, 53, ">> CLICK DETECTED! <<", false);
    } else if (sel_idx == MODE_VOLUME) {
        sh1106_draw_string(4, 53, "CLICK SW TO TUNE VOL", true);
    } else if (sel_idx == MODE_MIC_TEST) {
        sh1106_draw_string(4, 53, "CLICK SW TO TEST MIC", true);
    } else if (sel_idx == MODE_LED_CORE) {
        sh1106_draw_string(4, 53, "CLICK SW TO TUNE LED", true);
    } else if (sel_idx == MODE_CHIPTUNE) {
        sh1106_draw_string(4, 53, "CLICK SW: 8-BIT JUKEBOX", true);
    } else if (sel_idx == MODE_VOICE) {
        sh1106_draw_string(4, 53, "CLICK SW: VOICE ORACLE", true);
    } else if (sel_idx == MODE_COSMIC_FX) {
        sh1106_draw_string(4, 53, "CLICK SW: COSMIC FX DEMO", true);
    } else {
        sh1106_draw_string(6, 53, "HOLD QUERY: MIC TEST", true);
    }

    oled_ready = sh1106_update();
}

int main(void) {
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
        oled_ready = true;
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
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        cyw43_arch_enable_sta_mode();
    }

    LOG_INFO("SYS", "Initializing MAX98357A I2S audio subsystem (GP16=BCLK, GP17=LRC, GP18=DIN)...");
    audio_init();

    LOG_INFO("SYS", "Initializing INMP441 I2S microphone (GP10=SCK, GP11=WS, GP12=SD)...");
    mic_init();

    LOG_INFO("SYS", "Initializing WS2812 LED core (GP15=DATA, PIO0 SM1)...");
    leds_init();
#if CELESTIAL_FOUR_RINGS
    LOG_INFO("LED", "FOUR-RING DMA: frames:%u (base:%u + tail:%u) | brightness_cap:%u%% | reset:%uus | boot RGB:%ums/color | address mapping unverified",
             (unsigned)LED_RING_COUNT, (unsigned)LED_BASE_CHAIN_COUNT,
             (unsigned)LED_EXPERIMENT_TAIL_FRAMES, (unsigned)LED_BRIGHTNESS_LIMIT,
             (unsigned)LED_RESET_US, (unsigned)LED_BOOT_COLOR_MS);
#if CELESTIAL_RING_EXPERIMENT
    LOG_INFO("LED", "DIAGNOSTIC: RAINBOW spare holds pure R/G/B for %ums each; other effects unchanged",
             (unsigned)LED_DIAG_COLOR_MS);
#endif
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
        // Continuous audio buffer streaming, mic processing & LED updates
        audio_update();
        if (boot_voice_pending && current_state == STATE_IDLE && !audio_is_playing()) {
            audio_play_voice(VOICE_ONLINE);
            boot_voice_pending = false;
        }
        mic_update();
        leds_update(easter_egg_active, led_edit_mode, active_anim_idx, mic_get_level_percent());

        // Fast sampling of controls
        encoder_update();
        query_button_update();

        // 1. Check QUERY button events (Push-to-Talk or Exit sub-screen)
        if (query_button_just_pressed()) {
            if (chiptune_mode) {
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
            } else {
                current_state = STATE_LISTENING;
                easter_egg_active = false;
                mic_record_start();
                audio_play_tone(392.00f, 25); // Soft warm start-beep (G4)
                LOG_INFO("QUERY", ">>> QUERY PRESSED: Staff entered LISTENING state! Recording voice to RAM... <<<");
                need_display_update = true;
            }
        }

        uint32_t held_dur = query_button_just_released();
        if (held_dur > 0 && current_state == STATE_LISTENING) {
            size_t recorded_samples = mic_record_stop();
            uint32_t rec_ms = mic_get_recorded_duration_ms();
            LOG_INFO("QUERY", "QUERY RELEASED after %lu ms. Recorded %zu samples (%lu ms, %zu bytes). Starting local playback...",
                     (unsigned long)held_dur,
                     recorded_samples,
                     (unsigned long)rec_ms,
                     recorded_samples * sizeof(int16_t));
            if (recorded_samples > 0) {
                current_state = STATE_SPEAKING;
                audio_play_pcm16(mic_get_recorded_samples(NULL), recorded_samples);
            } else {
                current_state = STATE_IDLE;
                audio_play_voice(VOICE_READY); // Speak "System ready"
            }
            need_display_update = true;
        }

        // Return from SPEAKING to IDLE once PCM playback finishes
        if (current_state == STATE_SPEAKING && !audio_is_pcm16_playing()) {
            current_state = STATE_IDLE;
            LOG_INFO("AUDIO", "Recorded voice playback finished. Returned to IDLE.");
            need_display_update = true;
        }

        // 3. Encoder rotation handling
        int32_t delta = encoder_get_delta();
        if (delta != 0 && current_state != STATE_LISTENING) {
            if (volume_edit_mode) {
                int cur_vol = (int)audio_get_volume();
                cur_vol += delta; // 1 percentage point for quiet-level calibration.
                if (cur_vol < 0) cur_vol = 0;
                if (cur_vol > 100) cur_vol = 100;
                audio_set_volume((uint8_t)cur_vol);

                // Play live volume preview tone
                audio_play_tone(440.0f, 150); // Compare loudness at a fixed pitch.
                LOG_INFO("VOLUME", "Volume adjusted: %d%%", cur_vol);
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
                // Immediate, direct volume control while in Jukebox!
                int cur_vol = (int)audio_get_volume();
                cur_vol += delta; // Same quiet-level resolution as the volume menu.
                if (cur_vol < 0) cur_vol = 0;
                if (cur_vol > 100) cur_vol = 100;
                audio_set_volume((uint8_t)cur_vol);
                LOG_INFO("CHIPTUNE", "Live Volume tuned: %d%%", cur_vol);
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
            } else {
                int32_t curr_pos = encoder_get_position();

                // Mode cycling
                current_mode_idx = (current_mode_idx + delta) % MODE_COUNT;
                if (current_mode_idx < 0) {
                    current_mode_idx += MODE_COUNT;
                }

                // Audio tick on each click detent
                audio_play_blip();

                // Check trigger: sequence 6 -> 7 OR counter reaches 67
                if ((prev_encoder_pos == 6 && curr_pos == 7) || curr_pos == 67) {
                    easter_egg_active = true;
                    audio_play_chime();
                    LOG_INFO("EASTER_EGG", ">>> TRIGGERED! Sequence 6 -> 7 reached! Activating '!!! 67 !!!' <<<");
                }

                LOG_INFO("ENCODER", "Pos: %ld | Mode: [%s]",
                         (long)curr_pos,
                         MODE_NAMES[current_mode_idx]);

                prev_encoder_pos = curr_pos;
                need_display_update = true;
            }
        }

        // 4. Encoder switch handling
        if (encoder_switch_clicked() && current_state == STATE_IDLE) {
            if (easter_egg_active) {
                easter_egg_active = false;
                audio_play_tone(440.0f, 40);
                LOG_INFO("EASTER_EGG", "Switch clicked: Exiting '!!! 67 !!!' screen.");
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
            } else if (led_edit_mode) {
                led_edit_mode = false;
                leds_set_anim_mode(active_anim_idx);
                click_feedback_until = delayed_by_ms(get_absolute_time(), 600);
                audio_play_chime();
                LOG_INFO("LEDS", "Effect confirmed: [%s]. Exited LED menu.",
                         leds_get_anim_name(active_anim_idx));
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
            } else if (chiptune_mode) {
                // Advance to next track!
                active_track_idx = (active_track_idx + 1) % audio_get_chiptune_count();
                audio_play_chiptune(active_track_idx);
                LOG_INFO("CHIPTUNE", "Switched to track: %s", audio_get_chiptune_name(active_track_idx));
            } else if (voice_mode) {
                // Play selected voice phrase!
                audio_play_voice((voice_phrase_id_t)active_voice_idx);
                LOG_INFO("VOICE", "Speaking phrase: %s",
                         audio_get_voice_name((voice_phrase_id_t)active_voice_idx));
            } else if (cosmic_mode) {
                // Trigger selected cosmic sound effect!
                audio_play_cosmic_sfx(active_cosmic_idx);
                LOG_INFO("COSMIC", "Triggering Cosmic SFX: %s",
                         audio_get_cosmic_sfx_name(active_cosmic_idx));
            } else if (current_mode_idx == MODE_CHIPTUNE) {
                chiptune_mode = true;
                audio_play_chiptune(active_track_idx);
                LOG_INFO("CHIPTUNE", "Entering 8-bit Jukebox! Playing %s",
                         audio_get_chiptune_name(active_track_idx));
            } else if (current_mode_idx == MODE_VOICE) {
                voice_mode = true;
                audio_play_voice((voice_phrase_id_t)active_voice_idx);
                LOG_INFO("VOICE", "Entering Voice Oracle! Speaking %s",
                         audio_get_voice_name((voice_phrase_id_t)active_voice_idx));
            } else if (current_mode_idx == MODE_COSMIC_FX) {
                cosmic_mode = true;
                audio_play_cosmic_sfx(active_cosmic_idx);
                LOG_INFO("COSMIC", "Entering Cosmic FX Demo! Triggering %s",
                         audio_get_cosmic_sfx_name(active_cosmic_idx));
            } else {
                active_confirmed_mode = current_mode_idx;
                click_feedback_until = delayed_by_ms(get_absolute_time(), 600);
                audio_play_chime();
                LOG_INFO("ENCODER", ">>> SWITCH CLICKED! Active Mode Confirmed: [%s] <<<",
                         MODE_NAMES[active_confirmed_mode]);
            }
            need_display_update = true;
        }

        // 5. Periodic 1 Hz heartbeat
        if (absolute_time_diff_us(last_heartbeat, get_absolute_time()) >= 1000000) {
            last_heartbeat = get_absolute_time();
            tick_count++;
            led_state = !led_state;
            if (cyw43_ready) {
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
            }

            // Auto-recovery attempt for OLED if it was not detected at power-on
            if (!oled_ready && (tick_count % 3 == 0)) {
                int scan_addr = sh1106_i2c_scan(OLED_I2C_INST);
                if (scan_addr > 0 && sh1106_init(OLED_I2C_INST, (uint8_t)scan_addr)) {
                    oled_ready = true;
                    LOG_INFO("OLED", ">>> OLED RECOVERED! Initialized at 0x%02X <<<", scan_addr);
                }
            }

            LOG_INFO("HEARTBEAT", "Tick #%lu | OLED: %s | State: %s | Mode: [%s] | Vol: %d%% | Mic: %d%% | QRY: %s",
                     (unsigned long)tick_count,
                     oled_ready ? "ONLINE" : "OFFLINE",
                     staff_state_name(current_state),
                     MODE_NAMES[active_confirmed_mode],
                     (int)audio_get_volume(),
                     (int)mic_get_level_percent(),
                     query_button_is_held() ? "HELD" : "UP");
            if (AUDIO_DIAGNOSTIC_BOOT) audio_print_diagnostics();
            need_display_update = true;
        }

        // 6. Flash timer for animations (toggles every 400 ms)
        if (absolute_time_diff_us(last_flash_toggle, get_absolute_time()) >= 400000) {
            last_flash_toggle = get_absolute_time();
            flash_phase = !flash_phase;
            if (current_state == STATE_LISTENING || current_state == STATE_SPEAKING || easter_egg_active || volume_edit_mode ||
                mic_test_mode || led_edit_mode || chiptune_mode || voice_mode || cosmic_mode) {
                need_display_update = true;
            }
        }

        // 7. Rate-limited display update (~20 Hz / 50 ms)
        if (need_display_update && absolute_time_diff_us(last_display_refresh, get_absolute_time()) >= 50000) {
            last_display_refresh = get_absolute_time();
            need_display_update = false;

            if (current_state == STATE_LISTENING) {
                draw_listening_screen(query_button_current_hold_ms(), active_confirmed_mode, flash_phase);
                need_display_update = true;
            } else if (current_state == STATE_SPEAKING) {
                draw_speaking_screen(mic_get_recorded_duration_ms(), flash_phase);
                need_display_update = true;
            } else if (easter_egg_active) {
                draw_67_screen(flash_phase);
            } else if (volume_edit_mode) {
                draw_volume_screen(audio_get_volume());
            } else if (mic_test_mode) {
                draw_mic_test_screen(mic_get_level_percent(), mic_get_peak_amplitude());
                need_display_update = true;
            } else if (led_edit_mode) {
                draw_led_screen(active_anim_idx);
            } else if (chiptune_mode) {
                draw_chiptune_screen(active_track_idx, audio_is_chiptune_playing());
                need_display_update = true;
            } else if (voice_mode) {
                draw_voice_screen(active_voice_idx, audio_is_voice_playing());
                need_display_update = true;
            } else if (cosmic_mode) {
                draw_cosmic_screen(active_cosmic_idx, audio_is_cosmic_sfx_playing());
                need_display_update = true;
            } else {
                bool click_feedback = absolute_time_diff_us(get_absolute_time(), click_feedback_until) > 0;
                draw_dashboard(current_mode_idx,
                                encoder_get_position(), encoder_switch_is_down(), click_feedback);
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
                printf("[ENCODER] GP6(A):%d GP7(B):%d GP8(SW):%d | pos:%ld | mode:[%s] | vol_edit:%d\n",
                       a, b, sw, (long)encoder_get_position(), MODE_NAMES[current_mode_idx], volume_edit_mode);
            } else if (cmd_c == '?' || cmd_c == 'h' || cmd_c == 'H') {
                printf("[HELP] Available serial commands:\n");
                printf("  B     - Reboot into BOOTSEL flash mode\n");
                printf("  V<n>  - Set master volume to n%% (e.g. V3)\n");
                printf("  T     - Play celestial harmonic chime\n");
                printf("  M     - Play 8-bit chiptune track\n");
                printf("  C     - Play Cosmic Sci-Fi SFX demo\n");
                printf("  L<n>  - Set LED animation mode (0..%d)\n", LED_ANIM_COUNT - 1);
                printf("  P     - Play voice announcement\n");
                printf("  S     - Stop all audio playback\n");
                printf("  D     - Play fixed 440 Hz diagnostic sine for 1 second\n");
                printf("  U / N - 1 s 440 Hz with only positive / only negative samples\n");
                printf("  A     - Print audio PCM / DMA / FIFO diagnostics\n");
                printf("  E     - Print encoder pins / rotation status\n");
            }
            cmd_c = getchar_timeout_us(0);
        }

        sleep_ms(2);
    }

    return 0;
}
