#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/i2c.h"
#include "config.h"
#include "sh1106.h"
#include "encoder.h"

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
static bool easter_egg_active = false;
static int32_t prev_encoder_pos = 0;
static absolute_time_t click_feedback_until;

static void draw_67_screen(bool flash_phase) {
    if (!oled_ready) return;

    sh1106_clear();

    if (flash_phase) {
        // Solid inverted full-screen splash
        sh1106_fill_rect(0, 0, 128, 64, true);
        sh1106_draw_string(14, 6, "=== CELESTIAL ===", false);
        sh1106_draw_string_scaled(4, 22, "!!! 67 !!!", 2, false);
        sh1106_draw_string(16, 44, "* SPECIAL UNLOCK *", false);
        sh1106_draw_string(18, 54, "PRESS SW TO EXIT", false);
    } else {
        // Double-bordered framing splash
        sh1106_draw_rect(0, 0, 128, 64, true);
        sh1106_draw_rect(2, 2, 124, 60, true);
        sh1106_draw_string(14, 6, "=== CELESTIAL ===", true);
        sh1106_draw_string_scaled(4, 22, "!!! 67 !!!", 2, true);
        sh1106_draw_string(16, 44, "* SPECIAL UNLOCK *", true);
        sh1106_draw_string(18, 54, "PRESS SW TO EXIT", true);
    }

    sh1106_update();
}

static void draw_dashboard(uint32_t tick, bool led_on, int sel_idx, int32_t pos, bool sw_down, bool click_feedback) {
    if (!oled_ready) return;

    sh1106_clear();

    // Outer decorative border
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Title Header
    sh1106_draw_string(6, 3, "CELESTIAL ENGINE", true);
    sh1106_draw_string(106, 3, "P2", true);
    sh1106_draw_line(2, 11, 125, 11, true);

    // Large Rotation Counter (Scale 3 font)
    char pos_str[16];
    snprintf(pos_str, sizeof(pos_str), "%ld", (long)pos);

    int num_len = (int)strlen(pos_str);
    int num_width = num_len * 18 - 3;
    int num_x = (128 - num_width) / 2;
    if (num_x < 4) num_x = 4;

    sh1106_draw_string_scaled(num_x, 15, pos_str, 3, true);

    // Live raw pin diagnostic line: A, B, SW
    bool raw_a = false, raw_b = false, raw_sw = false;
    encoder_get_raw_pins(&raw_a, &raw_b, &raw_sw);

    char pin_str[24];
    snprintf(pin_str, sizeof(pin_str), "PINS: A=%d B=%d SW=%d", raw_a ? 1 : 0, raw_b ? 1 : 0, raw_sw ? 1 : 0);
    sh1106_draw_string(6, 40, pin_str, true);

    // Bottom telemetry separator
    sh1106_draw_line(2, 49, 125, 49, true);

    if (click_feedback || sw_down) {
        // High-contrast inverted bar when clicked/pressed
        sh1106_fill_rect(2, 51, 124, 11, true);
        sh1106_draw_string(14, 53, ">> CLICK DETECTED! <<", false);
    } else {
        char mode_str[24];
        snprintf(mode_str, sizeof(mode_str), "MODE: %s", MODE_NAMES[sel_idx]);
        sh1106_draw_string(6, 53, mode_str, true);
        sh1106_draw_string(102, 53, "SW:UP", true);
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

    LOG_INFO("SYS", "P2 Active. Turn knob to change numbers! Sequence 6 -> 7 unlocks '!!! 67 !!!'");

    uint32_t tick_count = 0;
    bool led_state = true;
    bool flash_phase = false;
    click_feedback_until = get_absolute_time();
    absolute_time_t last_heartbeat = get_absolute_time();
    absolute_time_t last_display_refresh = get_absolute_time();
    absolute_time_t last_flash_toggle = get_absolute_time();
    bool need_display_update = true;

    while (true) {
        // Fast sampling of encoder inputs
        encoder_update();

        int32_t delta = encoder_get_delta();
        if (delta != 0) {
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

            LOG_INFO("ENCODER", "Pos: %ld (prev: %ld) | Step: %s | Mode: [%s]",
                     (long)curr_pos,
                     (long)prev_encoder_pos,
                     (delta > 0) ? "CW (+)" : "CCW (-)",
                     MODE_NAMES[current_mode_idx]);

            prev_encoder_pos = curr_pos;
            need_display_update = true;
        }

        if (encoder_switch_clicked()) {
            if (easter_egg_active) {
                easter_egg_active = false;
                LOG_INFO("EASTER_EGG", "Switch clicked: Exiting '!!! 67 !!!' screen back to counter.");
            } else {
                active_confirmed_mode = current_mode_idx;
                click_feedback_until = delayed_by_ms(get_absolute_time(), 600);
                LOG_INFO("ENCODER", ">>> SWITCH CLICKED! Active Mode Confirmed: [%s] <<<",
                         MODE_NAMES[active_confirmed_mode]);
            }
            need_display_update = true;
        }

        // Heartbeat tick at 1 Hz
        if (absolute_time_diff_us(last_heartbeat, get_absolute_time()) >= 1000000) {
            last_heartbeat = get_absolute_time();
            tick_count++;
            led_state = !led_state;
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);

            bool a_val, b_val, sw_val;
            encoder_get_raw_pins(&a_val, &b_val, &sw_val);

            LOG_INFO("HEARTBEAT", "Tick #%lu | LED: %s | Pos: %ld | Pins: A=%d B=%d SW=%d | 67_Active: %s",
                     (unsigned long)tick_count,
                     led_state ? "ON" : "OFF",
                     (long)encoder_get_position(),
                     a_val ? 1 : 0, b_val ? 1 : 0, sw_val ? 1 : 0,
                     easter_egg_active ? "YES" : "NO");
            need_display_update = true;
        }

        // Flash timer for the 67 screen (toggles every 500 ms)
        if (easter_egg_active && absolute_time_diff_us(last_flash_toggle, get_absolute_time()) >= 500000) {
            last_flash_toggle = get_absolute_time();
            flash_phase = !flash_phase;
            need_display_update = true;
        }

        // Rate-limited display update (~30 Hz)
        if (need_display_update && absolute_time_diff_us(last_display_refresh, get_absolute_time()) >= 33000) {
            last_display_refresh = get_absolute_time();
            need_display_update = false;

            if (easter_egg_active) {
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
