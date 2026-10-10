# Experiment 005 — INMP441 I2S MEMS Microphone (P7)

Status: IN PROGRESS

## Goal
Verify I2S digital audio recording using PIO0 state machine and continuous DMA circular ring-buffer streaming at 16 kHz on RP2350 with the INMP441 MEMS microphone, and validate live voice sensitivity via the OLED live VU meter test menu.

## Hardware Required
- Raspberry Pi Pico 2 W (RP2350)
- INMP441 I2S MEMS omnidirectional microphone breakout
- 5x Dupont jumper wires (Female-Male or Male-Male)
- Micro-USB cable to PC

## Wiring Table

| INMP441 Pin | Pico Physical Pin | RP2350 GPIO / Function | Electrical Level / Notes |
| :--- | :---: | :--- | :--- |
| **VDD** | **Pin 36** (Row 5 Right, `j5`) | `3V3_OUT` | **3.3 V ONLY** (⚠️ NEVER CONNECT TO 5V VBUS!) |
| **GND** | **Pin 18** (Row 18 Left, `a18`) | Ground | Common Ground (0 V) |
| **L/R** | **Pin 13** (Row 13 Left, `a13`) | Ground | Tied to GND to select Left audio channel |
| **SCK** | **Pin 14** (Row 14 Left, `b14`) | `GP10` (PIO0 SM0) | 3.3 V I2S Bit Clock (2.048 MHz / 1.024 MHz) |
| **WS** | **Pin 15** (Row 15 Left, `b15`) | `GP11` (PIO0 SM0) | 3.3 V I2S Word Select / LRCLK (16 kHz) |
| **SD** | **Pin 16** (Row 16 Left, `b16`) | `GP12` (PIO0 SM0) | 3.3 V I2S Serial Data Out into Pico |

## Expected Behavior
1. In the menu, select `MIC TEST` and click the rotary encoder knob.
2. The OLED screen opens `[ MIC LIVE TEST ]` showing:
   - Live metrics: `LVL: %` and `PK: amplitude`.
   - Real-time animated VU meter bar with scale `0% 25% 50% 75% MAX`.
3. When speaking into the microphone or gently clapping/tapping, the VU meter bar actively jumps in real time.
4. When silent, the bar rests near 0%–5%.
5. Clicking the encoder knob confirms and exits back to the main menu.
6. When holding the QUERY button in normal mode, the recording screen also displays a live audio level bar underneath the timer.
