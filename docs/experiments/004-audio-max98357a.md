# Experiment 004 — MAX98357A I2S Audio Output (P6)

Status: COMPLETED (PASSED)

## Goal
Verify I2S digital audio output using PIO1 state machine and double-buffered DMA streaming on RP2350 with the MAX98357A Class-D amplifier driving a 4Ω 3W speaker, and implement master volume control through the rotary encoder UI.

## Hardware Required
- Raspberry Pi Pico 2 W (RP2350)
- MAX98357A I2S Mono 3.2W Class-D Amplifier Breakout
- 4Ω 3W dynamic speaker
- 5x Dupont jumper wires
- Micro-USB cable to PC

## Wiring Table

| MAX98357A Pin | Wire | Pico Physical Pin | RP2350 GPIO / Function | Electrical Level / Notes |
| :--- | :--- | :--- | :--- | :--- |
| **VIN** | Red | **Pin 40** (Row 1 Right, `j1`) | `VBUS` (5V USB) | **5.0 V Supply** (Amp supply; never connect to Pico GPIO) |
| **GND** | Black | **Pin 23** (Row 18 Right, `j18`) | Ground | Common Ground (0 V) |
| **DIN** | Blue | **Pin 24** (Row 17 Right, `j17`) | `GP18` (PIO1 SM0) | 3.3 V I2S Serial Data Out |
| **BCLK** | Yellow | **Pin 21** (Row 20 Right, `j20`) | `GP16` (PIO1 SM0) | 3.3 V I2S Bit Clock (1.024 MHz) |
| **LRC** | Green | **Pin 22** (Row 19 Right, `j19`) | `GP17` (PIO1 SM0) | 3.3 V I2S Left/Right Clock (16 kHz) |
| **GAIN** | — | — | Unconnected | Default 9 dB gain |
| **SD** | — | — | Unconnected | Default stereo downmix |
| **Speaker + / -** | — | Screw Terminal (+ / -) | — | Clamped to 4Ω 3W speaker terminals |

## Observed Behavior & Verification
1. Physical I2S streaming over PIO1 SM0 verified with loud, clear audio from the 4Ω 3W dynamic speaker.
2. Default output amplitude reduced from 6000 (100%) to a gentle 1750 (25%) default for comfortable near-field listening.
3. Master volume adjustment integrated into rotary encoder menu (`MODE_VOLUME`):
   - Selecting `VOLUME` mode and clicking the encoder enters the dedicated interactive Volume Setting screen.
   - Turning encoder adjusts volume in 5% steps (0% to 100%) with live graphical gauge bar.
   - Live tone feedback plays at each volume step with matching pitch and amplitude.
   - Clicking encoder confirms volume, plays harmonic chime, and returns to main dashboard.
   - Live volume indicator `V:25%` and rotation counter `ROT:+0` rendered cleanly on OLED dashboard.

## Result
PASSED — Audio output subsystem fully operational. Ready for microphone (P7) or LED ring (P4/P5).
