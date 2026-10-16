# Experiment 038 — Astronomical Chronometer: Two Clock Styles (Directional Dial & Orientation-Independent Hourglass)

**Date**: 2026-10-13  
**Objectives**: Implement two visual clock representations across 3 concentric rings (43 LEDs) and the center jewel:
1. **Directional Astronomical Dial (`DIAL CHRONOS`)** with hands and flying comet.
2. **Orientation-Independent Fluid Accumulator (`AETHER HOURGLASS`)**, readable at any tilt angle without relying on an upright 12 o'clock orientation.

---

## 1. Physical Layout (43 LEDs)

- **Center Stone (Index 0)**: 1 LED at geometric center.
- **Inner Jewel Ring (Indices 1..6)**: 6 LEDs.
- **Middle Ring (Indices 7..18)**: 12 LEDs.
- **Outer Ring (Indices 19..42)**: 24 LEDs.

---

## 2. Style 1: "DIAL CHRONOS" (Directional Dial)

- **Center Stone (0)**: 1 Hz pulsing pendulum (amber/topaz) + minute flash.
- **Inner Ring (1..6)**: 10-second decades (amethyst halo).
- **Middle Ring (7..18)**: 12-hour dial with golden hour hand, smoothly advancing with elapsed minutes. Cardinal points (12, 3, 6, 9) illuminated in cobalt sapphire.
- **Outer Ring (19..42)**:
  - Emerald minute marker with subpixel smoothing.
  - Flying second comet (diamond-white head with 3-pixel fading azure tail) completing circuit every 60 seconds.
- **OLED Screen**: Classic circular analog clock face with hour, minute, and second hands.

---

## 3. Style 2: "AETHER HOURGLASS" (Orientation-Independent Accumulator)

Does not require knowing which way is "up". Time is read directly by the **proportion and count of illuminated LEDs** on each ring:

| Ring | Role | Reading Principle | Color Palette |
|---|---|---|---|
| **Center Stone** (0) | Pendulum | Rhythmic 1 Hz pulse + minute flash | Warm Amber / White Supernova |
| **Middle Ring** (7..18, 12 LEDs) | **Hours** | Exactly **H** LEDs illuminate ($1..12$). The next LED progressively brightens with minutes. Simply count illuminated LEDs from any angle! | Imperial Solar Gold (`RGB: 255, 175, 20`) |
| **Outer Ring** (19..42, 24 LEDs) | **Minutes** | Progressive liquid fill arc from 0 to 24 LEDs (1 LED = 2.5 min): 6 LEDs = 15 min (quarter ring), 12 LEDs = 30 min (half ring), 18 LEDs = 45 min. Leading drop pulses with time. | Liquid Emerald / Cyan (`RGB: 0, 220, 160`) |
| **Inner Ring** (1..6, 6 LEDs) | **Decades of Seconds** | 1 to 6 LEDs illuminate (each LED = 10 seconds). | Celestial Amethyst / Purple (`RGB: 170, 35, 220`) |

- **OLED Screen**: 3 concentric fluid arcs expanding proportionally with elapsed time + digital `HH:MM:SS` + `RADIAL ACC` indicator.

---

## 4. Interactive Controls (Bourns PEC11R)

In `[05] CHRONOS` mode:
1. Clicking encoder switch (SW) enters 3-phase configuration:
   - **Phase 0 `[STYLE]`**: Turning encoder instantly swaps between `AETHER HOURGLASS` and `DIAL CHRONOS` with live preview on rings and OLED.
   - **Phase 1 `[SET HH]`**: Turning adjusts hours ($\pm 1$ h).
   - **Phase 2 `[SET MM]`**: Turning adjusts minutes ($\pm 1$ min).
   - **Phase 3**: Clicking confirms settings, chimes, and returns to active clock mode.
2. Pressing `QUERY` button at any time cancels/closes configuration without changes.
3. Both clock styles are also selectable in the general `[08] RUNE LEDS` animation menu.

---

## 5. Verification

- **C Native Driver Tests** (`.\scripts\check-audio.ps1`): 17/17 PASS.
- **Python Regressions** (`test_regressions.py`, `test_brain_transport.py`): 30/30 PASS.
- **Build & Flash**: `celestial_firmware.uf2` flashed directly to Pico 2 W via `picotool`.
- **Serial Monitor (COM9)**: Confirmed clean boot, OLED updates, Wi-Fi connectivity, and all 16 animation presets (`L0..L15`).
