#ifndef CELESTIAL_CONFIG_H
#define CELESTIAL_CONFIG_H

#include <stdio.h>

/* ==========================================================================
 * CELESTIAL ENGINE Mk.V — Hardware Configuration & Pin Map
 * Target: Raspberry Pi Pico 2 W (RP2350)
 * ========================================================================== */

/* Project Metadata */
#ifndef CELESTIAL_FOUR_RINGS
#define CELESTIAL_FOUR_RINGS 0
#endif
#ifndef CELESTIAL_RING_EXPERIMENT
#define CELESTIAL_RING_EXPERIMENT 0
#endif
#if CELESTIAL_RING_EXPERIMENT
#define FIRMWARE_VERSION       "1.0.10-RING-DIAGNOSTIC"
#elif CELESTIAL_FOUR_RINGS
#define FIRMWARE_VERSION       "1.0.14-FOUR-RINGS"
#else
#define FIRMWARE_VERSION       "1.1.0-RELEASE"
#endif
#define PROJECT_NAME           "Celestial Engine Mk.V"
#define FIRMWARE_CODENAME      "HALLOWEEN-ORACLE"
#define FIRMWARE_CHIP_TARGET   "RP2350 (ARM-S)"
#define FIRMWARE_BUILD_DATE    "28 OCT 2026"

/* Audio Configuration: Official Raspberry Pi 16-bit I2S Stereo with Hardware Sleep */
#define AUDIO_DIAGNOSTIC_BOOT   0        // Normal application UI and peripherals.
#if CELESTIAL_FOUR_RINGS
#define AUDIO_STARTUP_SOUNDS    0        // Retain the current four-ring build's quiet startup.
#define AUDIO_DEFAULT_VOLUME    55       // Default playback level (55%)
#else
#define AUDIO_STARTUP_SOUNDS    1        // Play gentle startup chime and announcement at boot.
#define AUDIO_DEFAULT_VOLUME    55       // Default playback level (55%)
#endif
#define AUDIO_VOLUME_RANGE_DB   24.0f    // V1..V100: -24..0 dB curve gives a loud, clear baseline.
#define AUDIO_I2S_SLOT_BITS     16       // Standard 16-bit stereo I2S (32 BCLKs per frame, 512 kHz).
#define AUDIO_PCM16_PEAK        24000.0f // Elevated digital ceiling closer to 16-bit full scale for MAX98357A.
#define AUDIO_CONTINUOUS_CLOCKS 0        // Stop BCLK during silence (>80 ms) for total amplifier shutdown (<0.6 uA).

/* OLED Display (SH1106 I2C) */
#define PIN_OLED_SDA           2   // GP2 (Pin 4) -> I2C1
#define PIN_OLED_SCL           3   // GP3 (Pin 5) -> I2C1
#define OLED_I2C_INST          i2c1
#define OLED_I2C_ADDR          0x3C
#define OLED_I2C_BAUDRATE      400000

/* Rotary Encoder (Bourns PEC11R) */
#define ENCODER_TRANSITIONS_PER_STEP 4 // PEC11R-4225F-S0024: one full quadrature cycle per detent.
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
#define LED_BASE_CHAIN_COUNT   43  // Verified chain: 7 (Core) + 12 (Inner) + 24 (Outer).
#if CELESTIAL_FOUR_RINGS
// Reserve all 16 tail slots; the common-DI jumper has unverified address consumption.
#define LED_EXPERIMENT_TAIL_FRAMES 16
#define LED_RING_COUNT         (LED_BASE_CHAIN_COUNT + LED_EXPERIMENT_TAIL_FRAMES)
#define LED_BRIGHTNESS_LIMIT   4   // Low-brightness, removable fourth-ring experiment.
#define LED_RESET_US           300 // Conservative inter-frame latch for mixed ring revisions.
#define LED_BOOT_COLOR_MS      2000 // Red, green, blue on all 59 slots before normal effects.
#define LED_DIAG_COLOR_MS      5000 // Experimental RAINBOW: static pure RGB on spare, five seconds each.
#else
#define LED_RING_COUNT         LED_BASE_CHAIN_COUNT
#define LED_BRIGHTNESS_LIMIT   18  // Percent cap for the current USB-powered prototype.
#define LED_RESET_US           300 // Safe latch for all WS2812 revisions
#endif
#define TOTAL_LED_COUNT        LED_RING_COUNT

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
