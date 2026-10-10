# Experiment 008 — WS2812 Full Array Integration (Milestone P5)

**Status: IN PROGRESS (Milestone P5: Full 59-LED 4-Ring Array: 7 + 12 + 16 + 24)**

## Ring Inventory & Hardware Geometry
The full celestial array consists of 4 concentric WS2812B rings (59 LEDs total):
- **Ring 1 (Core):** 7 LEDs (OD ~23 mm) — *Verified on GP15*.
- **Ring 2 (Inner Halo):** 12 LEDs (OD 37 mm, ID 23 mm).
- **Ring 3 (Orbit):** 16 LEDs (OD 45 mm, ID 32 mm).
- **Ring 4 (Outer Horizon):** 24 LEDs (OD 66 mm, ID 52 mm).

Total LED count: $7 + 12 + 16 + 24 = 59\text{ LEDs}$.

## Pad Topology & Power Architecture
Each outer breakout ring features:
- **2 x PWR (5V)** pads: Internally bridged power bus for pass-through daisy chain.
- **2 x GND** pads: Internally bridged ground plane for pass-through daisy chain.
- **1 x DIN** pad: Serial data input.
- **1 x DO** pad: Serial data output to next ring.

### Complete 59-LED Wiring Map
- **Data (Daisy Chain):**
  `Pico GP15` -> `7-LED DIN` | `7-LED DO` -> `12-LED DIN` | `12-LED DO` -> `16-LED DIN` | `16-LED DO` -> `24-LED DIN` | `24-LED DO` (Unconnected).
- **Power (Pass-through Bus):**
  - Ring 1 (7-LED): PWR & GND direct to Breadboard rails.
  - Ring 2 (12-LED): PWR1 & GND1 direct to Breadboard rails.
  - Ring 3 (16-LED): PWR1 & GND1 from Ring 2 (PWR2 & GND2).
  - Ring 4 (24-LED): PWR1 & GND1 from Ring 3 (PWR2 & GND2).
- **Firmware Status:**
  `LED_RING_COUNT = 59`, `LED_BRIGHTNESS_LIMIT = 18%` (safe USB current cap).
