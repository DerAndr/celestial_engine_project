# Experiment 051: 2D Polar Kinematics Engine Completion — Archimedean Spiral & Sacred Runes

**Date:** 2026-10-26  
**Status:** PASSED (Compiled cleanly to `firmware/build/celestial_firmware.uf2`, 23 animations active)  
**Target Hardware:** Raspberry Pi Pico 2 W (RP2350) + 43 WS2812B LEDs (7→12→24) on GP15 + Android Companion App  

---

## 1. Objective
Complete the full 2D Polar Kinematics lighting engine specification for Celestial Engine Mk.V across the 43 concentric LEDs (Center: 1, Ring 0: 6, Ring 1: 12, Ring 2: 24), implementing:
1. `LED_ANIM_ARCHIMEDES_SPIRAL` (#21): Continuous polar spiral arms $\theta(r, t) = \omega t - k \cdot r$ swirling from the center core outward with electric cyan-to-violet gradients and pulsating golden singularity.
2. `LED_ANIM_SACRED_POLYGON` (#22): Clockwork geometric runes rotating at harmonic gear ratios: Triquetra (3 vertices, $\omega$) on Ring 0, Hexagram / Merkaba (6 vertices, $-\omega/2$) on Ring 1, and Dodecagram (12 vertices, $\omega/4$) on Ring 2, with diamond white-gold resonance flash at alignment nodes.

---

## 2. Polar Coordinate System & Mathematical Formulation

| Ring Layer | LED Index Range | Radius ($r$) | LED Count | Angular Pitch ($\Delta\theta$) | Kinematic Geometry |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Singularity Core** | `0` | $0.0$ | 1 | — | Pulsating warm plasma ember |
| **Ring 0 (Inner)** | `1..6` | $1.0$ | 6 | $60^\circ$ | Triquetra (3-point equilateral rune) |
| **Ring 1 (Mantle)** | `7..18` | $2.0$ | 12 | $30^\circ$ | Hexagram / Merkaba (6-point sacred star) |
| **Ring 2 (Outer Rim)** | `19..42` | $3.2$ | 24 | $15^\circ$ | Dodecagram (12-point astral seal) |

### Continuous Polar Formulae:
- **Archimedean Dual Spiral**:
  $$\theta_{\text{arm}}(r, t) = (\omega t - k \cdot r) \pmod{360^\circ}$$
  $$\Delta\theta_i = \min(|\theta_i - \theta_{\text{arm}}|, 360^\circ - |\theta_i - \theta_{\text{arm}}|)$$
  $$\text{Amplitude} = \cos^2\left(\frac{\pi \cdot \Delta\theta_i}{2 \cdot W_r}\right)$$
- **Harmonic Sacred Ratios**:
  $$\text{Ring 0 (Triquetra)}: \theta_{\text{tri}}(t) = +3.6^\circ / \text{tick}$$
  $$\text{Ring 1 (Hexagram)}: \theta_{\text{hex}}(t) = -1.8^\circ / \text{tick} \quad (-\omega/2)$$
  $$\text{Ring 2 (Dodecagram)}: \theta_{\text{dod}}(t) = +0.9^\circ / \text{tick} \quad (+\omega/4)$$

---

## 3. Code & Architecture Updates

1. **`firmware/src/leds.h`**:
   - Added `LED_ANIM_ARCHIMEDES_SPIRAL = 21` and `LED_ANIM_SACRED_POLYGON = 22`.
   - Updated `LED_ANIM_COUNT` to 23.
2. **`firmware/src/leds.c`**:
   - Added `"ARCHIMEDES SPIRAL"` and `"SACRED POLYGON"` to `ANIM_NAMES`.
   - Implemented `render_archimedes_spiral(uint32_t tick)` and `render_sacred_polygon(uint32_t tick)`.
   - Updated dispatch switch in `leds_update()`.
3. **Android Companion App**:
   - `StaffModels.kt`: Added `ARCHIMEDES_SPIRAL` and `SACRED_POLYGON` to `LedAnimation` enum.
   - `CelestialProphecy.kt`: Added regex parsing for `ARCHIMEDES`, `SPIRAL_ARM`, `SACRED`, `POLYGON`, `RUNE`, `MERKABA`.
4. **`docs/tasks.md`**:
   - Marked 2D Polar Kinematics Engine as completed.
   - Marked Android Background Service as completed.

---

## 4. Verification & Build Artifacts
- **Firmware ELF/UF2**: Compiled cleanly with zero errors/warnings via `cmake --build firmware/build --target celestial_firmware`.
- **Target File**: `firmware/build/celestial_firmware.uf2` (1,110,016 bytes).
- Ready for flashing via `scratch/flash_pico.py` or USB BOOTSEL.
