# Experiment 033 — Unified Spiral Vortex on Recording & Concentric Radar Circles on Data Reception

## Date
2026-10-11

## Objective
1. Implement a unified logarithmic whirlpool / spiral animation across all concentric rings simultaneously during voice recording (`STATE_LISTENING`), with voice-reactive speed and luminous flare.
2. Implement concentric radar data reception ripples (`LED_ANIM_DATA_RECV`) during server query / response processing (`STATE_PROCESSING`), where rings flash inward sequentially as solid concentric circles (Ring 2 -> Ring 1 -> Ring 0 -> Center) like cybernetic data ingestion.

## Implementation Details
1. **[firmware/src/leds.h](file:///c:/projects/personal/hardware-01/celestial_engine_project/firmware/src/leds.h)**:
   - Added `LED_ANIM_DATA_RECV = 9` to `led_anim_mode_t`.
2. **[firmware/src/leds.c](file:///c:/projects/personal/hardware-01/celestial_engine_project/firmware/src/leds.c)**:
   - Added `"DATA RECEPTION"` to `ANIM_NAMES`.
   - **`render_vortex(uint32_t tick, uint8_t mic_level_pct)`**:
     - Modeled two 180°-symmetric Archimedean / logarithmic spiral arms rotating in the same direction across all concentric layers simultaneously (`delta_r` offsets: Ring 0 = 0.0 rad, Ring 1 = 0.80 rad, Ring 2 = 1.65 rad, Ring 3 = 2.50 rad).
     - Center LED pulses as the singularity eye of the whirlpool.
     - Rotation speed and arm ridge flare accelerate dynamically with `mic_level_pct` as the user speaks into the microphone.
   - **`render_data_recv(uint32_t tick)`**:
     - Cascading concentric circular rings: Ring 2 (outer 24 LEDs) -> Ring 1 (middle 12 LEDs) -> Ring 0 (inner 6 LEDs) -> Center (core LED).
     - Every LED on a given ring layer flashes at the exact same brightness and color in unison, producing solid glowing concentric rings of light.
     - Alternates between Sub-Etha neon cyan (`0, 255, 240`) and emerald matrix telemetry (`0, 255, 130`), with a diamond-white pulse on the core LED upon packet absorption.
3. **[firmware/src/main.c](file:///c:/projects/personal/hardware-01/celestial_engine_project/firmware/src/main.c)**:
   - Connected `current_state == STATE_LISTENING` -> `LED_ANIM_VORTEX`.
   - Connected `current_state == STATE_PROCESSING` -> `LED_ANIM_DATA_RECV`.

## Verification
- `scripts/check-audio.ps1`: All 17 native C harness tests passed.
- `cmake --build firmware/build`: Compiled cleanly with 0 errors and 0 warnings.
- `picotool load -f -x firmware/build/celestial_firmware.uf2`: Successfully loaded into RP2350 flash and rebooted.
- COM9 serial heartbeat confirmed:
  `[INFO] [HEARTBEAT] Tick #21 | OLED: ONLINE | State: IDLE | Mode: [DON'T PANIC] | Vol: 35% | Net: ONLINE (192.168.178.101) | Brain: READY | QRY: UP`.
