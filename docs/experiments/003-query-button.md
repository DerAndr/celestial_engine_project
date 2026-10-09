# Experiment 003 — QUERY Push-to-Talk Button (P3)

Status: COMPLETED (PASSED)

## Goal
Connect a momentary normally-open (NO) button to GP9 (Pico Pin 12), implement push-to-talk debouncing, and drive the staff state machine: hold QUERY to enter `LISTENING` (with recording duration counter on OLED), release QUERY to transition through `UPLOADING` to `IDLE`.

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

## Interaction Specification (Halloween MVP)
- **Hold QUERY**: Staff transitions to `LISTENING` state; OLED displays `[ REC ] LISTENING` and live elapsed timer in seconds and hundredths.
- **Release QUERY**: Staff transitions to `>>> UPLOADING >>>`, logs speech capture duration, then returns cleanly to `IDLE`.
- **Never implement QUERY as a toggle**: Verified momentary hold-to-talk operation.

## Tests & Milestones
- [x] Connect QUERY button diagonally between `c24` (Pico GP9 / Pin 12) and `h22` (Pico GND / Pin 18)
- [x] Build and flash firmware `celestial_firmware.uf2` v0.4.0-P3
- [x] Initial state: `IDLE` with clean OLED dashboard, active mode, rotation counter, and `QRY=UP`
- [x] Press and hold QUERY: state immediately switches to `LISTENING`, live recording timer ticks on OLED
- [x] Release QUERY: state switches to `UPLOADING` for 1.2s, then returns to `IDLE`; duration logged to serial
- [x] Encoder mode navigation continues to work seamlessly alongside QUERY button

## Result
PASSED — 4-pin tactile QUERY button verified working with diagonal pinout (`c24` to `b12`, `h22` to `b18`). Push-to-talk state machine transitions and live OLED recording telemetry confirmed working by user.
