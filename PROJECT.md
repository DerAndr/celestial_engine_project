# CELESTIAL ENGINE Mk.V

## Mission
Build a working interactive AI staff quickly from the hardware already purchased. This is an execution project, not a hardware-selection exercise.

The coding agent must guide the user physically, one testable prototype at a time, and maintain the repository as an engineering notebook.

## Current implementation — 2026-10-28 Standalone Grimoire, 24-Track Jukebox, Thematic Lighting, Multimodal Vision & Canonical TTS Streaming

Active physical staff: **7→12→24 = 43 LEDs**, GP15 / PIO0 SM1 / DMA, 300 µs reset, 18% brightness cap, SH1106 OLED, INMP441 Mic, MAX98357A Amp.
Firmware baseline: **1.1.0-RELEASE (HALLOWEEN-ORACLE)**, `firmware/build/celestial_firmware.uf2`. Pico 2 W / RP2350, official SDK 2.3.1.
Companion Mobile Brain: Android app v1.1.2 (`com.celestial.companion`) with Foreground Service (`StaffBrainService`), interactive **AI Persona & System Prompt Editor** (6 presets: Machine God, Wise Archmage, Marvin Android, Cyberpunk Netrunner, Jack Pumpkin King, and Custom Prompt with full EN/RU/DE localization), standalone **Arcane Grimoire** (Tab 0: Library with interactive per-spell toggles, Tab 1: Spell Forge), **Runes** continuous 43-LED ambient controller (21 animations), **Arcane Soundboard & Jukebox** (24 chiptunes & studio master tracks with Play All & Cycle modes), zero-latency fast-path casting (<20ms), Gemma 4 Vision & Voice, 32-bit Float TTS stream engine, and strict default English localization.

Wearable SBC Node: **Raspberry Pi 5 (16 GB RAM)** on **Pimoroni PIM699 Base** with **Lexar NM790 1 TB NVMe SSD** running in **PCIe Gen 3** mode (688 MB/s sustained write, 0x0 throttling, MicroSD completely removed, pure NVMe boot).
AI Brain Service: Active systemd daemon `celestial-brain.service` listening on UDP port 5005. Features both **100% Autonomous Offline AI** (`whisper.cpp` + Ollama `llama3.2:1b`/`gemma2:2b` + `piper` C++ ONNX) and **Hybrid Cloud Core** (OpenRouter + Edge-TTS) with seamless automatic failover.
Mission Control: `tools/start_monitor.sh` multi-pane tmux HUD combining `btop`, `pipeline_hud.py`, and live voice transaction logs.

| Area | Current behavior / status |
|---|---|
| Controls/display | SH1106 128×64; encoder/menu, language/model/volume selection; full-screen response marquee; 5 AI presets |
| QUERY/microphone | Hold streams 16 kHz PCM16 mono over Wi-Fi; release submits; 60 s capture limit; first 2 s retained in local 64 KB RAM buffer |
| Brain Server (Pi 5) | Active systemd daemon on UDP 5005; supports 100% Offline (Whisper+Ollama+Piper) and Cloud (OpenRouter+EdgeTTS) |
| Offline Latency | ~1.5s STT (Whisper Tiny) + ~2.1-5.6s LLM (Llama 1B / Gemma 2B) + ~0.5-0.8s TTS (Piper C++) = ~4.5-8.9s total |
| NVMe Subsystem | Pimoroni PIM699 + Lexar NM790 1 TB; PCIe Gen 3; 688 MB/s; standalone boot from /dev/nvme0n1p2; 35°C; throttled=0x0 |
| Speaker | MAX98357A + QUARKZMAN 4 Ω / 3 W; PCM16 stereo I2S, 512 kHz BCLK, 16 kHz LRC; V35 default, shared 40 dB curve |
| LEDs | Three rings (43 LEDs); recording vortex, processing data radar and speaking waves |
| Mission Control | Multi-pane tmux HUD: `btop` (CPU, RAM, NVMe I/O) + `pipeline_hud.py` (thermals, pipeline step) + `journalctl` |

## System architecture
There are two physically separate subsystems:

1. **STAFF** — Raspberry Pi Pico 2 W embedded controller handling controls, display, LEDs, microphone, speaker and Wi-Fi. Features complete **Standalone Offline Oracle** autonomy when no host is connected.
2. **AI BRAIN (Universal Host-Agnostic Architecture)** — handles neural processing, orchestration and speech return:
   - **Mode 1 (Primary / Default: Android Companion Hotspot & Mobile Brain):** Smartphone acts as Wi-Fi Hotspot and runs the Android Companion App. The phone serves as the UDP 5005 Brain Host directly, running Gemma/OpenRouter LLM and TTS without requiring external Wi-Fi or a laptop.
   - **Mode 2 (Wearable SBC — Optional Pi 5 Node):** Standalone wearable node on Geekworm X1209 UPS + Pimoroni PIM699 + Lexar NM790 running offline Whisper + Ollama/Llama + Piper TTS. Switchable via OLED `[HOST]` menu.
   - **Mode 3 (Home / Studio Dev PC):** Brain server runs locally on PC (`tools/ai_brain_server.py`) over local Wi-Fi.
   - **Mode 4 (Standalone Offline Oracle):** Zero external host. The staff operates completely self-contained with onboard microphone sensing, concentric LED vortex, local voice phrases from flash (EN, DE, RU), cosmic sound effects, and OLED retrofuturistic prophecies.
   - See [Universal Brain Architecture Specification](docs/architecture/ai_dual_brain_architecture.md).

