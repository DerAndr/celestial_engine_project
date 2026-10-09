#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/i2c.h"
#include "config.h"
#include "sh1106.h"

#if __has_include("wifi_credentials.h")
    #include "wifi_credentials.h"
    #define HAS_WIFI_CONFIG 1
#else
    #define HAS_WIFI_CONFIG 0
#endif

static bool oled_ready = false;

static void draw_dashboard(uint32_t tick, bool led_on, const char *status_msg) {
    if (!oled_ready) return;

    sh1106_clear();

    // Outer decorative border
    sh1106_draw_rect(0, 0, 128, 64, true);

    // Title Header
    sh1106_draw_string(8, 4, "CELESTIAL ENGINE", true);
    sh1106_draw_string(14, 14, "Mk.V  v0.2.0-P1", true);

    // Horizontal separator
    sh1106_draw_line(4, 24, 123, 24, true);

    // Operational telemetry
    char buf[24];
    snprintf(buf, sizeof(buf), "STATUS: %s", status_msg);
    sh1106_draw_string(6, 28, buf, true);

    sh1106_draw_string(6, 38, "I2C: 0x3C (400kHz)", true);

    snprintf(buf, sizeof(buf), "TICK: #%lu | LED:%s", (unsigned long)tick, led_on ? "ON" : "OFF");
    sh1106_draw_string(6, 48, buf, true);

    sh1106_update();
}

int main(void) {
    stdio_init_all();

    // Small delay to allow USB CDC terminal to attach
    sleep_ms(2000);

    printf("\n========================================\n");
    printf("   %s\n", PROJECT_NAME);
    printf("   Firmware: %s\n", FIRMWARE_VERSION);
    printf("   Target:   Raspberry Pi Pico 2 W (RP2350)\n");
    printf("========================================\n\n");

    LOG_INFO("SYS", "Configuring I2C1 for SH1106 OLED (GP2 SDA, GP3 SCL, 400 kHz)...");
    i2c_init(OLED_I2C_INST, OLED_I2C_BAUDRATE);
    gpio_set_function(PIN_OLED_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_OLED_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_OLED_SDA);
    gpio_pull_up(PIN_OLED_SCL);

    // Scan I2C bus to detect connected display
    int detected_addr = sh1106_i2c_scan(OLED_I2C_INST);
    uint8_t target_addr = (detected_addr > 0) ? (uint8_t)detected_addr : OLED_I2C_ADDR;

    LOG_INFO("OLED", "Attempting initialization at I2C address 0x%02X...", target_addr);
    if (sh1106_init(OLED_I2C_INST, target_addr)) {
        oled_ready = true;
        LOG_INFO("OLED", "SH1106 OLED initialized successfully!");
        draw_dashboard(0, true, "INIT_OK");
    } else {
        LOG_ERROR("OLED", "SH1106 initialization failed or no acknowledge received!");
    }

    LOG_INFO("SYS", "Initializing CYW43 wireless architecture...");
    if (cyw43_arch_init()) {
        LOG_ERROR("SYS", "CYW43 initialization failed!");
    } else {
        LOG_INFO("SYS", "CYW43 initialized successfully.");
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        cyw43_arch_enable_sta_mode();
    }

    LOG_INFO("SYS", "P1 loop active. Rendering telemetry to OLED and USB CDC at 1 Hz...");

    bool led_state = true;
    uint32_t heartbeat_count = 0;

    while (true) {
        led_state = !led_state;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
        heartbeat_count++;

        // Update OLED screen
        draw_dashboard(heartbeat_count, led_state, oled_ready ? "P1_ACTIVE" : "OLED_ERR");

        LOG_INFO("HEARTBEAT", "Tick #%lu | LED: %s | OLED: %s | State: P1_OLED",
                 (unsigned long)heartbeat_count,
                 led_state ? "ON" : "OFF",
                 oled_ready ? "ONLINE" : "OFFLINE");

        sleep_ms(1000);
    }

    return 0;
}
