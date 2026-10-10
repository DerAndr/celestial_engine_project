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
1. Physical I2S streaming over PIO1 SM0 verified with clear audio from the 4Ω 3W dynamic speaker.
2. Master volume scale updated to perceptual quadratic curve (`vol_norm^2 * 1200`), with startup volume set to gentle 12% (whisper-quiet, non-piercing).
3. Equalization (Fletcher-Munson compensation) implemented: frequencies >250 Hz are smoothly attenuated (`f_comp = powf(250 / f, 0.70)`), eliminating ear fatigue and resonance screech on small speakers.
4. Soft harmonic timbre: synthesized tone blends 85% fundamental sine with 15% 2nd harmonic and 12 ms raised-cosine envelope, eliminating harsh square-wave edges and clicks.
5. Digitized Speech Engine: 4 voice clips embedded in flash (16 kHz 8-bit mono):
   - `VOICE_ONLINE` ("Celestial Engine online")
   - `VOICE_READY` ("System ready")
   - `VOICE_ORACLE` ("Voice Oracle activated")
   - `VOICE_WARNING` ("Celestial anomaly detected")
   - Plays automatically at power-on and after voice recording, plus selectable via new `VOICE ORACLE` menu mode.
6. Extended 8-Bit Chiptune Jukebox: expanded to 6 full iconic compositions (Super Mario Bros, Tetris Korobeiniki, Doom E1M1, Zelda Overworld, Star Wars Imperial March, Mega Man 2).
7. In-Jukebox Controls: turning encoder adjusts volume directly in real-time (2% steps) with live gauge; clicking switch skips to next track; QUERY button exits.
8. Default LED Idle Animation: updated to `GALAXY VORTEX` (smooth rotating comet orbiting LEDs 1..6 with pulsing core).

## Result
PASSED — Audio output subsystem calibrated for gentle, pleasant acoustics, robotic speech synthesis, and full user volume control.

