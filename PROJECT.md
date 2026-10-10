# CELESTIAL ENGINE Mk.V

## Mission
Build a working interactive AI staff quickly from the hardware already purchased. This is an execution project, not a hardware-selection exercise.

The coding agent must guide the user physically, one testable prototype at a time, and maintain the repository as an engineering notebook.

## Current implementation — 2026-10-10

Current firmware **0.7.2-AUDIO-DIAG** targets Pico 2 W / RP2350. The physical array is configured for 43 LEDs across 3 concentric rings (7 Core + 12 Inner + 24 Outer; the defective 16-LED ring was removed). Audio dynamic range has been calibrated with a smooth perceptual gain curve down to 1% (-80 dBFS whisper quiet) without coil heating (`AUDIO_CONTINUOUS_CLOCKS = 1`). All 27 regression unit tests and native C audio checks pass.

| Area | Current behavior / status |
|---|---|
| Display and controls | SH1106 on I2C1; encoder A/B decoded in GPIO IRQ; QUERY press/release require 30 ms stability |
| QUERY | Local microphone test: IDLE -> LISTENING -> IDLE; live meter and hold timer |
| LEDs | Active 3-ring array: 43 pixels (7 + 12 + 24) on GP15; 18% brightness cap; dynamic audio-reactive sync and cosmic effects |
| Microphone | 16 kHz live metering, 256-sample circular DMA window; PIO0 SM0 |
| Speaker | 16 kHz frames, PCM24 in 32-bit channel slots, PIO1 SM0; 13 world chiptune tracks, 7 cosmic SFX; whisper-quiet calibrated |
| Mission Control | Web dashboard at http://localhost:5050 with one-click UF2 flasher, live telemetry stream, and presets |
| Milestones | P0–P4/P6/P7 validated; 43-LED multi-ring verified; P8 brain integration next |

See the [audio experiment 011](docs/experiments/011-audio-volume-fix.md), [three rings experiment 010](docs/experiments/010-three-rings-cosmic-effects.md), [pinout](docs/wiring/pinout.md), [inventory](docs/hardware/inventory.md) and [interactive map](docs/interactive_map.html).

## System architecture
There are two physically separate systems:

1. **STAFF** — Raspberry Pi Pico 2 W embedded controller handling controls, display, LEDs, microphone, speaker and Wi-Fi.
2. **WEARABLE BRAIN** — Raspberry Pi 5 handling STT, agent/LLM/tools, TTS and orchestration.

Communication is wireless over Wi-Fi. Do not introduce a physical data cable between Pi 5 and the staff.

For now use the built-in Wi-Fi hardware already present on Pico 2 W and Raspberry Pi 5. Do not add another Wi-Fi adapter unless actual testing demonstrates a need.

## Halloween MVP
The target interaction below is not yet implemented end to end:

1. Hold QUERY.
2. Staff enters LISTENING and records speech.
3. Release QUERY.
4. Staff sends audio to Pi 5 over Wi-Fi.
5. Pi performs STT -> agent -> TTS.
6. Staff enters SPEAKING and plays the response.
7. OLED and LEDs represent system state throughout.

Reliability matters more than enclosure polish.

## Purchased / selected hardware
### Staff
- Raspberry Pi Pico 2 W with factory headers (RP2350, Wi-Fi/Bluetooth)
- 1.3-inch 128x64 SH1106 I2C OLED
- Bourns PEC11R-4225F-S0024 rotary encoder, 24 detents/pulses, push switch
- Momentary normally-open QUERY button for prototype
- INMP441 I2S MEMS microphone
- MAX98357A I2S mono Class-D amplifier
- 4 ohm / 3 W mini speaker
- WS2812B ring set: 7 + 12 + 16 + 24 LEDs = 59 LEDs total
- SN74AHCT125N level shifter(s)
- ~470 ohm series resistor(s) for WS2812 data
- ~1000 uF, >=10 V electrolytic capacitor(s)
- Velleman VTBB3 400-hole breadboard
- Dupont/jumper wiring
- 22 AWG red/black wire for power
- 26 AWG multicolor wire for signals

### LED dimensions
- 7 LED: OD ~23 mm
- 12 LED: OD 37 mm, ID 23 mm
- 16 LED: OD 45 mm, ID 32 mm
- 24 LED: OD 66 mm, ID 52 mm, thickness ~6.7 mm

Important geometry: the 16-LED ring can pass through the 52 mm opening of the 24-LED ring. The 12-LED ring cannot pass through the 32 mm opening of the 16-LED ring. Final mechanical geometry is deliberately not fixed yet.

### Wearable brain
Already owned:
- Raspberry Pi 5
- official Raspberry Pi Active Cooler

Portable power subsystem planned/ordered separately:
- Geekworm X1209 V1.0
- Geekworm X12-A1 + X12-A3
- 4 x Molicel INR18650-M35A 3500 mAh flat-top

This battery system belongs to the wearable Raspberry Pi 5, NOT the staff.

## Target UI concept
OLED modes:
- ORACLE
- GUIDE
- SCANNER
- CANDY DETECTOR
- SYSTEM

Encoder:
- rotate: choose mode / adjust current parameter
- press: SELECT / confirm

QUERY:
- press and hold: start listening/recording
- release: stop and submit
- never implement as a toggle

## Staff state machine
Use one authoritative state machine for OLED, LEDs and interaction logic:

BOOT -> CONNECTING -> IDLE -> LISTENING -> UPLOADING -> PROCESSING -> SPEAKING -> IDLE

Exceptional states:
- OFFLINE
- ERROR