Communication is wireless over Wi-Fi (UDP protocol v2 on port 5005). Do not introduce a physical data cable between the host and the staff. The staff firmware is completely agnostic to whether Cloud, PC, phone hotspot, or Pi 5 is answering.

## Halloween MVP
The voice loop runs with the host-agnostic brain server (Cloud or PC) and has standalone fallback:

1. Hold QUERY.
2. If Brain is online: Staff enters LISTENING and streams audio to Cloud/Host.
3. If Brain is offline: Staff enters OFFLINE ORACLE and samples audio locally.
4. Release QUERY.
5. If Brain is online: Server performs STT -> LLM -> TTS and streams synthesized voice into staff speaker.
6. If Brain is offline: Staff plays local voice phrase in selected language [EN/DE/RU], triggers concentric LED shockwaves, and displays cosmic oracle marquee on OLED.
7. Reliability and autonomy matter more than enclosure polish.


## Purchased / selected hardware
### Staff
- Raspberry Pi Pico 2 W with factory headers (RP2350, Wi-Fi/Bluetooth)
- 1.3-inch 128x64 SH1106 I2C OLED
- Bourns PEC11R-4225F-S0024 rotary encoder, 24 detents/pulses, push switch
- Momentary normally-open QUERY button for prototype
- INMP441 I2S MEMS microphone
- MAX98357A I2S mono Class-D amplifier
- 4 ohm / 3 W mini speaker
- Active WS2812B rings: 7 + 12 + 24 = 43 LEDs; purchased 16-LED ring discarded after instability
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
Already owned & operational:
- Raspberry Pi 5 (16 GB RAM, Bookworm Lite 64-bit baseline)
- official Raspberry Pi Active Cooler
- official Raspberry Pi 27W USB-C PSU
- Lexar NM790 1 TB M.2 NVMe SSD (healthy; waiting for PIM699)

Storage subsystem update (2026-10-13):
- Geekworm X1002 V1.1 & P580-V2 retired due to 3.3V write brownout on Gen 4 SSD.
- Pimoroni PIM699 (NVMe Base) selected to replace X1002 (see [Incident Report](docs/hardware/rpi5_nvme_migration_incident_report.md)).

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

Current local/network interaction follows IDLE → LISTENING → PROCESSING → SPEAKING → IDLE; UPLOADING is implicit in live capture. Definitions are in `firmware/src/staff_state.h`, with local orchestration in `firmware/src/main.c`; recording/processing/speaking LED patterns are integrated. Full state-machine consolidation remains a future cleanup.

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

### P5 — Three-ring core [ACTIVE / USER CONFIRMED]
Fourth ring discarded by the user due to instability. Active chain is 7→12→24 (43 LEDs, 18% cap); no replacement or four-ring rewrite is required. Keep the working three-ring animations and state patterns.

### P6 — Microphone [PASSED / VERIFIED]
INMP441 verified on RP2350 via PIO0 SM0 circular DMA. Live voice reaction verified in telemetry and integrated into LED voice-reactive animation mode.

### P7 — Speaker [REVALIDATED AFTER USER SOLDER REPAIR]
MAX98357A + one 4 ohm/3 W speaker on PIO1 SM0. User repaired bad soldering and confirms sound and volume control work; ERR-AUDIO-02/03 are resolved. GAIN stays unconnected. Shared gain and 16-bit stereo I2S are preserved; current three-ring main retains V35 startup. Continue local audio/capture integration without reopening the historical speaker fault as an active blocker.

### P8 — Integrated staff [IN PROGRESS]
Run OLED + controls + LEDs + microphone + speaker under the central state machine. Add multilingual selector (EN default, DE, RU) and canonical offline audio cues ("Celestial Sphere online" / "Himmelssphäre online" / "Небесная сфера в сети").
The staff must operate independently of the brain/network, including local effects, music, settings and predictable QUERY handling. Live Wi-Fi requests, generation and response playback now run with the PC server. Validate response fixes, then move the same service to Pi 5; preserve offline local functionality.

### P9 — Brain link [PC SERVICE WORKING / V2 FIXES PREPARED]
Built-in Wi-Fi with heartbeat, live upload, cloud AI and acknowledged response streaming. Validate the v2 pair before changing network/provider deployment.

### P10 — Pi 5 deployment & feature synchronization [ACTIVE TASK]
Offline local AI baseline (Whisper + Ollama + Piper on NVMe) verified in Exp 041. Active task: synchronize Pi 5 Python server (`tools/ai_brain_server.py`, `tools/local_ai_engine.py`) with canonical release v1.1.0 (6 AI Personas, concise <25 words constraint, [SPELL: ...] hardware dispatch, default English, and wearable field networking). See docs/tasks.md.


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
