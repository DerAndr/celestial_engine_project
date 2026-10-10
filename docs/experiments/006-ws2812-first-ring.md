# Experiment 006 — WS2812B 7-LED Core Ring (Milestone P4)

Status: PASSED (Rainbow menu & 67 vortex verified by user)

## Goal
Verify driving the first WS2812B 7-LED core ring using PIO0 SM1 on RP2350 (GP15), validating:
1. 7-color rainbow selection menu (`[ LED RAINBOW ]`) driven by rotary encoder.
2. Fast spinning rainbow vortex animation on rotation sequence 67 (easter egg).
3. Polished, glitch-free audio synthesis via hardware DMA IRQ0 and higher amplitude scaling.

## Hardware Required
- Raspberry Pi Pico 2 W (RP2350)
- WS2812B 7-LED ring (already soldered with 3 wires: 5V, GND, DIN)
- Breadboard & jumper connections to Pico

## Wiring Table

| WS2812B Pad | Pico Physical Pin | RP2350 Line | Breadboard Pin / Coordinate | Notes |
| :--- | :---: | :--- | :--- | :--- |
| **PWR5V** (5V) | **Pin 40** | `VBUS` (5.0 V) | Row 1 Right (`i1`) | 5V USB power rail |
| **GND** (0V) | **Pin 38** or **Pin 18** | Ground (0 V) | Row 38 Right (`i38`) or `c18` | Common Ground |
| **DIN** (Data) | **Pin 20** | `GP15` (PIO2 SM0) | Row 20 Left (`b20` or `a20`) | 800 kHz WS2812 NRZ data |

*(Pads `DO` and second `GND` on the 7-LED ring remain unconnected for this milestone).*

## Electrical Safety Notes
- Firmware strictly caps master brightness at ~18% (~45/255 per channel).
- All 7 LEDs at this level consume only ~20–25 mA total (completely safe for Pico VBUS / USB supply).
- `GP15` is configured as a dedicated PIO output on `pio2`, completely independent from audio (`pio1`) and mic (`pio0`).

## Expected Behavior
1. **Startup Chime**: Sounds whisper-quiet, gentle and clean.
2. **Idle Mode**: 7 LEDs show a slow, subtle cosmic breathing glow in the active mode color.
3. **LED Rainbow Menu**:
   - Turn encoder to `LED RAINBOW` and click the knob.
   - Screen displays `[ RAINBOW CORE ]` with `COLOR X / 7` and color name in large text.
   - Turning the knob cycles through the 7 rainbow colors:
     1. Red
     2. Orange
     3. Yellow
     4. Green
     5. Cyan
     6. Blue
     7. Violet
   - All 7 LEDs instantly update to match the selected color!
   - Clicking the knob confirms the color and exits back to main dashboard.
4. **Sequence 67 Easter Egg**:
   - Rotate the encoder to position 67 (or turn 6 -> 7).
   - The OLED flashes `!!! 67 !!! SPECIAL UNLOCK`.
   - The 7 LEDs rapidly spin in a dazzling, swirling rainbow vortex!
   - Pressing the encoder switch exits the easter egg and returns to normal operation.