This is the target flow. Current state definitions are in `firmware/src/staff_state.h`, with local orchestration in `firmware/src/main.c`; unified state-driven LEDs and transport states remain incomplete.

## Electrical constraints
- Pico GPIO is 3.3 V.
- Never apply 5 V directly to a Pico GPIO.
- WS2812B power is 5 V.
- Use SN74AHCT125N for reliable Pico 3.3 V -> WS2812 data-level conversion.
- Suggested path: Pico GPIO -> SN74AHCT125 -> ~470 ohm -> WS2812 DIN.
- Use bulk capacitance near LED power (~1000 uF, >=10 V).
- 59 WS2812 LEDs have a theoretical worst-case current near 3.54 A at 5 V at maximum full-white output. Firmware must impose brightness/current limits.
- Do not route full LED-array current through a Pico GPIO or assume the Pico board is the final high-current power path.
- Independently powered interconnected electronics require common ground where appropriate.
- Observe electrolytic capacitor polarity.
- Disconnect power before rewiring power rails.

## Development policy
The hardware has already been selected. Do not redesign the project or recommend replacements simply because another MCU/module is easier.

Do not suggest replacing Pico with ESP32/Arduino, replacing the OLED, changing microphone/amplifier, or changing the LED architecture unless the existing component is proven defective or technically incapable of the requirement.

Small supporting purchases are acceptable only when testing proves they are required.

## How the coding agent must work
For each physical step:
1. Inspect repository/current experiment status.
2. Establish which relevant parts have physically arrived.
3. State exactly which components to take out.
4. Provide exact wiring, including Pico physical pin number and GPIO number.
5. Explicitly identify GND / 3.3 V / 5 V/VBUS.
6. Warn before risky power connections.
7. Implement only the minimum code for that experiment.
8. Give exact build/flash/run commands.
9. State exactly what the user should observe.
10. Troubleshoot from observed behavior rather than assumptions.
11. Record successful wiring and findings in docs.
12. Only then proceed to the next experiment.

Do not dump the entire finished firmware at the user. The user is physically building alongside the agent.

## Master pinout requirement
Before peripherals are permanently integrated, create and maintain `docs/wiring/pinout.md` as the single source of truth.

Plan pins globally for:
- SH1106 I2C
- encoder A/B/SW
- QUERY
- INMP441 I2S
- MAX98357A I2S
- WS2812 DATA / SN74AHCT125
- serial/debugging needs

Do not assign GPIO independently in unrelated examples. Account for RP2350/Pico 2 W capabilities rather than blindly copying RP2040 tutorials.

## Prototype sequence
### P0 — Pico alive [PASSED]
Pico + computer only. Establish toolchain, build, flash, serial logs, onboard LED and Wi-Fi scan/connect.

### P1 — OLED [PASSED]
Add SH1106. Display `CELESTIAL ENGINE Mk.V` and boot/status information.

### P2 — Encoder [PASSED]
Add PEC11R. Implement rotation, press and debounce. Navigate the modes and adjust parameters.

### P3 — QUERY [PASSED]
Add momentary NO button. Log/display QUERY DOWN and QUERY UP; connect this to LISTENING state.

### P4 — WS2812 Core Ring & Celestial Animations [PASSED / VERIFIED]
Direct GP15 driving verified. Added seven animation modes (`[ LED EFFECTS ]`): Cosmic Breathe, Galaxy Vortex, Pulsar Pulse, Solar Plasma, Starry Night, Voice Reactive and Rainbow Wave. The revised frame, color and brightness handling awaits experiment 007.

### P5 — Full LED core [BLOCKED BY AUDIO REGRESSION]
After experiment 007 passes, confirm the outstanding rings and validate the actual level shifter and power setup, then connect 7/12/16/24 rings. Prefer a documented daisy chain if practical. Abstract physical offsets behind logical ring IDs CORE/HALO/ORBIT/OUTER. Full-array wiring, logical rings and current limiting are pending.

### P6 — Microphone [PASSED / VERIFIED]
INMP441 verified on RP2350 via PIO0 SM0 circular DMA. Live voice reaction verified in telemetry and integrated into LED voice-reactive animation mode.

### P7 — Speaker [REVALIDATION FAILED — DEBUGGING]
MAX98357A + 4 ohm/3 W speaker using PIO1 SM0. Historical output pass is retained, but the user now reports distortion and excessive loudness at V10 after removing GAIN. PCM precision and volume-preview defects were fixed; the current diagnostic build boots muted. Speaker/amp damage is not established. Complete experiment 009 before continuing integration.

### P8 — Integrated staff [IN PROGRESS]
Run OLED + controls + LEDs + microphone + speaker under the central state machine.

### P9 — Pi link
Use built-in Wi-Fi. Start with PING/PONG and heartbeat, then state messages and audio transport.

### P10 — End-to-end voice
QUERY hold -> capture -> release -> upload -> Pi STT -> agent -> TTS -> return audio -> playback -> IDLE.

### P11 — Portable operation
Integrate/test Pi 5 portable power separately. Do not confuse it with staff power.

### P12 — Mechanical integration
Only after electronics are reliable: soldered board, connectors as actually needed, strain relief, mounting, instrument pod, celestial-core structure, decorative controls and finish.

## Initial protocol vocabulary
Keep it simple initially:
- HELLO
- HEARTBEAT
- PING / PONG
- QUERY_START
- QUERY_END
- AUDIO
- STATE
- RESPONSE_START
- RESPONSE_AUDIO
- RESPONSE_END
- ERROR

Do not over-engineer the protocol before basic communication works.

## Repository expectations
See AGENTS.md for agent behavior. Maintain hardware inventory, master pinout and experiment logs continuously.
