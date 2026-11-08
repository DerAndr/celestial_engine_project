#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
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
#include "net_client.h"
#include "ui_screens.h"
#include "event_dispatcher.h"

static void on_mic_stream_chunk(const int16_t *samples, size_t count) {
    if (net_client_is_connected()) {
        net_client_send_audio_chunk(samples, count);
    }
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
        oled_ready = true;
        ui_set_oled_ready(true);
        sh1106_set_page_yield_callback(mic_update);
        LOG_INFO("OLED", "SH1106 OLED initialized successfully!");
    } else {
        oled_ready = false;
        ui_set_oled_ready(false);
        LOG_ERROR("OLED", "SH1106 initialization failed or no response!");
    }

    LOG_INFO("SYS", "Initializing CYW43 wireless architecture...");
    if (cyw43_arch_init()) {
        cyw43_ready = false;
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

    event_dispatcher_init();

    bool boot_voice_pending = AUDIO_STARTUP_SOUNDS && !AUDIO_DIAGNOSTIC_BOOT;
    if (AUDIO_DIAGNOSTIC_BOOT) {
        LOG_INFO("AUDIO", "Diagnostic boot: muted, continuous I2S, startup sounds disabled. V5 then D for a 1s sine; A for stats; S to stop.");
    } else if (AUDIO_STARTUP_SOUNDS) {
        LOG_INFO("SYS", "Celestial Engine Online! Playing harmonic chime and voice announcement.");
        audio_play_chime();
    } else {
        LOG_INFO("SYS", "Celestial Engine Online! Silent startup; use VOLUME to enable audio.");
    }

    while (true) {
        watchdog_update();

        // Continuous audio buffer streaming & mic processing
        audio_update();
        if (boot_voice_pending && current_state == STATE_IDLE && !audio_is_playing()) {
            audio_play_voice(VOICE_ONLINE);
            boot_voice_pending = false;
        }
        mic_update();

        // Core 0 event dispatching (Controls, State Machine, Display, Serial CDC, UDP Telemetry)
        event_dispatcher_tick();

        sleep_ms(2);
    }

    return 0;
}
