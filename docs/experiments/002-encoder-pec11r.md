# Experiment 002 — Bourns PEC11R Rotary Encoder (P2)

Status: COMPLETED (PASSED)

2026-10-10 software review: A/B transitions are now decoded in a GPIO edge IRQ so synchronous OLED refreshes cannot hide a complete quadrature cycle. The previously verified two-substep threshold and rotation direction are retained. Accumulated delta is consumed atomically; switch polling remains in the main loop. Physical fast/slow rotation revalidation is pending in experiment 007.

The five-mode menu below is the historical P2 baseline. Current firmware has ten menu entries, listed in [PROJECT.md](../../PROJECT.md); the revised firmware still needs [experiment 007](007-code-review-regression.md).

## Goal
Connect the Bourns PEC11R-4225F-S0024 rotary encoder (Channel A, Channel B, Push Switch) to RP2350 (GP6, GP7, GP8), implement quadrature decoding and debouncing, and navigate the 5 Celestial Engine modes on the OLED display.

## Hardware Required
- Raspberry Pi Pico 2 W (on breadboard c1–h20)
- SH1106 1.3" OLED (on breadboard rows 26–29)
- Bourns PEC11R-4225F-S0024 rotary encoder (bare 5-pin component, flying leads)
- 5x Dupont Female-to-Male jumper wires
- Micro-USB cable

## Bourns PEC11R Pin Layout
- **3-pin side (Encoder signals)**:
  - **Pin A** (left): Quadrature Channel A -> GP6 (Pico Pin 9)
  - **Pin C** (center): Common Ground -> GND (Pico Pin 8)
  - **Pin B** (right): Quadrature Channel B -> GP7 (Pico Pin 10)
- **2-pin side (Push switch)**:
  - **Pin SW1**: Push switch contact -> GP8 (Pico Pin 11)
  - **Pin SW2**: Push switch ground -> GND (Pico Pin 13)

## Wiring Table (Confirmed Working)

| Encoder Terminal | Physical Encoder Pin | Pico Location (Column `b`) | RP2350 GPIO / Function | Purpose |
| :--- | :--- | :--- | :--- | :--- |
| **Signal A** | 3-pin side: **left pin** | **Pin 9** (Row 9 left `b9`) | GPIO 6 (`PIN_ENCODER_A`) | Quadrature signal A (pull-up enabled) |
| **Common C (GND)** | 3-pin side: **center pin** | **Pin 8** (Row 8 left `b8`) | Ground (GND) | Shared ground for signals A and B |
| **Signal B** | 3-pin side: **right pin** | **Pin 10** (Row 10 left `b10`) | GPIO 7 (`PIN_ENCODER_B`) | Quadrature signal B (pull-up enabled) |
| **Switch SW1** | 2-pin side: **pin 1** | **Pin 11** (Row 11 left `b11`) | GPIO 8 (`PIN_ENCODER_SW`) | Shaft push switch (pull-up enabled, shorts to GND) |
| **Switch SW2** | 2-pin side: **pin 2** | **Pin 13** (Row 13 left `b13`) | Ground (GND) | Ground reference for push switch |

## Tests & Milestones
- [x] Implement quadrature decoder and mode navigation in firmware (`firmware/src/encoder.c`)
- [x] Build firmware `celestial_firmware.uf2` v0.3.0-P2
- [x] Connect PEC11R off-board via 5 Dupont wires directly to Pico pins 8, 9, 10, 11, 13
- [x] Rotating encoder successfully detected (CW / CCW detents updating position and modes)
- [x] Rotating 6 -> 7 activates full-screen '!!! 67 !!!' Easter egg
- [x] Pressing encoder push switch confirms mode on OLED and logs to serial (SW pin verified)

## Result
PASSED — Bourns PEC11R fully operational. Quadrature rotation with Gray decoding, dynamic detent thresholds, debounced push switch, and 5-mode menu navigation with full-screen Easter egg confirmed working by user.
