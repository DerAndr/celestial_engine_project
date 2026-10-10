# Experiment 006 — WS2812B 7-LED Core Ring (Milestone P4)

Status: PASSED (Rainbow menu & 67 vortex verified by user)

2026-10-10 review note: this log preserves the historical direct-ring pass. Current firmware offers seven LED EFFECTS and defaults to GALAXY VORTEX. PIO0 SM1 is the actual LED allocation. FIFO writes/latching, HSV interpolation and an enforced 18% cap were corrected; physical regression testing is pending in experiment 007. Phase 2 shifter instructions below do not establish a recorded shifter pass.

## Historical test goal
Verify driving the first WS2812B 7-LED core ring using PIO0 SM1 on RP2350 (GP15), validating:
1. 7-color rainbow selection menu (`[ LED RAINBOW ]`) driven by rotary encoder.
2. Fast spinning rainbow vortex animation on rotation sequence 67 (easter egg).
3. Polished, glitch-free audio synthesis via hardware DMA IRQ0 and higher amplitude scaling.

## Hardware Required
- Raspberry Pi Pico 2 W (RP2350)
- WS2812B 7-LED ring (already soldered with 3 wires: 5V, GND, DIN)
- Breadboard & jumper connections to Pico

### Phase 1: Direct Smoke Test (Initial Verification)
| WS2812B Pad | Pico Physical Pin | RP2350 Line | Breadboard Pin / Coordinate | Notes |
| :--- | :---: | :--- | :--- | :--- |
| **PWR5V** (5V) | **Pin 40** | `VBUS` (5.0 V) | Row 1 Right (`i1`) | 5V USB power rail |
| **GND** (0V) | **Pin 38** or **Pin 18** | Ground (0 V) | Row 3 Right (`i3`) or `c18` | Common Ground |
| **DIN** (Data) | **Pin 20** | `GP15` (PIO0 SM1) | Row 20 Left (`b20` or `a20`) | 800 kHz WS2812 NRZ data |

### Phase 2: Proposed level-shifter layout — physical verification pending
The SN74AHCT125N + 470R + 1000uF layout below is a proposal, not a recorded installation. It assumes moving QUERY to rows 21 & 23 to free rows 24–30. Actual assembly and chip markings must be checked before using these coordinates; do not infer that the move occurred from this table.

| Component / Pin | Wire Color | Where on Breadboard | Connects To | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **5V Rail Feed** | **Red** | Pico Pin 40 (`j1`) | **Red (+) Side Rail** | 5.0 V VBUS supply |
| **GND Rail Feed** | **Black** | Pico Pin 38 (`j3`) | **Blue (-) Side Rail** | Common Ground (0 V) |
| **Capacitor 1000uF (+)** | Long leg | **Red (+) Side Rail** | 5V Rail | Bulk decoupling |
| **Capacitor 1000uF (-)** | Short / stripe | **Blue (-) Side Rail** | GND Rail | Mind polarity! |
| **SN74 Pin 14 (VCC)** | **Red** | Row 24 Right (`g24` / `j24`) | **Red (+) Side Rail** | 5.0 V supply |
| **SN74 Pin 7 (GND)** | **Black** | Row 30 Left (`c30` / `a30`) | **Blue (-) Side Rail** | Ground |
| **SN74 Pin 1 (/1OE)** | **Black** | Row 24 Left (`c24` / `a24`) | **Blue (-) Side Rail** | Enable Channel 1 (Active LOW) |
| **SN74 Pin 2 (1A, IN)** | **Green** | Row 25 Left (`c25`) | Pico Pin 20 (`b20` / GP15) | 3.3 V PIO0 SM1 data signal |
| **Resistor 470R** | Axial leg 1 | Row 26 Left (`d26`) | SN74 Pin 3 (1Y, 5V OUT) | Signal damping |
| **Resistor 470R** | Axial leg 2 | Row 22 Left (`d22`) | Free row tie-point | Output tap point |
| **WS2812 DIN** | **White / Yellow** | Row 22 Left (`c22`) | Connects to Resistor leg 2 | 5.0 V safe data line |
| **WS2812 PWR5V** | **Red** | **Red (+) Side Rail** | 5V Rail | Ring 5V power |
| **WS2812 GND** | **Black** | **Blue (-) Side Rail** | GND Rail | Ring Ground |
| **QUERY Btn Leg 1** | — | Row 21 (`d21`, `f21`) | Tactile switch top | Shifted from row 22 |
| **QUERY Btn Leg 2** | — | Row 23 (`d23`, `f23`) | Tactile switch bottom | Shifted from row 24 |
| **QUERY Signal Wire** | **Orange** | Row 23 Left (`c23`) | Pico Pin 12 (`b12` / GP9) | Active LOW |
| **QUERY Ground Wire** | **Black** | Row 21 Right (`h21`) | **Blue (-) Side Rail** | Ground reference |

*(Pads `DO` and second `GND` on the 7-LED ring remain unconnected for this milestone).*

## Electrical Safety Notes
- Firmware strictly caps master brightness at ~18% (~45/255 per channel).
- Seven full-white LEDs at 18% have an estimated worst-case current of 75.6 mA using the project's 60 mA/LED model, before quiescent current. Lower-current animation observations are not a full-white maximum or a measured USB power budget.
- `GP15` uses PIO0 SM1; microphone uses PIO0 SM0, and speaker uses PIO1 SM0. The two PIO0 programs fit separate state machines and instruction memory; CYW43 currently claims from PIO2.

## Current behavior (0.7.1-P4; physical regression pending)

The main menu entry is `LED EFFECTS`, with seven selectable effects: COSMIC BREATHE, GALAXY VORTEX, PULSAR PULSE, SOLAR PLASMA, STARRY NIGHT, VOICE REACTIVE and RAINBOW WAVE. Default idle effect is GALAXY VORTEX. Only seven pixels are active, and the setter enforces an 18% brightness cap. The 67 Easter egg remains available. Validate revised behavior through [experiment 007](007-code-review-regression.md).

## Historical expected behavior
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
