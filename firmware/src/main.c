#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "config.h"

#if __has_include("wifi_credentials.h")
    #include "wifi_credentials.h"
    #define HAS_WIFI_CONFIG 1
#else
    #define HAS_WIFI_CONFIG 0
#endif

static int scan_result_cb(void *env, const cyw43_ev_scan_result_t *result) {
    if (result) {
        printf("  [WIFI SCAN] SSID: %-24s | RSSI: %3d dBm | Channel: %2d\n",
               result->ssid, result->rssi, result->channel);
    }
    return 0;
}

int main(void) {
    stdio_init_all();

    // Small delay to allow USB CDC terminal to attach if connected immediately
    sleep_ms(2500);

    printf("\n========================================\n");
    printf("   %s\n", PROJECT_NAME);
    printf("   Firmware: %s\n", FIRMWARE_VERSION);
    printf("   Target:   Raspberry Pi Pico 2 W (RP2350)\n");
    printf("========================================\n\n");

    LOG_INFO("SYS", "Initializing CYW43 wireless architecture...");
    if (cyw43_arch_init()) {
        LOG_ERROR("SYS", "CYW43 initialization failed!");
        while (true) {
            tight_loop_contents();
        }
    }
    LOG_INFO("SYS", "CYW43 initialized successfully.");

    // Turn onboard LED on
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    LOG_INFO("SYS", "Onboard LED turned ON.");

    // Enable station mode for Wi-Fi scanning
    cyw43_arch_enable_sta_mode();

    LOG_INFO("WIFI", "Starting Wi-Fi scan for nearby networks...");
    cyw43_wifi_scan_options_t scan_options = {0};
    int scan_err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result_cb);
    if (scan_err < 0) {
        LOG_WARN("WIFI", "Failed to start Wi-Fi scan: %d", scan_err);
    } else {
        // Wait for scan to finish
        while (cyw43_wifi_scan_active(&cyw43_state)) {
            sleep_ms(100);
        }
        LOG_INFO("WIFI", "Wi-Fi scan completed.");
    }

#if HAS_WIFI_CONFIG
    LOG_INFO("WIFI", "Attempting connection to SSID: '%s'...", WIFI_SSID);
    int connect_res = cyw43_arch_wifi_connect_timeout_ms(
        WIFI_SSID,
        WIFI_PASSWORD,
        CYW43_AUTH_WPA2_AES_PSK,
        30000
    );
    if (connect_res == 0) {
        LOG_INFO("WIFI", "Connected successfully! IP: %s",
                 ip4addr_ntoa(netif_ip4_addr(netif_default)));
    } else {
        LOG_ERROR("WIFI", "Connection failed with error code: %d", connect_res);
    }
#else
    LOG_INFO("WIFI", "No wifi_credentials.h found. To test connection, copy wifi_credentials.h.example.");
#endif

    LOG_INFO("SYS", "P0 validation loop active. Pulsing onboard LED at 1 Hz...");

    bool led_state = true;
    uint32_t heartbeat_count = 0;

    while (true) {
        led_state = !led_state;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);

        LOG_INFO("HEARTBEAT", "Tick #%lu | LED: %s | State: P0_ALIVE",
                 (unsigned long)++heartbeat_count,
                 led_state ? "ON" : "OFF");

        sleep_ms(1000);
    }

    return 0;
}
