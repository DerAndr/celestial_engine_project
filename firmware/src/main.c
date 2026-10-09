#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/i2c.h"
#include "config.h"
#include "sh1106.h"
#include "encoder.h"
#include "query_button.h"
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
    MODE_SYSTEM,
    MODE_COUNT
} staff_mode_t;

static const char *MODE_NAMES[MODE_COUNT] = {
    "ORACLE",
    "GUIDE",
    "SCANNER",
    "CANDY DETECTOR",
    "SYSTEM"
};

static bool oled_ready = false;
static int current_mode_idx = 0;
static int active_confirmed_mode = 0;
static staff_state_t current_state = STATE_IDLE;
static absolute_time_t state_timeout;

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

    sh1106_update();
}

static void draw_listening_screen(uint32_t hold_ms, int mode_idx, bool anim_blink) {
    if (!oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Recording Header Banner
    sh1106_fill_rect(2, 2, 124, 12, true);
    if (anim_blink) {
        sh1106_draw_string(14, 4, "[ REC ] LISTENING", false);
    } else {
        sh1106_draw_string(14, 4, "[  *  ] LISTENING", false);
    }

    // Active mode line
    char mode_buf[24];
    snprintf(mode_buf, sizeof(mode_buf), "MODE: %s", MODE_NAMES[mode_idx]);
    sh1106_draw_string(6, 17, mode_buf, true);

    // Hold Duration Header
    sh1106_draw_string(6, 27, "RECORD TIME:", true);

    // Elapsed Recording Timer (large scale 2)
    char timer_buf[20];
    snprintf(timer_buf, sizeof(timer_buf), "%lu.%02lu s",
             (unsigned long)(hold_ms / 1000),
             (unsigned long)((hold_ms % 1000) / 10));
    sh1106_draw_string_scaled(20, 37, timer_buf, 2, true);

    // Footer instruction
    sh1106_draw_line(2, 53, 125, 53, true);
    sh1106_draw_string(4, 55, "RELEASE TO SUBMIT", true);

    sh1106_update();
}

static void draw_uploading_screen(void) {
    if (!oled_ready) return;

    sh1106_clear();
    sh1106_draw_rect(0, 0, 128, 64, true);

    sh1106_draw_string(16, 6, "CELESTIAL ENGINE", true);

    sh1106_fill_rect(4, 20, 120, 14, true);
    sh1106_draw_string(14, 23, ">>> UPLOADING >>>", false);

    sh1106_draw_string(12, 40, "SENDING AUDIO PI 5", true);
    sh1106_draw_line(2, 52, 125, 52, true);
    sh1106_draw_string(24, 54, "* RETURNING IDLE *", true);

    sh1106_update();
}

static void draw_dashboard(uint32_t tick, bool led_on, int sel_idx, int32_t pos, bool sw_down, bool click_feedback) {
    if (!oled_ready) return;

    sh1106_clear();

    // Outer decorative border
    sh1106_draw_rect(0, 0, 128, 64, true);

    // 1. Header (y=3..10)
    sh1106_draw_string(6, 3, "CELESTIAL ENGINE", true);
    sh1106_draw_string(106, 3, "P3", true);
    sh1106_draw_line(2, 12, 125, 12, true);

    // 2. Selected Mode (y=15..22)
    char mode_buf[24];
    snprintf(mode_buf, sizeof(mode_buf), "MODE: > %s", MODE_NAMES[sel_idx]);
    sh1106_draw_string(6, 15, mode_buf, true);

    // 3. Rotation number & state (y=25..38)
    char pos_buf[16];
    snprintf(pos_buf, sizeof(pos_buf), "ROT:%+ld", (long)pos);
    sh1106_draw_string_scaled(6, 25, pos_buf, 2, true);
    sh1106_draw_string(84, 28, "[IDLE]", true);

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
        sh1106_draw_string(8, 53, ">> QUERY HELD (REC) <<", false);
    } else if (click_feedback || sw_down) {
        // High-contrast inverted bar when encoder switch clicked
        sh1106_fill_rect(2, 52, 124, 10, true);
        sh1106_draw_string(14, 53, ">> CLICK DETECTED! <<", false);
    } else {
        sh1106_draw_string(6, 53, "HOLD QUERY TO RECORD", true);
    }

    sh1106_update();
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
        LOG_INFO("OLED", "SH1106 OLED initialized successfully!");
    } else {
        LOG_ERROR("OLED", "SH1106 initialization failed or no response!");
    }

    LOG_INFO("SYS", "Initializing CYW43 wireless architecture...");
    if (cyw43_arch_init()) {
        LOG_ERROR("SYS", "CYW43 initialization failed!");
    } else {
        LOG_INFO("SYS", "CYW43 initialized successfully.");
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        cyw43_arch_enable_sta_mode();
    }

    LOG_INFO("SYS", "Milestone P3 Active! Hold QUERY button to enter LISTENING; release to submit.");

    uint32_t tick_count = 0;
    bool led_state = true;
    bool flash_phase = false;
    click_feedback_until = get_absolute_time();
    state_timeout = get_absolute_time();
    absolute_time_t last_heartbeat = get_absolute_time();
    absolute_time_t last_display_refresh = get_absolute_time();
    absolute_time_t last_flash_toggle = get_absolute_time();
    bool need_display_update = true;

    while (true) {
        // Fast sampling of controls
        encoder_update();
        query_button_update();

        // 1. Check QUERY button events (Push-to-Talk)
        if (query_button_just_pressed()) {
            current_state = STATE_LISTENING;
            easter_egg_active = false;
            LOG_INFO("QUERY", ">>> QUERY PRESSED: Staff entered LISTENING state! <<<");
            need_display_update = true;
        }

        uint32_t held_dur = query_button_just_released();
        if (held_dur > 0) {
            current_state = STATE_UPLOADING;
            state_timeout = delayed_by_ms(get_absolute_time(), 1200);
            LOG_INFO("QUERY", ">>> QUERY RELEASED after %lu ms! State: UPLOADING -> IDLE <<<",
                     (unsigned long)held_dur);
            need_display_update = true;
        }

        // 2. Handle temporary states (UPLOADING -> IDLE)
        if (current_state == STATE_UPLOADING) {
            if (absolute_time_diff_us(state_timeout, get_absolute_time()) >= 0) {
                current_state = STATE_IDLE;
                LOG_INFO("STAFF", "Upload complete. Returning to IDLE state.");
                need_display_update = true;
            }
        }

        // 3. Encoder rotation handling
        int32_t delta = encoder_get_delta();
        if (delta != 0 && current_state != STATE_LISTENING) {
            int32_t curr_pos = encoder_get_position();

            // Mode cycling
            current_mode_idx = (current_mode_idx + delta) % MODE_COUNT;
            if (current_mode_idx < 0) {
                current_mode_idx += MODE_COUNT;
            }

            // Check trigger: sequence 6 -> 7 OR counter reaches 67
            if ((prev_encoder_pos == 6 && curr_pos == 7) || curr_pos == 67) {
                easter_egg_active = true;
                LOG_INFO("EASTER_EGG", ">>> TRIGGERED! Sequence 6 -> 7 reached! Activating '!!! 67 !!!' <<<");
            }

            LOG_INFO("ENCODER", "Pos: %ld | Mode: [%s]",
                     (long)curr_pos,
                     MODE_NAMES[current_mode_idx]);

            prev_encoder_pos = curr_pos;
            need_display_update = true;
        }

        // 4. Encoder switch handling
        if (encoder_switch_clicked()) {
            if (easter_egg_active) {
                easter_egg_active = false;
                LOG_INFO("EASTER_EGG", "Switch clicked: Exiting '!!! 67 !!!' screen.");
            } else {
                active_confirmed_mode = current_mode_idx;
                click_feedback_until = delayed_by_ms(get_absolute_time(), 600);
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
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);

            bool a_val, b_val, sw_val;
            encoder_get_raw_pins(&a_val, &b_val, &sw_val);

            LOG_INFO("HEARTBEAT", "Tick #%lu | LED: %s | State: %s | Mode: [%s] | Pos: %ld | QRY: %s",
                     (unsigned long)tick_count,
                     led_state ? "ON" : "OFF",
                     staff_state_name(current_state),
                     MODE_NAMES[active_confirmed_mode],
                     (long)encoder_get_position(),
                     query_button_is_held() ? "HELD" : "UP");
            need_display_update = true;
        }

        // 6. Flash timer for animations (toggles every 400 ms)
        if (absolute_time_diff_us(last_flash_toggle, get_absolute_time()) >= 400000) {
            last_flash_toggle = get_absolute_time();
            flash_phase = !flash_phase;
            if (current_state == STATE_LISTENING || easter_egg_active) {
                need_display_update = true;
            }
        }

        // 7. Rate-limited display update (~30 Hz)
        if (need_display_update && absolute_time_diff_us(last_display_refresh, get_absolute_time()) >= 33000) {
            last_display_refresh = get_absolute_time();
            need_display_update = false;

            if (current_state == STATE_LISTENING) {
                draw_listening_screen(query_button_current_hold_ms(), active_confirmed_mode, flash_phase);
                need_display_update = true; // Continuous update while recording
            } else if (current_state == STATE_UPLOADING) {
                draw_uploading_screen();
            } else if (easter_egg_active) {
                draw_67_screen(flash_phase);
            } else {
                bool click_feedback = absolute_time_diff_us(get_absolute_time(), click_feedback_until) > 0;
                draw_dashboard(tick_count, led_state, current_mode_idx,
                               encoder_get_position(), encoder_switch_is_down(), click_feedback);
            }
        }

        sleep_ms(2);
    }

    return 0;
}
