---
name: hardware-experiment-workflow
description: >-
  Standardized protocol for physical hardware experimentation, safe electrical wiring,
  and milestone verification on Celestial Engine Mk.V. Use when guiding physical breadboard wiring,
  checking power rails, validating breakout pinouts, and documenting experiments.
---

# Hardware Experimentation and Safety Protocol

## Core Rules
1. **Never guess voltages**: Pico 2 W GPIO is strictly 3.3 V max. Applying 5 V directly to a GPIO pin damages the RP2350.
2. **Power Off First**: Always disconnect USB and all external power supplies before making or altering any jumper connections.
3. **Common Ground**: All interconnected subsystems (Pico, level shifters, LED rails, audio modules) must share a common GND.
4. **Capacitor Polarity**: Electrolytic bulk capacitors (e.g. 1000 uF >=10V) are polarized. Negative strip must connect to GND; positive leg connects to 5V.
5. **Physical Board Verification**: Breakout board silk-screen labels often vary between manufacturers. Always verify the printed labels on the user's specific module before powering.

## Step Protocol Structure
Every hardware guidance interaction must strictly contain:
1. **Goal**: Exactly one concise sentence.
2. **Parts**: Only the components needed for the current step.
3. **Power off**: Explicit confirmation to disconnect power.
4. **Wiring table**: Module pin -> Pico physical pin -> GPIO/Function -> Voltage/Notes.
5. **Code/build changes**: Minimal focused test firmware.
6. **Commands**: Exact commands run from project root or firmware dir.
7. **Expected result**: Specific observable LED/serial/display behavior.
8. **If it fails**: Ordered triage checklist.
9. **Documentation update**: Log status in `docs/experiments/` and `docs/hardware/inventory.md`.

## Experiment Documentation Format
Every experiment file in `docs/experiments/<NNN>-<name>.md` must record:
- Goal
- Hardware used
- Wiring diagram/table
- Build & flash instructions
- Expected observations
- Actual user observation
- Diagnostic notes & resolution
- Final status (`PASSED` or `FAILED`)
