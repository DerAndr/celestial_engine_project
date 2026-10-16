# Experiment 039 — Quartet of Clock Styles & R2-D2 Astromech Suite

**Date**: 2026-10-13  
**Objectives**: 
1. Expand the clock style submenu to **4 distinct representations** (including orientation-free spectral time encoding and a kinetic planetary orrery).
2. Implement a dedicated **R2-D2 Astromech sound suite** (synthesizer for whistles, chirps, panic screams, and droid banter, accessible in the sound effect player).

---

## 1. Four Clock Styles (`CHRONOS` Sub-Menu)

In `[05] CHRONOS` mode, the style submenu offers 4 independent algorithms for visualizing time across 43 LEDs and OLED:

| Style | Name | Type | Ring Mapping & Color Encoding |
|---|---|---|---|
| **0** | **`DIAL CHRONOS`** | Directional Dial | **Middle (12 LED)**: 12-hour dial with golden hour hand.<br>**Outer (24 LED)**: Emerald minute marker + flying diamond comet with azure tail.<br>**Inner (6 LED)**: Escapement tick.<br>**Center (1 LED)**: Amber 1 Hz pendulum.<br>*OLED*: Classic analog clock face with hands. |
| **1** | **`AETHER HOURGLASS`** | Concentric Accumulator (Orientation-free) | **Middle (12 LED)**: Number of illuminated LEDs = current hour ($1..12$) in radiant gold.<br>**Outer (24 LED)**: Liquid fill arc of minutes ($0..24$ LED) in cyan.<br>**Inner (6 LED)**: Decades of seconds ($0..6$ LED) in amethyst.<br>**Center (1 LED)**: 1 Hz pulse + minute flash.<br>*OLED*: 3 concentric liquid fill arcs. |
| **2** | **`CHROMA PRISMATIC`** | Spectral Clock (Orientation-free) | **Full Spectral Time Encoding**:<br>• **Middle (12 LED)**: All 12 LEDs uniformly glow in the **Color of Current Hour** (12 rainbow spectral tints from Ruby to Magenta). Allows instant reading from across the room at any angle!<br>• **Outer (24 LED)**: Ring tint shifts every 10 minutes across 6 decades (Cyan, Emerald, Gold, Plasma, Coral, Amethyst); arc proportionally fills with minutes ($0..24$ LED).<br>• **Inner (6 LED)**: 6 spectral gemstones (Red, Orange, Yellow, Green, Azure, Violet) illuminate for every 10 seconds of the minute.<br>• **Center (1 LED)**: 1 Hz chromatic heart shifting color every 10-second decade.<br>*OLED*: Spectral concentric dots + dynamic decade scale `[R O Y G C V]`. |
| **3** | **`CELESTIAL ORRERY`** | Kinetic Planetary Clock (Orientation-free) | **Concentric Planetary Orbits**:<br>• **Center (1 LED)**: Sun with solar corona rays.<br>• **Inner (6 LED)**: Breathing harmonic forcefield of the hour (gold morning, purple evening).<br>• **Middle (12 LED)**: Orbit of minute planet with moon, smoothly revolving.<br>• **Outer (24 LED)**: Second comet with fading ion tail completing a circuit in 60 seconds.<br>*OLED*: Planetary orrery with orbits and revolving celestial bodies. |

---

## 2. Clock Navigation and R2-D2 Demo Suite

### 2.1. Seamless Navigation in `[05] CHRONOS`
Simplified, unified interaction model:
1. **Switching Clock Style**:
   - While on `CHRONOS`, **pressing the encoder switch (SW)** opens style selection (`clock_style_edit_mode`).
   - Turning the encoder cycles through styles (`DIAL`, `HOURGLASS`, `CHROMA`, `ORRERY`) with **instantaneous preview on LED rings** and OLED!
   - Pressing **SW again** commits the style, chimes, and returns to normal time display.
2. **Setting Time (Hours and Minutes)**:
   - A dedicated 5th item `SET TIME` is available in the style list.
   - When entering `SET TIME`: turning adjusts hours (`[HH]:MM`), clicking moves to minutes (`HH:[MM]`), clicking again saves time and resets seconds to zero.
   - Pressing the `QUERY` button at any time cancels and exits without saving.

### 2.2. Sound Effects and R2-D2 Demo Suite (`[11] COSMIC FX`)
- Oracle dialect selection (`LINGUA`) is reserved for real spoken languages: `[EN]` (default), `[AUTO]`, `[DE]`, `[RU]`.
- All sound effects and the R2-D2 synth are consolidated into an interactive **`COSMIC FX` suite** in the main menu:
  - 10 Sci-Fi sound effects:
    1. `WARP JUMP` (hyperdrive spool and jump)
    2. `PULSAR BEACON` (rhythmic pulsar ping)
    3. `ALIEN SCANNER` (alien sensor arpeggio)
    4. `PHOTON LASER` (plasma blaster blast)
    5. `TRACTOR BEAM` (gravitational beam)
    6. `STARGATE PORTAL` (portal shimmer)
    7. `SUPERNOVA BASS` (deep sub-bass detonation)
    8. `R2-D2 HAPPY CHIRP` (joyful whistle and trill)
    9. `R2-D2 DROID CHATTER` (sassy astromech banter)
    10. `R2-D2 PANIC SCREAM` (alarm screech)
  - Controls: Turning encoder selects effect, clicking SW plays it, pressing `QUERY` returns to menu.

---

## 3. Verification

1. **Compilation**:
   - Built `firmware/build/celestial_firmware.uf2` with zero errors or warnings (18 LED effects `0..17`, multilingual selector, 4 clock styles).
2. **Automated Tests**:
   - `.\scripts\check-audio.ps1`: **17/17 PASS**.
   - `python -m unittest discover -s tests -p "test_*.py" -v`: **30/30 PASS**.
3. **Flashing to Physical Hardware**:
   - Loaded to Pico 2 W via `picotool 2.3.1 load -f -x`.
   - Serial monitor (`COM9`):
     - `Tick #33 | OLED: ONLINE | State: IDLE | Mode: [GUIDE] | Vol: 30% | Net: ONLINE (192.168.178.101)`
     - Command `L` confirmed cycling through all 18 LED animation modes.
