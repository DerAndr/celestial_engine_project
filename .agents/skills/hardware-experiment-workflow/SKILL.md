---
name: hardware-experiment-workflow
description: >-
  Provides the standardized protocol for physical hardware experimentation, safe electrical wiring,
  breadboard power-rail placement, and milestone verification on Celestial Engine Mk.V.
  Use this skill when the user asks for physical breadboard wiring guidance, power rail checks,
  capacitor placement, breakout pinout validation, or hardware experiment documentation.
---

# Hardware Experimentation and Safety Protocol

## Core Electrical & Breadboard Rules
1. **Never guess voltages**: Pico 2 W (`RP2350`) GPIO is strictly **3.3 V max**. Applying 5 V directly to any GPIO pin permanently damages the RP2350.
2. **Power Off First**: Always disconnect USB and all external power supplies before making or altering any jumper connections or inserting components.
3. **Strict Power Domain Isolation**:
   - **Staff Power**: 5V USB power bank into Pico `VBUS` (`Pin 40`) + `1000 µF` electrolytic decoupling capacitor across the 5V (`+`) and GND (`−`) bus rails.
   - **Wearable Pi 5 Power**: Completely independent `Geekworm X1209` UPS + 4× `Molicel INR18650-M35A` cells. Never mix or cross-wire these supplies.
4. **Common Star Ground**: All interconnected staff subsystems (Pico 2 W, `SN74AHCT125N` level shifter, 43 `WS2812B` LEDs, `MAX98357A` amplifier, `INMP441` mic, `SH1106` OLED, `PEC11R` encoder) must share a common `GND` bus.
5. **Capacitor Polarity & Physical Breadboard Placement (`VTBB3`)**:
   - Electrolytic bulk capacitors (`1000 µF >= 10V`) are polarized: **Long leg (`+`, unstriped side)** goes to the **Red `+` (5V VBUS) rail**; **Short leg (`−`, white/grey striped side with minus symbols)** goes to the **Blue/Black `−` (GND) rail**.
   - **Physical Placement Rule**: Never assume the exact breadboard holes directly opposite `Pin 40 (VBUS)` or `Pin 38 (GND)` are unoccupied. Always explain clearly that the red `+` and blue/black `−` side bus rails run continuously along the entire length of the breadboard (`Rows 1–30`), so the user can insert the capacitor into **any free adjacent pair of `+` and `−` holes** along that same side rail (ideally close to the jumper wires feeding the WS2812B LED rings and MAX98357A amplifier).
6. **Physical Board Verification**: Breakout board silk-screen labels often vary between manufacturers. Always verify the printed labels on the user's actual module before powering.

## Step Protocol Structure
Every hardware guidance interaction must strictly contain:
1. **Goal**: Exactly one concise sentence.
2. **Parts**: Only the components needed for the current step.
3. **Power off**: Explicit confirmation to disconnect power before touching wires.
4. **Wiring table & physical hole guidance**: Module pin -> Pico physical pin / breadboard rail -> GPIO/Function -> Voltage/Notes (including practical placement on the `+`/`−` bus rail when primary row holes are full).
5. **Code/build changes**: Minimal focused test firmware.
6. **Commands**: Exact commands run from project root or `firmware/` dir.
7. **Expected result**: Specific observable LED/serial/display/audio behavior.
8. **If it fails**: Ordered triage checklist.
9. **Documentation update**: Log status in `docs/experiments/` and `docs/hardware/inventory.md` / `docs/wiring/pinout.md`.

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
