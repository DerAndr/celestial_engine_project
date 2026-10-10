#ifndef CELESTIAL_CONFIG_H
#define CELESTIAL_CONFIG_H

#include <stdio.h>

/* ==========================================================================
 * CELESTIAL ENGINE Mk.V — Hardware Configuration & Pin Map
 * Target: Raspberry Pi Pico 2 W (RP2350)
 * ========================================================================== */

/* Project Metadata */
#define FIRMWARE_VERSION       "0.7.2-AUDIO-DIAG"
#define PROJECT_NAME           "Celestial Engine Mk.V"

/* Audio fault isolation: boot muted, no automatic startup sounds. */
#define AUDIO_DIAGNOSTIC_BOOT   1
#define AUDIO_DEFAULT_VOLUME   0
#define AUDIO_I2S_SLOT_BITS    32
#define AUDIO_PCM16_PEAK       8000.0f  // Digital ceiling in equivalent 16-bit units (-12 dBFS headroom, clean, safe acoustic volume).
#define AUDIO_CONTINUOUS_CLOCKS 1     // Continuous clean I2S bit/word clocks; eliminates clock-stop DC bias, PLL unlock and desync.

/* OLED Display (SH1106 I2C) */
#define PIN_OLED_SDA           2   // GP2 (Pin 4) -> I2C1
#define PIN_OLED_SCL           3   // GP3 (Pin 5) -> I2C1
#define OLED_I2C_INST          i2c1
#define OLED_I2C_ADDR          0x3C
#define OLED_I2C_BAUDRATE      400000

/* Rotary Encoder (Bourns PEC11R) */
#define PIN_ENCODER_A          6   // GP6 (Pin 9)
#define PIN_ENCODER_B          7   // GP7 (Pin 10)
#define PIN_ENCODER_SW         8   // GP8 (Pin 11)

/* Query Button (Momentary NO) */
#define PIN_QUERY_BTN          9   // GP9 (Pin 12)

/* INMP441 MEMS Microphone (I2S Input) */
#define PIN_MIC_SCK            10  // GP10 (Pin 14)
#define PIN_MIC_WS             11  // GP11 (Pin 15)
#define PIN_MIC_SD             12  // GP12 (Pin 16)

/* WS2812 LED Core (via SN74AHCT125N Level Shifter) */
#define PIN_WS2812_DATA        15  // GP15 (Pin 20)
#define TOTAL_LED_COUNT        43  // 7 (Core) + 12 (Inner) + 24 (Outer) = 43 LEDs
#define LED_RING_COUNT         43  // Active 3-ring concentric array
#define LED_BRIGHTNESS_LIMIT   18  // Percent cap for the current USB-powered prototype.

/* MAX98357A Mono Amplifier (I2S Output) */
#define PIN_SPK_BCLK           16  // GP16 (Pin 21)
#define PIN_SPK_LRC            17  // GP17 (Pin 22)
#define PIN_SPK_DIN            18  // GP18 (Pin 24)

/* Diagnostic Logging Macros */
#define LOG_TAG(level, tag, fmt, ...) \
    printf("[%s] [" tag "] " fmt "\n", level, ##__VA_ARGS__)

#define LOG_INFO(tag, fmt, ...)  LOG_TAG("INFO",  tag, fmt, ##__VA_ARGS__)
#define LOG_WARN(tag, fmt, ...)  LOG_TAG("WARN",  tag, fmt, ##__VA_ARGS__)
#define LOG_ERROR(tag, fmt, ...) LOG_TAG("ERROR", tag, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(tag, fmt, ...) LOG_TAG("DEBUG", tag, fmt, ##__VA_ARGS__)

#endif /* CELESTIAL_CONFIG_H */
