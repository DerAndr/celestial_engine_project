# CELESTIAL ENGINE Mk.V

## Mission
Build a working interactive AI staff quickly from the hardware already purchased. This is an execution project, not a hardware-selection exercise.

The coding agent must guide the user physically, one testable prototype at a time, and maintain the repository as an engineering notebook.

## Checkpoint and next priorities — 2026-10-11

The user repaired the 16-position ring with a jumper bypassing one failed LED; it is installed last in the physical **7→12→24→16** chain. The damaged position remains dark. The user confirms that buggy bright flashes of light on the 4th ring are no longer visible, and inactive backgrounds in PULSAR/VOICE/VORTEX remain dark.

Work order: **finish the standalone Pico staff first**, including a full rewrite of music/microphone lighting and effects for all four rings, unified state/UI behavior, bounded QUERY capture/local playback and a minimal versioned Wi-Fi contract. Then build the Raspberry Pi brain independently against that contract and join the two working systems. The staff must remain useful without the Pi or a network. See the [prioritized tasks](docs/tasks.md) and [end-of-session checkpoint](docs/checkpoints/2026-10-11.md).

## Current implementation — 2026-10-11

2026-10-11 current main: **1.0.11-VOICE-CAPTURE**, built/checked as **firmware/build/celestial_firmware.uf2**. Implements continuous 16 kHz 16-bit PCM microphone speech capture into RAM (up to 5.0 s, 160 KB) upon holding QUERY, and immediate playback via MAX98357A speaker upon release ([Experiment 026](docs/experiments/026-mic-record-ram-buffer.md)). Ring cosmetics postponed to backlog per user priority. Both files keep 59-word DMA, 300 µs reset, 4% cap, silent V0 and existing wiring.

Archived three-ring application **1.0.5-AUDIO-GAIN** targets Pico 2 W / RP2350. **ERR-AUDIO-02/03 RESOLVED:** the user repaired bad soldering and confirms working sound/volume, including the encoder. That archived profile: 43 LEDs (7 + 12 + 24; defective 16-LED ring removed). Firmware uses official 16-bit stereo I2S, default V10 and startup sounds. Startup and encoder now share one 40 dB volume curve: startup V10 is +3.64 dB from 1.0.4; V100 still uses the 14400 digital ceiling. Build, flash verification and live COM9 confirm the new version and startup peak 205 (previously 135). See [consistent gain adjustment 018](docs/experiments/018-volume-consistent-gain.md); physical loudness preference awaits ordinary use. The audio blocker is cleared; further integration may resume.

| Area | Current behavior / status |
|---|---|
| Display and controls | SH1106 on I2C1; encoder A/B decoded in GPIO IRQ; QUERY press/release require 30 ms stability |
| QUERY | Hold records voice into 160 KB RAM buffer; release stops recording and triggers speaker playback |
| LEDs | Main four-ring profile: 59 reserved slots (7 + 12 + 24 + spare reserve 16), GP15 / PIO0 SM1 / DMA, 4% cap; backgrounds dark |
| Microphone | 16 kHz live metering + continuous 16-bit PCM voice capture buffer (up to 5 s / 80k samples) |
| Speaker | MAX98357A + one QUARKZMAN 4 ohm / 3 W, OUT+/OUT−; solder repair and shared gain work. Current four-ring startup V0; archived audio profile used V10 |
| Mission Control | Web dashboard at http://localhost:5050 with one-click UF2 flasher, live telemetry stream, and presets |
| Milestones | Historical P0–P4/P6/P7 passes retained; P5 four-ring data response confirmed, effects partial; P7 audio revalidated after user solder repair; P8 standalone integration remains partial |

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
- LANGUAGE (EN default / DE / RU)
- SYSTEM

Encoder:
- rotate: choose mode / adjust current parameter
- press: SELECT / confirm

QUERY:
- press and hold: start listening/recording
- release: stop and submit
- never implement as a toggle

### Multilingual Architecture (Pico 2 W + Raspberry Pi 5)
- **Supported Languages:** **English** (default), **Deutsch**, **Русский**.
- **Mode Selection:** `[ LANGUAGE ]` screen on OLED; switch with encoder, saved to state. Indicator (`[EN]`, `[DE]`, `[RU]`) displayed in status header.
- **Canonical Offline Voice Phrases (Pico 2 W flash):**
  - **English (Default):** *"Celestial Sphere online"*, *"System ready"*, *"Celestial anomaly detected"*
  - **Deutsch:** *"Himmelssphäre online"*, *"System bereit"*, *"Kosmische Anomalie erkannt"*
  - **Русский:** *"Небесная сфера в сети"*, *"Система готова"*, *"Обнаружена аномалия"*
- **Full AI Voice Pipeline (Raspberry Pi 5):**
  - **STT:** Multilingual Whisper / Sherpa-onnx targeted to language code received from staff.
  - **LLM:** System prompt persona "Celestial Sphere" in selected language.
  - **TTS:** Fast local Piper TTS with native voices (`en_GB-alan-medium`, `de_DE-thorsten-medium`, `ru_RU-dmitri-medium`).

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

### P5 — Four-ring LED core [HARDWARE VERIFIED / EFFECTS UNIFICATION IN PROGRESS]
All four rings are connected as 7→12→24→16; the user repaired the 16-LED ring with a jumper bypassing one failed LED. Four-ring light response is stable at 4% brightness cap; user confirmed buggy white flashes are gone and dark backgrounds in PULSAR/VOICE/VORTEX work properly. Next: rewrite all four-ring animation and music/mic lighting effects with unified logical geometry (7→12→16→24) decoupled from physical wiring offsets.

### P6 — Microphone [PASSED / VERIFIED]
INMP441 verified on RP2350 via PIO0 SM0 circular DMA. Live voice reaction verified in telemetry and integrated into LED voice-reactive animation mode.

### P7 — Speaker [REVALIDATED AFTER USER SOLDER REPAIR]
MAX98357A + one 4 ohm/3 W speaker on PIO1 SM0. User repaired bad soldering and confirms sound and volume control work; ERR-AUDIO-02/03 are resolved. GAIN stays unconnected. Shared gain and 16-bit stereo I2S are preserved; current four-ring builds start silently at V0. Continue local audio/capture integration without reopening the historical speaker fault as an active blocker.

### P8 — Integrated staff [IN PROGRESS]
Run OLED + controls + LEDs + microphone + speaker under the central state machine. Add multilingual selector (EN default, DE, RU) and canonical offline audio cues ("Celestial Sphere online" / "Himmelssphäre online" / "Небесная сфера в сети").
The staff must operate independently of the brain/network, including local effects, music, settings and predictable QUERY handling. Complete the Pico side and its interface contract before building the Raspberry Pi service; transport and AI remain future work.

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
