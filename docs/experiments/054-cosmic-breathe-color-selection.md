# Experiment 054: Cosmic Breathe Color Palette Selection

## Date
2026-10-26

## Objective
Implement dynamic color palette selection for the **Cosmic Breathe** (`LED_ANIM_BREATHE`) animation across firmware, physical OLED/encoder UI, serial CLI, and remote control protocol.

## Features & Implementation
1. **Curated Celestial Palette (`firmware/src/leds.h` & `firmware/src/leds.c`)**:
   - Expanded `RAINBOW_COLOR_COUNT` to 10 curated celestial colors:
     - `0: CYAN` — Cosmic Aether Core (`{0, 220, 255}`)
     - `1: AZURE` — Deep Space Azure Blue (`{0, 60, 255}`)
     - `2: VIOLET` — Nebula Violet / Amethyst (`{180, 0, 255}`)
     - `3: MAGENTA` — Cyberpunk Magenta / Rose (`{255, 15, 140}`)
     - `4: CRIMSON` — Solar Flare Crimson (`{255, 0, 0}`)
     - `5: AMBER` — Sunburst Amber / Orange (`{255, 85, 0}`)
     - `6: GOLD` — Radiant Celestial Gold (`{255, 200, 15}`)
     - `7: EMERALD` — Matrix Emerald Green (`{0, 255, 60}`)
     - `8: STARLIGHT` — Pure Starlight White (`{255, 255, 255}`)
     - `9: SPECTRUM` — Continuous chromatic smooth hue cycling via `hsv_to_rgb`
   - Added `leds_get_rainbow_index()` and `leds_get_rainbow_count()`.

2. **OLED Screen & Rotary Encoder UI (`firmware/src/main.c`)**:
   - **Dashboard Card (`MODE_LED_CORE`)**: Shows current animation and active color (`COLOR: [CYAN]`).
   - **Animation Selector (`led_edit_mode`)**: When scrolling to `COSMIC BREATHE`, clicking the encoder switch transitions directly into `led_color_edit_mode`.
   - **Color Tune Screen (`led_color_edit_mode`)**: Displays `[ BREATHE COLOR ]` with palette counter `(1/10)` and `< COLOR >`. Rotating the encoder immediately updates the breath color in real time. Clicking the switch confirms and saves the selection with a harmonic chime.
   - **Query Button Cancel**: Pressing the Query button safely exits without changing state.

3. **Serial CLI & Remote Control (`main.c` & `net_client.h`)**:
   - Added serial command `R` / `R<n>`: cycles or sets color directly (0..9).
   - Added authenticated Companion App remote command `case 'R':` for wireless color control.

## Verification
- Firmware compiled with `cmake` (exit 0) and flashed via `picotool.exe` to Pico 2 W.
- Verified serial cycling: `R` cycled through `[AZURE]`, `[VIOLET]`, `[CYAN]`, and `[SPECTRUM]`.
- Live telemetry and Wi-Fi connection verified stable (`10.169.248.64` linked to `PHONE BRAIN`).
