# Experiment 003 — QUERY Push-to-Talk Button (P3)

Status: COMPLETED (PASSED)

## 2026-10-10 review correction

The historical `UPLOADING` screen and 1.2-second delay below were a UI simulation, not audio capture or Pi transport. Firmware 0.7.1 now labels QUERY as a microphone test and returns directly from LISTENING to IDLE on release. Both input edges require 30 ms stability. Software regressions pass; physical revalidation is pending in experiment 007.

## Goal
Verify the normally-open button on GP9 (Pico Pin 12), stable hold/release detection and local `IDLE -> LISTENING -> IDLE` interaction with a microphone level meter and hold-time display. Retained query capture and upload are future P10 work.

## Hardware Required
- Raspberry Pi Pico 2 W (on breadboard c1–h20)
- SH1106 1.3" OLED (on breadboard rows 26–29)
- Bourns PEC11R encoder (connected via flying leads to GP6/7/8)
- 1x 4-pin momentary tactile push button (seated across rows 22 and 24)
- 2x Dupont jumper wires (Orange, Black)
- Micro-USB cable

## Verified Wiring Table (Ground Truth)

| Component Terminal | Where on Breadboard | Pico Physical Pin | RP2350 GPIO / Function | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **Button Legs** | `d24`, `f24`, `d22`, `f22` | — | 4-pin tactile switch straddling gutter | Verified mechanical seating |
| **Signal Wire (Orange)** | **Ряд 24 слева (`c24`)** | **Pin 12** (Ряд 12 слева `b12`) | **GPIO 9** (`PIN_QUERY_BTN`) | Active LOW (Internal pull-up) |
| **Ground Wire (Black)** | **Ряд 22 справа (`h22`)** | **Pin 18** (Ряд 18 слева `b18`) | **GND** | Ground Reference (Diagonal connection) |

## Current interaction (0.7.1-P4; physical regression pending)
- **Hold QUERY**: After 30 ms stability, enter `LISTENING`; OLED shows `[ MIC ] LISTENING`, a hold timer and the live mic meter.
- **Release QUERY**: After 30 ms stability, return to `IDLE`; serial explicitly states that audio capture/upload is not implemented.
- **Never implement QUERY as a toggle**: Keep momentary hold-to-talk operation. The target MVP adds retained recording and Pi transport in a later milestone.

## Historical tests & milestones (0.4.0-P3)
- [x] Connect QUERY button diagonally between `c24` (Pico GP9 / Pin 12) and `h22` (Pico GND / Pin 18)
- [x] Build and flash firmware `celestial_firmware.uf2` v0.4.0-P3
- [x] Initial state: `IDLE` with clean OLED dashboard, active mode, rotation counter, and `QRY=UP`
- [x] Press and hold QUERY: state switches to `LISTENING`, elapsed-hold timer ticks on OLED (historically labelled recording)
- [x] Release QUERY: simulated `UPLOADING` screen for 1.2s, then `IDLE`; hold duration logged to serial, no audio transmitted
- [x] Encoder mode navigation continues to work seamlessly alongside QUERY button

## Result
Historical PASSED — 4-pin tactile QUERY button verified with diagonal pinout (`c24` to `b12`, `h22` to `b18`). Button transitions and OLED hold telemetry were confirmed by the user; speech capture/upload were not validated. Revised behavior awaits [experiment 007](007-code-review-regression.md).
