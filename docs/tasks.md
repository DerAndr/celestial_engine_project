# Tasks — Final Setup: Phone + Pico 2 W Core, Hotspot First, & AI Spells

Updated 2026-11-21. The project baseline locks the primary operating topology to **Pico 2 W (Modular RP2350 Firmware v1.4.0) + Android Companion Phone (Mobile Brain v1.4.0 with On-Device Moonshine/SenseVoice ONNX STT, SmolLM2-135M ONNX LLM & Kokoro-82M Voice Profiles)**, with an optional connection profile to **Raspberry Pi 5**, and a full **Dual-Mode Interactive Web Digital Twin & In-Browser Local LLM Showcase (`v1.4.0`)** deployed on GitHub Pages.

---

## 1. Architectural Baseline: Primary Pair & Fallback Node

```
┌─────────────────────────────────┐        Wi-Fi (UDP 5005)       ┌─────────────────────────────────┐
│        CELESTIAL STAFF          │◄─────────────────────────────►│    ANDROID COMPANION PHONE      │
│  - Pico 2 W (RP2350)            │   (Direct Mobile Hotspot AP)  │  - Wi-Fi Hotspot Host (AP)      │
│  - 43 WS2812B LEDs (7→12→24)   │                               │  - Mobile Brain Server          │
│  - SH1106 OLED (128x64)         │                               │  - Gemma / OpenRouter LLM Core  │
│  - INMP441 Mic + MAX98357A Amp  │                               │  - Local / Cloud TTS Engine     │
│  - Physical QUERY & PEC11R Enc  │                               │  - Live HUD & Remote Control    │
└─────────────────────────────────┘                               └─────────────────────────────────┘
                 ▲                                                                 ▲
                 │                     Optional Secondary Link                     │
                 └─────────────────────────────────────────────────────────────────┘
                                                  │
                                                  ▼
                                  ┌─────────────────────────────────┐
                                  │      RASPBERRY PI 5 (OPTIONAL)  │
                                  │  - Wearable SBC Backpack Node   │
                                  │  - 100% Offline Ollama/Whisper  │
                                  │  - NVMe Standalone Storage      │
                                  └─────────────────────────────────┘
```

- **Default Setup**: **Phone + Pico 2 W**. On boot, Pico connects to Phone's AP, phone hosts UDP 5005, and acts as the brain.
- **Secondary / Optional Setup**: **Raspberry Pi 5 Wearable Node**. Accessible via OLED menu `[HOST] -> PI 5 BRAIN` or auto-mesh when on home network.

---

## 2. Priority Action Items

### 🚨 URGENT TASK: Default Model Migration to `gemma-4-E2B`, Concise Responses & Bounded Logs — ✅ COMPLETED
> **User Directives (2026-10-25):**
> 1. *"gemma-4-E2B — давай работать вот с этой моделью по умолчанию. Потому что она точно удобнее и проще и быстрее скорее всего. Это будет новая срочная задача."*
> 2. *"так же добавь задачу что ответы должны быть короткими"*
> 3. *"И еще лог в приложении не должен быть бесконечным - давай его ограничим"*

- [x] **Android Companion App (`GemmaModelEngine.kt`)**:
  - Set default variant to `gemma-4-E2B` (`G4-E2B` / `google/gemma-4-e2b-it`).
  - Implemented automatic network fallback to `google/gemma-4-26b-a4b-it` (3.8B active param MoE) if E2B endpoint is absent.
- [x] **Concise & Punchy Voice Responses (Краткие ответы)**:
  - System Prompt tuning (RU/EN/DE): Strictly constrained verbal responses to **1–2 short, punchy sentences (max 15–25 words)**.
  - Tone: witty, razor-sharp cosmic oracle without long-winded monologues, corporate filler, markdown, or emojis.
  - Capped `max_tokens` to `128` tokens so generation finishes almost instantly and TTS audio playback lasts < 3 seconds on the staff speaker.
  - Compact text displays crisply on the SH1106 128x64 screen without overwhelming the user.
- [x] **Dashboard & UI (`DashboardViewModel.kt`, `OracleTerminalScreen.kt`)**:
  - Updated default selector to `GemmaVariant.GEMMA_4_E2B`.
  - Added `G4-E2B` model chip and updated dynamic query button to `Query G4-E2B`.
  - Verified spell tag parsing (`[SPELL: ...]`) and two-stage acoustic query pipeline with G4-E2B.
- [x] **Bounded In-App Log Telemetry & History (`StaffRepository.kt`, `DashboardViewModel.kt`, `LiveLogConsole.kt`)**:
  - Capped `StaffRepository` live telemetry log buffer to `MAX_LOG_ENTRIES = 100` with sliding window truncation.
  - Capped `DashboardViewModel._chatHistory` to `MAX_CHAT_HISTORY = 40` messages to eliminate memory leak and LazyColumn rendering stutter.
  - Updated `LiveLogConsole` telemetry header badge to show bounded count `${logs.size}/100`.
- [x] **Staff Integration & Latency Benchmark**:
  - Measure round-trip time from staff GP9 button release to speaker playback.
  - Document results in [`docs/experiments/048-gemma-4-e2b-default-migration.md`](experiments/048-gemma-4-e2b-default-migration.md).

### Task 1: Phone + Pico Default Mode & Firmware/App Alignment
- [x] **Firmware (`net_client.c`)**:
  - Direct Phone Hotspot AP IP link (`10.111.174.64` ↔ `10.111.174.174`).
  - Active heartbeat exchanges and telemetry tick stream.
- [x] **Android App (`StaffRepository.kt` & `MainActivity.kt`)**:
  - `PHONE_BRAIN_SERVER` active by default.
  - UDP 5005 bound to phone tethering interface to bypass Android tethering EPERM restrictions.
  - Retain active screen and vision state across configuration and lifecycle changes.
- [x] **Android Background Service**:
  - Run foreground service `StaffBrainService` to prevent OS sleep/throttling when phone screen is locked (verified active foreground service ID 2026).
- [x] **Preserve Raspberry Pi 5 Switchability**:
  - Keep menu item `[HOST]` on OLED with choices: `PHONE BRAIN (Default)`, `PI 5 BRAIN`, and `AUTO MESH`.
  - Companion App dashboard toggle to hand off brain execution between Phone, Pi 5, or Auto-Mesh.

### Task 2: Mobile Hotspot First Network Flow (Zero External Wi-Fi)
- [x] **Firmware (`wifi_credentials.h` & `net_client.c`)**:
  - Phone Hotspot is default primary operating network without external routers.
  - DHCP IP allocation with direct gateway communications.
- [x] **Android App Hotspot HUD**:
  - Live HUD displaying Staff IP (`10.111.174.64`), ONLINE status, tick counter, and digital twin state.

### Task 3: Universal Emergency Stop / Abort Button (Pico + Android)
- [x] **Android Companion App**:
  - Added prominent **[STOP / ABORT]** button on Dashboard / Oracle Terminal when generating.
  - Cancels LLM coroutine, halts TTS audio synthesis, and resets app state immediately to `IDLE`.
- [x] **Pico Physical Staff Interrupt**:
  - Clicking rotary encoder button (PEC11R SW) or QUERY on staff during `STATE_PROCESSING` / `STATE_SPEAKING` aborts audio DMA and resets state. Verified in firmware and test harness.

### Task 4: Multimodal Gemma 4 Vision & Live Camera
- [x] **Memory-Safe Downscaling & Decoding**:
  - Two-pass `decodeSampledBitmap` (max 800px, EXIF orientation correction, intermediate bitmap recycling).
  - `android:largeHeap="true"` enabled.
  - Compressed Base64 payload (<100 KB) with 30s/60s network timeouts.
  - Robust `catch (e: Throwable)` with error banner diagnostics.
- [x] **Live Camera Snap & Gallery Integration**:
  - `androidx.core.content.FileProvider` (`res/xml/file_paths.xml`) for direct camera snapshots.
  - Dedicated camera capture button with runtime permission handling.
  - Dedicated gallery picker button.
- [x] **Contextual Vision Preset Chips**:
  - Dynamic photo presets: `🔍 Что на фото?`, `✨ Артефакт`, `📜 Прочесть текст`, `🔮 Аура и тайна` with 1-click execution.
  - Model selector pills: `G4-31B`, `G4-26B`, `G3-27B`, `G2.5-FL`.

### Task 5: AI Spells & Dynamic Arcane LED Incantations & Sound Combos
- [x] Initial specification and brainstorming documented in [ai_spells_and_dynamic_lighting.md](architecture/ai_spells_and_dynamic_lighting.md).
- [x] **LLM Prompt Arcane Tag Injection (`GemmaModelEngine.kt`)**:
  - System prompts (RU/EN/DE) instruct Gemma / Gemini to prepend:
    `[SPELL: Name | ANIM: Animation | SOUNDS: Sfx1, Sfx2, ...] <Incantation text>`
  - Generates clever, hilarious sound chains (e.g. Alien ➔ Droid Panic ➔ Supernova) paired with techno-magical incantations.
- [x] **Real-time Stream Dispatch & Execution (`DashboardViewModel.kt`)**:
  - Regex interceptor catches `[SPELL: ...]` in earliest token stream chunk.
  - Dynamically triggers 43 WS2812B LEDs, broadcasts spell title to SH1106 OLED over UDP, and plays paced sound chains with 750 ms delays.
- [x] **Arcane Grimoire UI & Presets (`OracleTerminalScreen.kt`)**:
  - Active Spell card with glowing border, LED badge, Sound sequence badge, "Replay Sounds", "Re-Cast", and "Dismiss".
  - Scrollable Grimoire with presets: *Квантовая Утка*, *Плазменный Шторм*, *Паника Дроида*, *Гравитационный Чих*, *Астральный Портал*.
  - "🎲 AI Conjure" button generating unpredictable, hilarious new spells.
- [x] **Soundboard Combos (`SoundboardScreen.kt`)**:
  - Quick-trigger sound chain buttons directly on the soundboard.
- [x] **2D Polar Kinematics Engine in `leds.c` (43 LEDs: 7→12→24)**:
  - `SPELL_ANIM_SPIRAL_SNAKE`: Multi-ring Ouroboros serpent coiling through 24 -> 12 -> 7 with core flare.
  - `SPELL_ANIM_RADIAL_WAVE`: Concentric ripples & shockwaves propagating outward with inverse-square damping.
  - `SPELL_ANIM_COLLIDING_WAVES`: Counter-rotating wave crests colliding at 0°/180° with white constructive flash.
  - `SPELL_ANIM_ARCHIMEDES_SPIRAL`: Hypnotic rotating dual spiral arms with radial phase lag across concentric rings.
  - `SPELL_ANIM_SACRED_POLYGON`: Stroboscopic geometric runes (Triquetra, Hexagram / Merkaba) rotating at harmonic ratios.
  - Phonetic VU Coupling: Real-time amplitude modulation from speaker DMA (words glow, plosives shockwave).

### 🎯 Active Priority: Raspberry Pi 5 Wearable Node Modernization & Full Parity (Актуализация Raspberry Pi 5) — ✅ COMPLETED (2026-11-07)
> **Goal:** Bring the Raspberry Pi 5 Wearable Node (`tools/ai_brain_server.py` & `tools/local_ai_engine.py`) to 100% parity with the companion app and canonical v1.3.0 firmware, enabling seamless standalone offline AI with personas, spells, and concise speech.

- [x] **1. AI Persona Presets & System Prompt Modernization (`tools/ai_brain_server.py`, `tools/local_ai_engine.py`)**:
  - Ported all 6 interactive AI Personas (*Machine God, Wise Archmage, Marvin Android, Cyberpunk Netrunner, Jack Pumpkin King, Custom Prompt*) into the Python server (`ORACLE_PERSONAS`) with voice-command switching in EN/DE/RU.
  - Enforced concise, punchy speech: 1–2 sentences, maximum 15–25 words, with `"think": False` and `"num_predict": 48` for fast inference on Pi 5.
  - Strict guardrails: stripped `<think>`/`<thought>` reasoning blocks, markdown, asterisks, and emojis (`strip_thinking_and_markdown`) for clean Piper TTS speech.
  - Default language: English (`[EN]`), with native EN/DE/RU system prompt templates.
- [x] **2. Arcane Spells & Incantations Hardware Dispatch (`[SPELL: ...]`)**:
  - Added `FastSpellMatcher` (`match_fast_spell`), prompt injection (`SPELL_PROMPT_INJECTION`), and regex interceptor (`parse_spell_tag`) in `ai_brain_server.py` for:
    `[SPELL: Name | ANIM: Animation | SOUNDS: Sfx1, Sfx2, ...] <Incantation text>`
  - Implemented `BrainTransport.send_app_command()` (`NET_MSG_APP_COMMAND = 0x20`, signed with FNV-1a `"apophis"`) to trigger all 35 LED animations (`0..34`) and 14 sound effects (`0..13`) on the staff.
- [x] **3. Dynamic Model Catalog & Protocol Alignment**:
  - Aligned `NET_MSG_CATALOG_RESP` (`build_catalog_payload`), `DEFAULT_PI5_CATALOG` in `firmware/src/net_client.c`, and `LOCAL_MODELS_BY_PRESET` (`4=qwen2.5:0.5b`, `5=gemma4:e2b`, `6=deepseek-r1:1.5b`, `7=qwen2.5:1.5b`).
  - Added startup RAM pre-warming (`keep_alive=-1`) for `qwen2.5:0.5b` and `gemma4:e2b` on `celestial-brain.service` boot.
- [x] **4. Pi 5 Deployment & Service Verification**:
  - Connected to Raspberry Pi 5 (`rpi-15`, `192.168.178.51`), linked `/home/andr/celestial_engine_project` to Git (`origin/master`), deployed updated `tools/`, and restarted `celestial-brain.service`.
- [x] **5. Wearable Network Profile (Offline Field Mobility)**:
  - Configured NetworkManager (`nmcli`) on Pi 5 with auto-connect profile `Celestial-Hotspot` (priority 5) alongside home Wi-Fi `FRITZ!Box 6690 YQ` (priority 10).
- [x] **6. End-to-End Voice Cycle Verification (UDP v2 ↔ Pi 5)**:
  - Verified live UDP v2 `Heartbeat ACK`, `Catalog Response` (8 models), `Whisper.cpp` STT (`1.09s`), `Ollama` `qwen2.5:0.5b` (`0.35s` warm) & `gemma4:e2b` (`2.35s` warm), and `Piper` 16 kHz PCM TTS streaming.
- [x] **7. 2026 SOTA English Audio Stack (Moonshine ONNX + Kokoro-82M ONNX) & Interactive Web Lab (`:8080` / `:8443`)**:
  - Deployed `Moonshine Tiny INT8 ONNX` (`sherpa-onnx`, `271–480 ms` transcription, 2.6–4.5x faster than Whisper) and `Kokoro-82M INT8 ONNX` (`kokoro-onnx`, 6 studio voices matched to the 6 Oracle Personas, `0.72x RTF` faster-than-realtime).
  - Built and deployed the English-only Interactive Web Test Console (`tools/pi5_web_console.py`, automatically launched by `celestial-brain.service`) on `http://192.168.178.51:8080/` and `https://192.168.178.51:8443/` with live browser microphone recording, interactive **43-LED Concentric Ring & SH1106 128×64 OLED Digital Twin**, Web Audio 8-bit SFX synthesizer, `[STOP / ABORT]` button, 11 quick spell chips, audio player, and side-by-side SOTA benchmark.


---

---

## 3. Active Debugging: BUG-VOICE-01 (End-to-End Voice Cycle) — ✅ RESOLVED (Exp 046/047)
- [x] **Verify phone captures audio from Pico over Hotspot UDP 5005**:
  - Staff INMP441 mic captures 16 kHz PCM mono, packages into `NET_MSG_AUDIO_STREAM`, and streams to phone `10.111.174.174:5005`.
- [x] **Verify OpenRouter / Gemma streams tokens into `DashboardViewModel`**:
  - Two-stage acoustic pipeline: fast acoustic transcription (~250ms) + Gemma model generation with Chain-of-Thought (`<thought>...</thought>`).
- [x] **Verify Android TTS generates 16 kHz PCM and `streamAudioToStaff` transmits chunks back**:
  - Pre-warmed `StaffTtsSynthesizer` with 1.15x speed factor.
  - Deep audio pre-buffer burst (25 chunks at 1ms delay) wakes Pico DAC within 7ms and eliminates buffer starvation.
  - Android 15 hotspot socket routing via `ACCESS_LOCAL_NETWORK` and bound `DatagramSocket(null)`.
- [x] **Verify Pico speaker plays the speech response clearly**:
  - MAX98357A speaker playback verified cleanly on Pico 2 W without stutter or drops.

---

## 4. Full Independent Refactoring Plan: Eliminating Cruft & Fragility
> **Objective**: Remove accumulated technical debt, ad-hoc hacks, and legacy files from past experiments that cause unexpected bugs. Re-architect the codebase into clean, modular, testable components.

### Phase 1: Workspace Sanitation & Housekeeping — ✅ COMPLETED (2026-11-07)
- [x] **Purge Root Clutter**:
  - Removed stale build directories: `temp_apk*`, `build_apk`, `build_artifacts`, `android_artifact*`, `android_bin`, `android_build_artifact*`.
  - Purged obsolete temporary root screenshots (`screen_*.png`, `app_screen_*.png`, `live_*.png`) while preserving `docs/assets/celestial_flask_physical.png`.
  - Hardened root `.gitignore` (`/*.png`, `/*.jpg`, `!docs/assets/**`, `temp_apk*/`, `build_apk/`) to prevent generated artifacts and test images from dirtying the working tree.
- [x] **Configuration & Script Hygiene**:
  - Standardized script paths (`tools/`) and dynamic LAN/Hotspot IP discovery (`discover_local_ips()`).

### Phase 2: Android Companion App Architecture Decoupling — ✅ COMPLETED (2026-11-07)
*Target: Break down monolithic files (`StaffRepository.kt`, `GemmaModelEngine.kt`, `DashboardViewModel.kt`) into clean, decoupled layers.*

- [x] **Binary Protocol Codec Layer (`protocol/`) — ✅ Extracted & Unit Tested**:
  - Extracted packet serialization and deserialization into `CelestialPacket.kt`, `NetMessageType.kt`, and `ProtocolConstants.kt`, verified by `CelestialPacketTest.kt`, `tests/test_protocol_v2_parity.py`, and `tests/test_android_packet_compat.py`.
- [x] **Network Transport Layer (`network/`) — ✅ Extracted**:
  - Isolated Android 15 socket binding, hotspot network discovery, and UDP packet transport into `StaffTransport.kt` and `StaffUdpTransport.kt`.
- [x] **Fast Spell Matcher & Prophecy Parser (`ai/FastSpellMatcher.kt`, `domain/model/CelestialProphecy.kt`) — ✅ Extracted & Unit Tested**:
  - Extracted deterministic spell trigger matching (EN/RU/DE, including Harry Potter, eBay, Sailor Moon, Pokémon) and `[SPELL: ...]` tag parsing, verified by `FastSpellMatcherTest.kt` and `CelestialProphecyTest.kt`.
- [x] **Model Variant Registry (`ai/GemmaVariantRegistry.kt`) — ✅ Extracted & Unit Tested**:
  - Extracted `GemmaVariant` and `GemmaVariantRegistry` out of `GemmaModelEngine.kt` with `GemmaVariantRegistryTest.kt`.
- [x] **AI & Speech Subsystem Modularization (`ai/AcousticTranscriptionService.kt`, `ai/GemmaModelService.kt`) — ✅ Extracted & Unit Tested**:
  - Split `GemmaModelEngine.kt` into focused services:
    - `AcousticTranscriptionService.kt`: Handles fast Stage-1 audio-to-text transcription helper, WAV packaging, and noise-hallucination filtering.
    - `GemmaModelService.kt`: Handles LLM system prompt construction, persona/language formatting, model endpoint streaming, and SSE token/thought extraction.
- [x] **Domain UseCases & State Reducers (`domain/usecase/`) — ✅ Extracted & Unit Tested (`29/29` JUnit 4 tests pass)**:
  - Extracted business flows out of `DashboardViewModel.kt` and `StaffRepository.kt`, verified by `DomainUseCasesTest.kt`:
    - `ProcessStaffVoiceQueryUseCase.kt`: Coordinates Mic Audio -> Transcription -> Fast Spell Pre-Match -> LLM Stream -> Prophecy & Spell Dispatch -> TTS Text Preparation.
    - `SyncStaffTelemetryUseCase.kt`: Manages heartbeat ticks, online timeout evaluation, bounded telemetry logs (`100`), and bounded chat history (`40`).
    - `EmergencyAbortUseCase.kt`: Clean instant cancellation across AI coroutines, TTS synthesis, and Pico audio playback.

### Phase 3: Pico 2 W Firmware Modularization & Cleanup (`firmware/src/`) — ✅ COMPLETED (2026-11-07)
*Target: Streamline `net_client.c` and `audio.c` into clean, modular, host-testable components.*

- [x] **Modular Packet Dispatcher (`net_dispatcher.c`, `net_dispatcher.h`) — ✅ Extracted & Host-Tested**:
  - Replaced ad-hoc packet switch branches with a lookup-table-driven message validator and dispatcher (`net_dispatch_validate_packet`, `net_dispatch_lookup_descriptor`, `net_dispatch_route_packet`), enforcing strict boundary checks for all UDP v2 message types (`test_net_dispatcher_validation_and_routing`).
- [x] **Network State Machine Decoupling (`net_client.c`) — ✅ Completed**:
  - Separated connection lifecycle (Wi-Fi join, DHCP, IP resolution, Heartbeat) from validated downlink message routing (`s_downlink_routes`).
- [x] **Audio DMA & Buffer Separation (`audio_ring_buffer.c`, `audio_ring_buffer.h`) — ✅ Extracted & Host-Tested**:
  - Decoupled MAX98357A I2S DMA ring buffer management into a clean lock-safe power-of-two FIFO abstraction (`audio_rb_init`, `audio_rb_push_samples`, `audio_rb_pop_sample`) with explicit `overflow_drops` and `underflow_reads` counters (`test_audio_ring_buffer_fifo_and_bounds`).
- [x] **Strict Compiler Cleanliness (`-Wall -Wextra`) — ✅ Completed (2026-11-07)**:
  - Enabled `-Wall -Wextra` in `firmware/CMakeLists.txt` and verified zero compiler warnings across all 292 targets (`arm-none-eabi-gcc 15.2`) and all 21 MSVC C host tests.

### Phase 4: Integration Test Suite & Protocol Regression Prevention — ✅ COMPLETED (2026-11-07)
- [x] **Automated Protocol Packet Round-Trip Tests (`tests/test_protocol_v2_parity.py`)**:
  - Verifies exact 3-way binary layout and constant parity across C (`net_protocol.h`, `net_client.c`), Kotlin (`NetMessageType.kt`, `ProtocolConstants.kt`, `CelestialPacket.kt`), and Python (`tools/brain_transport.py`, `tools/ai_brain_server.py`).
- [x] **Mock Voice Loop End-to-End Test (`tests/test_protocol_v2_parity.py`)**:
  - Runs a full in-process UDP socket voice loop (`HEARTBEAT` ➔ `CATALOG_REQ` ➔ `QUERY_START` ➔ `AUDIO_CHUNK` ➔ `QUERY_END` ➔ `[SPELL: ...]` `APP_COMMAND 0x20` dispatch ➔ `RESP_TEXT` ➔ `RESP_START` ➔ `RESP_CHUNK` ➔ `RESP_END`) without physical hardware.

---

## 5. Themed Packs (eBay, Harry Potter, Pokémon, Sailor Moon, Doom, Tetris, Zelda, etc.) — ✅ COMPLETED (Exp 055–060 & Web Twin v1.3.0)

> **User Requests (2026-10-26 .. 2026-10-29):**
> 1. *"Таска на будущее - не делать сейчас - сделай как одну из визуализаций на посохе - в духе eBay для светодиодов и смешных звуков. Так же сделай темы в духе гарри поттера и pokenom И сейлор мун!"*
> 2. *"про звуки - добавь 8-bit мелодии к тем темам мв задачи"*

- [x] **1. eBay Chaos Auction Mode (Visuals, 8-bit Bidding Chiptune & Hilarious SFX) — ✅ Completed (Exp 056 & Web Twin)**:
  - **LED Animation**: 4-color bouncy eBay strobe palette (Red, Blue, Yellow, Green) racing around the 3 concentric rings in accelerating bid steps, culminating in an energetic rainbow auction flash (`FX #26`).
  - **8-Bit Chiptune Track**: Retro arcade "eBay Auction Sniping Frenzy" fast-paced 8-bit square wave melody (`Track #13`).
  - **Sound Effects**: Cash register "cha-ching!" (`SFX #10`).
  - **OLED Text**: Retro 80s marketplace marquee: `$$$ WINNING BID! $$$`, `BID ACCEPTED`, `OUTBID!`.
  
- [x] **2. Harry Potter Arcane Pack (House Runes & 8-bit Hedwig's Theme) — ✅ Completed (Exp 056 & Web Twin)**:
  - **Hogwarts House Runes**: Preset 43-LED color themes for Gryffindor (scarlet & gold) and Lumos/Patronum spell flares (`FX #24`).
  - **8-Bit Chiptune Track**: Iconic **8-bit "Hedwig's Theme"** (`Track #9`) played on retro pulse-wave synth with synchronized mystical LED ripples.
  - **Signature Spells**: *Lumos Flare* (`SFX #12`) and *Expecto Patronum* (`SFX #13`).

- [x] **3. Pokémon Arcane Pack (Pokéball Ring & 8-bit Battle / Center Chiptunes) — ✅ Completed (Exp 056 & Web Twin)**:
  - **Pokéball Concentric Topology**: Outer ring crimson red / white with pulsing capture center LED (`FX #25`).
  - **8-Bit Chiptune Tracks & SFX**: *Pokémon Theme* (`Track #10`) and *Pokécenter Healing Jingle* (`SFX #11`).

- [x] **4. Sailor Moon / Magical Girl Arcane Pack (Prism Power & 8-bit Moonlight Densetsu) — ✅ Completed (Exp 056 & Web Twin)**:
  - **Moon Prism Power Transformation Sequence**: Concentric sparkling waves of pastel pink, magenta, gold, and cyan (`FX #23`).
  - **8-Bit Chiptune Track**: Iconic **8-bit "Moonlight Densetsu"** (`Track #11`) transcribed for RP2350 and Web Audio synth.

- [x] **5. Doom Mode (E1M1 At Doom's Gate & BFG Plasma Shockwaves) — ✅ Completed (Exp 055)**:
  - **8-Bit Chiptune Track**: Driving heavy metal square-wave riff **"At Doom's Gate" (E1M1)** with fast bass chug and bridge.
  - **LED Animation**: Hellfire Crimson & BFG9000 Acid Green plasma shockwaves erupting from the center outward with heavy beat strobing.
  - **Sound Effects**: Shotgun cocking & blast, demon screech, BFG9000 plasma charge-up sound.
  - **OLED Text**: `DOOM: RIP & TEAR`, `E1M1: KNEE-DEEP IN THE DEAD`.

- [x] **6. Tetris Arcade Mode (Concentric Tetromino Ring Drop & Line Clear) — ✅ Completed (Exp 055)**:
  - **8-Bit Chiptune Track**: Full retro Game Boy **"Korobeiniki"** theme with accelerating BPM as game progresses.
  - **LED Animation**: Concentric falling tetrominoes: 4-LED geometric blocks rotating through inner rings and stacking into the 24-LED outer rim, culminating in a 4-line "Tetris!" white flash.
  - **Sound Effects**: Hard drop thud, line clear jingle, level-up fanfare, retro game over descent.
  - **OLED Text**: `TETRIS: 4-LINE CLEAR!`, `SCORE: 999990`.

- [x] **7. The Legend of Zelda Mode (Triforce Rune & Overworld Fanfare) — ✅ Completed (Exp 055)**:
  - **8-Bit Chiptune Track**: Iconic **Zelda Overworld March & Fanfare** with majestic medieval chiptune cadence.
  - **LED Animation**: Golden Sacred Triforce: 3 golden vertex points on outer ring rotating in harmonic resonance with an emerald inner courage core.
  - **Sound Effects**: Secret discovery chime (`da-na-na-na-na-NA!`), sword beam slash, heart container refill beep.
  - **OLED Text**: `IT'S DANGEROUS TO GO ALONE!`, `TAKE THIS 🗡️`.

- [x] **8. The Elder Scrolls III: Morrowind Mode (Nerevar Rising & Red Mountain Aurora) — ✅ Completed (Exp 055)**:
  - **8-Bit Chiptune Track**: Jeremy Soule's legendary **"Nerevar Rising / Call of Magic"** transcribed into evocative, rich 8-bit harmonics.
  - **LED Animation**: Red Mountain ash storm: volcanic amber/crimson core pulsing outward, surrounded by mystical Dagoth Ur purple and tranquil Vivec turquoise auroras.
  - **Sound Effects**: Silt Strider haunting call, Cliff Racer screech, Level-up drum fanfare ("You should rest and meditate on what you've learned").
  - **OLED Text**: `STAND UP, THERE YOU GO...`, `NEREVARINE AWAKENS`.

- [x] **9. Ghostbusters Mode (Proton Pack Crossing Streams & Ectoplasm Green) — ✅ Completed (Exp 055)**:
  - **8-Bit Chiptune Track**: Ray Parker Jr.'s **"Ghostbusters"** funky 8-bit slap bassline and synth hook ("Who you gonna call?!").
  - **LED Animation**: Slimer Ectoplasm neon lime swirl colliding with crossing Proton Pack streams (counter-propagating red and blue particle beams creating white containment sparks).
  - **Sound Effects**: Proton pack spin-up & firing hum, ghost trap opening zap, PKE meter rapid ticking.
  - **OLED Text**: `WHO YOU GONNA CALL?`, `GHOSTBUSTERS! 🚫👻`.

- [x] **10. Star Wars Mode (The Imperial March & Lightsaber Plasma Clash) — ✅ Completed (Exp 055)**:
  - **8-Bit Chiptune Track**: John Williams' **"The Imperial March (Darth Vader's Theme)"** with authoritative militaristic march tempo.
  - **LED Animation**: Dual Lightsaber Clash: Sith Crimson wave and Jedi Azure wave colliding at 0° and 180° with white plasma flash and inverse shockwave.
  - **Sound Effects**: Lightsaber ignition hiss & dynamic hum, TIE Fighter flyby roar, heavy blaster discharge.
  - **OLED Text**: `THE EMPIRE STRIKES`, `MAY THE FORCE BE WITH YOU`.

- [x] **11. Spooky Scary Skeletons Mode (Bone Dance Strobe & Pumpkin Glow) — ✅ Completed (Exp 055)**:
  - **8-Bit Chiptune Track**: Andrew Gold's viral **"Spooky Scary Skeletons"** catchy electro-swing 8-bit xylophone & bass groove.
  - **LED Animation**: Bone-white stroboscopic skeleton dance alternating with eerie Jack-o'-Lantern glowing pumpkin orange and spectral violet ripples.
  - **Sound Effects**: Rattling bone castanets, cartoon xylophone stumble, witch cackle, eerie howling wind.
  - **OLED Text**: `SPOOKY SCARY SKELETONS`, `SEND SHIVERS DOWN YOUR SPINE! 💀🎃`.

---

## 6. GitHub Repository Polish & Interactive GitHub Pages Simulator (Public Showcase) — ✅ COMPLETED (2026-10-29, v1.3.0)

> **User Request (2026-10-26 .. 2026-10-29):**
> *"так же добавь к задачам в конце оформить гитхаб + сделать в тему этого репозитория гитхаб пейджес чтобы люди могли посмотреть на имитацию того что в итоге собрано."*

### 6.1. GitHub Repository Polish & Presentation Showcase
- [x] **Hero README & Presentation Overhaul (`README.md`)**:
  - Cleaned up `README.md` to present concise, verified engineering facts, BOM, RP2350 pinout table, and author attribution (**Dr. Andrey Holz** — [LinkedIn](https://www.linkedin.com/in/andrey-holz/)).
  - Added architectural flow diagrams for: (1) Primary `Staff ⇄ Android Phone Hotspot`, (2) Optional `Staff ⇄ Phone ⇄ Wearable Raspberry Pi 5 Local LLM Node`, and (3) `Next-Gen v2.0 Target Flow (2026 Best-Practice Roadmap)`.
  - Automated Android version sync (`v1.1.2`) in `.github/workflows/pages.yml`.

### 6.2. Interactive GitHub Pages Simulator (Web Digital Twin of Celestial Engine Mk.V)
- [x] **Themed Web Application hosted on GitHub Pages (`docs/`, live at `derandr.github.io/celestial_engine_project/`)**:
  - **Interactive 2D Physical Replica & Direct RP2350 Pinout Map**: High-DPI 2D Assembled view, Exploded view, and interactive RP2350 Pinout & Equipment Wiring Map with component isolation dimming (`staff-2d.js`, `breadboard-2d.js`).
  - **Interactive 43-LED Staff Head Simulation**: All **28 concentric LED patterns** (`0..27`) matching physical firmware (`leds.c`).
  - **Virtual SH1106 OLED Display Emulator (128×64)**: Live 20 FPS ticker, 13 modes, rotary encoder (`PEC11R`) & side `QUERY` button (`GP9`).
  - **Web Audio API Chiptune & Sound Engine**: All **21 onboard 8-bit chiptunes** and **14 Cosmic / R2-D2 / Spell Sound FX** (`simulator.js`).
  - **Live In-Browser Local LLM & Voice Input**: Top callout banner, 6 authoritative personas (`OraclePersona.kt` parity), English microphone speech capture (up to 40s with live 12-band FFT meter), grounded repository & author knowledge base (`repo-knowledge.js`), and in-browser `SmolLM2-135M` / `LFM2.5-350M` via Transformers.js v4.3.2 WebGPU/WASM (`worker.js`).

---

## 7. Future Task: Modern 2025/2026 Honest Offline Multilingual AI Pipeline (Focus on English)

> **User Directive (2026-10-29):**
> *"давай в задачи на будущее запишем честный offline многоязычный с фокусом на английский."*  
> *"проверь в интернете как сейчас делают такое на девайсе - мне кажется ты устаревший стек предложил"*

---

### 7.1. Technology Shift: Legacy 2022/2023 vs Modern 2025/2026 Edge AI Stack

| Pipeline Stage | ❌ Legacy 2022/2023 Stack (Sluggish, High RAM/CPU) | ⚡ Modern 2025/2026 State-of-the-Art Stack (Sub-Second, NPU/INT8) |
|---|---|---|
| **STT (Speech-to-Text)** | **OpenAI Whisper (whisper.cpp)**<br>Fixed 30-second window with zero-padding; wastes 90% compute on short 2s voice commands; heavy CPU draw. | **Moonshine (Useful Sensors, 2024/2025)**<br>Variable-length RoPE encoder-decoder. **5x faster than Whisper** for voice queries, ~26MB footprint, sub-80ms latency.<br>— *Alternative*: **SenseVoice-Small (FunASR/Sherpa-ONNX)** with 50x real-time speed & emotion detection. |
| **LLM Engine (Brain)** | **Naive llama.cpp / Ollama on CPU**<br>Slow Time-To-First-Token (TTFT), poor mobile battery life, no NPU utilization. | **Meta ExecuTorch** or **Google LiteRT-LM (TFLite successor)**<br>Direct mobile NPU/GPU hardware acceleration (Qualcomm QNN, MediaTek NeuroPilot, Apple Metal, ARM KleidiAI). Runs **Llama 3.2 1B/3B Instruct**, **Gemma 2 2B / Gemma 3 1B**, or **Qwen 2.5 1.5B** at 45–80 tok/s. |
| **TTS (Speech Synthesis)** | **Piper TTS (VITS 2022)** or **Android System TTS**<br>Robotic intonations, flat pitch, clipping artifacts, monotonic cadence. | **Kokoro-82M (Late 2024/2025)**<br>Ultra-compact 82M param model (~80MB in INT8 ONNX). Studio-grade 24 kHz natural speech (human breath, emotion, cadence comparable to ElevenLabs). RTF < 0.08 on mobile CPU/ONNX. Apache 2.0 license. |
| **End-to-End Alternative** | Not viable locally in 2023. | **Mini-Omni2 / Llama-Omni (Speech-to-Speech)**<br>Direct audio-in ➔ audio-out parallel autoregression; bypasses text tokens with **~226ms glass-to-glass latency**. |

---

### 7.2. End-to-End Pipeline Architecture (Modern Stack)

```
┌─────────────────────────────────────────────────────────────────────────────────────────────┐
│                   MODERN 2025/2026 ON-DEVICE OFFLINE AI VOICE PIPELINE                       │
└─────────────────────────────────────────────────────────────────────────────────────────────┘
  1. Staff Microphone (INMP441 MEMS)
         │  16 kHz 16-bit Mono PCM streamed over Wi-Fi (UDP port 5005)
         ▼
  2. Ultra-Fast Variable-Length STT (Speech-to-Text)
     ├── Primary: Moonshine (Useful Sensors) INT8 ONNX (~27M params, ~26 MB footprint)
     │   - Variable-length RoPE: No 30s zero padding, 5x faster than Whisper on short queries (<80ms)
     └── Multilingual / Emotion: SenseVoice-Small via Sherpa-ONNX (50x RTF, detects laughter/whisper/events)
         │  Verbatim transcript string (e.g. "Cast a solar flare into the darkness")
         ▼
  3. Accelerated Edge LLM Core (Reasoning & Spell Synthesis)
     ├── Android Host: Meta ExecuTorch (NPU/GPU) or Google LiteRT-LM
     │   - Models: Llama 3.2 1B/3B Instruct, Gemma 2 2B / Gemma 3 1B, or Qwen 2.5 1.5B INT4
     │   - Latency: TTFT ~120ms, throughput 50+ tokens/sec on mobile NPU
     └── Wearable SBC Host (Pi 5): llama.cpp with ARM KleidiAI acceleration (Llama 3.2 1B Q4_K_M)
         │  Generates dynamic persona prophecy + [SPELL: Solar Flare | ANIM: PLASMA | SOUNDS: ...]
         ▼
  4. Ultra-Realistic Neural TTS (Speech Synthesis)
     └── Kokoro-82M via ONNX Runtime Mobile / Sherpa-ONNX (INT8 ~80 MB)
         - 82M params, studio-quality 24 kHz speech with natural cadence, breaths, and emotional warmth
         - RTF < 0.08 (synthesizes 1 second of audio in under 80 ms on device CPU)
         │  Generates 16 kHz Mono PCM audio chunks
         ▼
  5. Staff Speaker (MAX98357A I2S + 43 WS2812B LEDs)
     Spoken vocal playback synchronized with real-time concentric polar lighting effects
```

---

### 7.3. Actionable Milestone Breakdown

#### Phase 1: Real On-Device STT — Moonshine & SenseVoice Integration
- [x] **Android Companion: Moonshine ONNX Runtime Integration (`OnDeviceSotaAudioEngine.kt`) — ✅ Completed (2026-11-15)**:
  - Integrated `com.microsoft.onnxruntime:onnxruntime-android:1.20.0` with on-demand download & caching of quantized `Moonshine Tiny INT8 ONNX` (`preprocess.onnx`, `encode.int8.onnx`, `uncached_decode.int8.onnx`, `cached_decode.int8.onnx`, `tokens.txt` ~26 MB) into `filesDir/sota_models/moonshine/` and zero-latency acoustic DSP fallback.
- [x] **Multilingual Support via SenseVoice-Small (`sherpa-onnx`) & Acoustic Emotion Tags — ✅ Completed (2026-11-21)**:
  - Integrated quantized `sherpa-onnx-sense-voice-zh-en-ja-ko-yue-2024-07-17` (`model.int8.onnx` + `tokens.txt`) in `OnDeviceSotaAudioEngine.kt` for seamless English (`[EN]`), German (`[DE]`), and Russian (`[RU]`) support.
  - Implemented `parseSenseVoiceRichTags` and `detectAcousticEventsFromPcm16` to capture acoustic emotion/event tags (`[laughter]`, `[applause]`, `[sigh]`, `[happy]`, `[sad]`) and pass them to `OfflineEdgeEngine.kt` and `OnDeviceLlmEngine.kt` for in-character persona humor.

#### Phase 2: Edge LLM Engine — On-Device Generative ONNX Runtime LLM
- [x] **Android Companion: On-Device Generative LLM Integration (`OnDeviceLlmEngine.kt`, `SmolLM2-135M-Instruct` INT8 ONNX) — ✅ Completed (2026-11-15)**:
  - Implemented `OnDeviceLlmEngine.kt` with a pure-Kotlin GPT-2 Byte-Level BPE Tokenizer (`ByteLevelBpeTokenizer`), ChatML framing (`<|im_start|>`/`<|im_end|>`), and full 30-layer KV-cache autoregressive token generation (`past_key_values.0..29` -> `present.0..29`) via `com.microsoft.onnxruntime:onnxruntime-android:1.20.0`.
  - Downloads `HuggingFaceTB/SmolLM2-135M-Instruct` (`onnx/model_int8.onnx` ~130.8 MB + `tokenizer.json`) on-demand into `filesDir/sota_models/smollm2/` and streams real neural tokens in `OfflineEdgeEngine.kt` (`modelTag = "SMOLLM2-ONNX"`).
- [x] **Raspberry Pi 5 SOTA Audio & LLM Optimization (Completed 2026-11-07)**:
  - Deployed `Moonshine Tiny INT8 ONNX` (`sherpa-onnx`), `Kokoro-82M INT8 ONNX` (`kokoro-onnx`, 4 Cortex-A76 threads), and multi-model RAM residency (`OLLAMA_MAX_LOADED_MODELS=3`, `keep_alive=-1`) on Bookworm 64-bit NVMe.

#### Phase 3: Ultra-Realistic Neural TTS — Kokoro-82M via ONNX
- [x] **Android Companion: Kokoro-82M Persona Voice Profiling & ONNX Pipeline (`KokoroVoiceProfile.kt`, `StaffTtsSynthesizer.kt`) — ✅ Completed (2026-11-15)**:
  - Mapped all 6 Oracle Personas (`MACHINE_GOD`, `ARCHMAGE`, `MARVIN`, `CYBERPUNK`, `PUMPKIN_LORD`, `CUSTOM`) to their canonical Kokoro-82M voice profiles (`am_adam`, `bm_george`, `af_bella`, `am_michael`, `af_heart`) with persona-calibrated pitch/speech-rate modulation and 16 kHz mono PCM streaming (`NET_MSG_RESP_AUDIO_CHUNK`).
- [x] **Voice Quality Calibration**:
  - Calibrated 22,000 peak normalization and persona vocal profiles in `StaffTtsSynthesizer.kt` for non-clipping playback on the staff's MAX98357A speaker.

#### Phase 4: Long-Term Horizon — End-to-End Speech-to-Speech (Omni)
- [ ] **Evaluate Mini-Omni2 / Llama-Omni on High-RAM Android**:
  - Benchmark direct audio-to-audio streaming on 16 GB RAM devices (Google Pixel 9 Pro XL / Galaxy S24 Ultra).
  - Target glass-to-glass conversational latency: **< 300 ms**.

---

## 8. SOTA Architectural Refactoring & Modular Modernization Plan

> **Goal:** Transform the project codebase from a rapid Halloween prototyping baseline (monolithic C files and interconnected Android ViewModel handlers) into a pristine, modular, modern State-of-the-Art (2025/2026) edge computing architecture.
> **Philosophy:** Respect the physical reality (a joyful, festive Halloween cyber-flask with foam beads and chiptunes), while enforcing enterprise-grade engineering rigor under the hood.

### 8.1. Firmware Decomposition (Raspberry Pi Pico 2 W / RP2350)

Currently, `firmware/src/main.c` (~134 KB) and `firmware/src/audio.c` (~97 KB) handle multiple overlapping responsibilities (UI rendering, state dispatch, DMA management, UDP packet parsing, and chiptune synthesis).

```
firmware/src/ (Target Refactored Layout)
├── drivers/                   # Hardware Abstraction Layer (HAL)
│   ├── sh1106_i2c.c/h         # I2C1 400kHz OLED driver (GP2/GP3)
│   ├── inmp441_pio.c/h        # PIO0 SM0 I2S RX driver (GP10/11/12)
│   ├── max98357a_pio.c/h      # PIO1 SM0 I2S TX driver (GP16/17/18) with auto-sleep
│   ├── ws2812_pio.c/h         # PIO0 SM1 WS2812B driver (GP15) with 18% safety cap
│   ├── pec11r_encoder.c/h     # Quadrature IRQ decoder (GP6/GP7) + Click SW (GP8)
│   └── query_button.c/h       # Debounced PTT active-low button (GP9)
├── net/                       # Zero-Latency Networking
│   ├── udp_transport.c/h      # CYW43 Wi-Fi socket, non-blocking receive on port 5005
│   ├── packet_codec.c/h       # FNV-1a authenticated UDP v2 binary serialization
│   └── watchdog_net.c/h       # 1000ms packet loss detector & autonomous failover trigger
├── audio/                     # Audio Processing & Synthesizer
│   ├── chiptune_core1.c/h     # Core 1 dedicated 16 kHz square-wave synth (24 tracks)
│   ├── pcm_streamer.c/h       # Ring buffer DMA feeder with hardware silence muting
│   └── track_catalog.c/h      # Note arrays, ADSR envelope definitions & track metadata
├── core/                      # Application Logic & FSM
│   ├── staff_fsm.c/h          # Central state machine (BOOT, IDLE, LISTENING, SPEAKING, STANDALONE)
│   ├── menu_system.c/h        # Cybernetic HUD menus (13 modes, scrolling ticker)
│   └── event_queue.c/h        # Lock-free inter-core event queue (Core 0 <-> Core 1)
└── main.c                     # Minimal entry point: hardware_init(), core1_launch(), event_loop()
```

- [x] **Task 8.1.1: Driver Isolation — ✅ Completed (2026-11-08)**:
  - Extracted `ws2812_driver` (`firmware/src/ws2812_driver.c/h`) encapsulating PIO0 SM1 + DMA channel claiming, configuration, non-blocking transmission, and latch timing.
  - Extracted `max98357a_driver` (`firmware/src/max98357a_driver.c/h`) encapsulating PIO1 SM0 + double-buffered ping-pong DMA with circular ring buffers, RP2350-E5 chaining errata safety, hardware sleep/wake, and live IRQ latency profiling.
  - Decoupled `firmware/src/leds.c` and `firmware/src/audio.c`, stripping low-level DMA and PIO register poking out of high-level sequencer logic.
- [x] **Task 8.1.2: Core 1 Dedicated Chiptune Sandbox & Modular Synthesizer — ✅ Completed (2026-11-08)**:
  - Extracted `chiptune_synth` (`firmware/src/chiptune_synth.c/h`) containing all 21 8-bit chiptune score arrays (Super Mario, Tetris, Doom E1M1, Zelda, Imperial March, etc.), 14 Cosmic SFX sequences, and sample-accurate synthesis rendering (`0.65*s1 + 0.25*s3 + 0.10*s5` arcade pulse wave with anti-click envelopes).
  - Reduced `firmware/src/audio.c` from 1,927 lines down to 705 lines.
  - Verified with 29 passing unit tests (including all 21 MSVC host firmware regressions in `test_regressions.py`).
  - Flashed `celestial_firmware.uf2` to physical Pico 2 W over `picotool load -f -x` on COM9; verified live heartbeat telemetry, chime ('T'), chiptune ('M 0'), and audio diagnostics ('A') with 0 late refills and 0 TX stalls.
- [x] **Task 8.1.3: LED Patterns & Ring Animators Extraction — ✅ Completed (2026-11-09)**:
  - Extracted 35 LED animation algorithms (~2,400 lines) from `firmware/src/leds.c` into `firmware/src/led_patterns.c/h`, preserving all 28 menu animations, 7 dynamic state animations (Vortex, Speech Expand, Data Receive, Galaxy, Solar, Earth, 67 Easter Egg), chiptune audio sync, and 4-ring diagnostic hooks.
  - Reduced `firmware/src/leds.c` from 2,660 lines down to **228 lines**.
- [x] **Task 8.1.4: UI Screens & SH1106 Card Presentation Extraction — ✅ Completed (2026-11-09)**:
  - Extracted all 14 screens and 6 card renderers (Galaxy, Solar System, Earth, Celestial Clock with 4 astronomical styles, Dynamic Architecture, and Dashboard) from `firmware/src/main.c` into `firmware/src/ui_screens.c/h`.
  - Reduced `firmware/src/main.c` from 3,067 lines down to **1,303 lines**.
  - Decoupled OLED presentation logic from Pico 2 W core event loop and networking state machine.
  - Verified full test suite (41/41 tests passing) and flashed to physical Pico 2 W over COM9 (`Tick #2..#4 | OLED: ONLINE | State: IDLE`).
- [x] **Task 8.1.5: Hardware Watchdog & Power Guard — ✅ Active in `firmware/src/main.c`**:
  - RP2350 hardware watchdog (`watchdog_enable(8000, 1)`, `watchdog_caused_reboot()`, `watchdog_update()`) active alongside the 1000ms network timeout and 18% WS2812B brightness cap.
- [x] **Task 8.1.6: Firmware Minimal Entry Point & Event Dispatcher (`firmware/src/event_dispatcher.c/h`) — ✅ Completed (2026-11-09)**:
  - Extracted the high-level input/event state machine, telemetry broadcasting, chiptune LED synchronization, and serial/remote command routing from `firmware/src/main.c` into a dedicated `event_dispatcher.c/h`.
  - Reduced `firmware/src/main.c` from 1,303 lines down to **148 lines** — a minimal, pristine entry point dedicated strictly to hardware clock init, peripheral setup, Core 1 launch, and the top-level tick scheduler.
  - Verified with 41 passing unit tests (including all 21 MSVC host firmware regressions) and verified on physical hardware via COM9.

---

### 8.2. Android Companion Modernization (MVI & State-of-the-Art 2025/2026 AI)

Modernize [`android/`](android/) by isolating the UDP networking, deterministic Spell Forge, and AI inference engines into clean, testable layers.

```
com.celestial.companion/
├── core/
│   ├── transport/             # Network Infrastructure
│   │   ├── StaffUdpTransport.kt    # DatagramSocket bound to hotspot interface (port 5005)
│   │   ├── UdpPacketCodec.kt       # Binary protocol framing & FNV-1a integrity
│   │   └── HeartbeatMonitor.kt     # Keepalive, latency tracking & auto-reconnect
│   └── service/
│       └── StaffBrainService.kt    # Foreground service keeping UDP socket active during screen off
├── domain/
│   ├── model/                 # State & Data Models
│   │   ├── StaffState.kt           # Current operating mode, battery, volume, rune colors
│   │   └── SpellDefinition.kt      # Keyword triggers, RGB signatures, duration
│   └── spells/
│       └── SpellForge.kt           # Sub-20ms regex/acoustic deterministic spell compiler
├── ai/
│   ├── engine/
│   │   ├── AiModelEngine.kt        # Unified interface: generateSpeechResponse(audioChunk)
│   │   ├── CloudGeminiEngine.kt    # Gemini 2.5 Flash cloud streaming via OpenRouter (Default)
│   │   └── OfflineSotaEngine.kt    # Moonshine STT + LiteRT/ExecuTorch LLM + Kokoro-82M TTS
│   └── tts/
│       ├── CloudTtsStreamer.kt     # Google Cloud TTS float PCM chunker
│       └── KokoroTtsStreamer.kt    # On-device Kokoro-82M INT8 ONNX synthesizer
└── ui/                        # Presentation (Jetpack Compose + Material 3)
    ├── screens/
    │   ├── dashboard/              # 33 Hz 43-LED visualizer, status chips, quick cast
    │   ├── cosmos/                 # Multi-scale Keplerian astronomy viewport
    │   └── terminal/               # Bounded Oracle log console & model switcher
    └── theme/                      # Cyber-Arcane dark mode palette & glowing shaders
```

- [x] **Task 8.2.1: Spell Forge Decoupling (`FastSpellMatcher.kt` & `CelestialProphecy.kt`) — ✅ Completed (2026-11-07)**:
  - Isolated `FastSpellMatcher` and `CelestialProphecy` with 10 JUnit 4 unit tests (`FastSpellMatcherTest.kt`, `CelestialProphecyTest.kt`) verifying sub-20ms EN/RU/DE spell trigger matching and tag stripping.
- [x] **Task 8.2.2: Dual Engine Provider & 100% Offline Edge Mode — ✅ Completed (2026-11-08)**:
  - Implemented `AiEngineMode` (`AUTO_FALLBACK`, `OFFLINE_EDGE`, `CLOUD_ONLY`) with persistent SharedPreferences storage.
  - Implemented `OfflineEdgeEngine` featuring sub-20ms regex/acoustic spell compilation, 6 multilingual persona prophecy banks (RU/EN/DE, $\le 25$ words), and full `GemmaStreamChunk` streaming lifecycle.
  - Implemented `DualEngineProvider` implementing `CelestialAiEngine` with zero-friction auto-fallback when API keys are blank or network fails.
  - Added interactive 3-chip UI selectors (`[🔄 AUTO]`, `[⚡ OFFLINE]`, `[☁️ CLOUD]`) in `DashboardScreen.kt` and `OracleTerminalScreen.kt`.
  - Added full JUnit test suite in `DualEngineProviderTest.kt` verifying spell matching, persona boundaries, and streaming lifecycle.
- [x] **Task 8.2.3: Zero-Recomposition Canvas Optimization (`ConcentricLedRingsView.kt`) — ✅ Completed (2026-11-07)**:
  - Replaced `Canvas` with `Modifier.drawWithCache` + `onDrawBehind` and precomputed normalized polar LED coordinates (`LED_UNIT_COS`, `LED_UNIT_SIN`) to eliminate per-frame allocations and Compose layout passes during 33 Hz UDP packet bursts.
- [x] **Task 8.2.4: Dedicated UDP Transport, Foreground Service & Clean Memory Unload (`StaffUdpTransport.kt`, `StaffBrainService.kt`, `CelestialApp.kt`) — ✅ Completed (2026-11-15)**:
  - Isolated `StaffUdpTransport` (`:5005`) with full `MulticastLock` and dual-socket release in `stop()`, plus `StaffRepository.shutdown()` canceling all background heartbeat/discovery coroutines and shutting down `StaffTtsSynthesizer`.
  - Updated `StaffBrainService` (`START_NOT_STICKY`, `onTaskRemoved`, and notification `⏻ Shut Down` action) and added `CelestialApp.shutdownAndExit()` + top-bar **`[⏻ EXIT]`** button in `DashboardScreen.kt` so the user can deterministically close the app, remove the task from Recents (`finishAndRemoveTask()`), and unload the process (`killProcess` / `exitProcess(0)`) with zero lingering background threads or RAM consumption.
- [x] **Task 8.2.5: Android Companion On-Device SOTA Audio Engine (Moonshine STT + Kokoro-82M TTS via ONNX Runtime) — ✅ Completed (2026-11-15)**:
  - Integrated `com.microsoft.onnxruntime:onnxruntime-android:1.20.0` (`OnDeviceSotaAudioEngine.kt`) with on-demand download & local caching of quantized `Moonshine Tiny INT8 ONNX` (`preprocess.onnx`, `encode.int8.onnx`, `uncached_decode.int8.onnx`, `cached_decode.int8.onnx`, `tokens.txt`), autoregressive `OrtSession` greedy decoding, and deterministic `close()` on shutdown.
  - Added `KokoroVoiceProfile.kt` mapping all 6 Oracle Personas (`MACHINE_GOD`, `ARCHMAGE`, `MARVIN`, `CYBERPUNK`, `PUMPKIN_LORD`, `CUSTOM`) to their canonical Kokoro-82M voice profiles (`am_adam`, `bm_george`, `af_bella`, `am_michael`, `af_heart`) with persona-specific pitch and speech-rate modulation in `StaffTtsSynthesizer.kt` and `OfflineEdgeEngine.kt`, plus a Dashboard SOTA Audio status/download card and JUnit 4 unit tests (`OnDeviceSotaAudioEngineTest.kt`).
- [x] **Task 8.2.6: MVI Architecture & ViewModel Decomposition (`StaffUiState.kt`, `GrimoireStorageManager.kt`, `DashboardViewModel.kt`) — ✅ Completed (2026-11-15)**:
  - Extracted `StaffUiState`, `StaffIntent` (`dispatchIntent`), and `OracleChatMessage` into `StaffUiState.kt` and isolated custom spell/voice preference persistence into `GrimoireStorageManager.kt`.

---

### 8.3. Physical Twin & Showcase on GitHub Pages — ✅ COMPLETED (2026-10-29)

- [x] **Store Real Hardware Photo**: Stored physical artifact photo in [`docs/assets/celestial_flask_physical.jpg`](assets/celestial_flask_physical.jpg) (`136 KB`) and [`docs/assets/celestial_flask_physical.webp`](assets/celestial_flask_physical.webp) (`91 KB`).
- [x] **Interactive Exploded vs Assembled vs 2D Pinout View**:
  - Added interactive switcher (`🔮 Assembled`, `🧩 Exploded`, `🔌 2D Pinout Map`) with High-DPI scaling and component isolation dimming.
- [x] **Main-Page DIY & Halloween Prop Disclaimer**:
  - Replaced standalone `halloween.html` with a clean main-page disclaimer banner on `docs/index.html` and `README.md`.

---

### 8.4. Bidirectional Mesh Host Routing & Remote Locking (`v1.3.4`) — ✅ COMPLETED (2026-11-08)

- [x] **Firmware Host Locking Symmetry (`firmware/src/net_client.c`)**:
  - Eliminated bug where incoming companion phone catalog responses unconditionally reset `preferred_host` to `HOST_TYPE_PHONE`.
  - Added explicit guard so `[LOCK PI 5]` and `[PI5 LOCK]` can never be overridden by background phone packets.
  - Retained automatic failover when in `AUTO MESH` mode (`HOST_TYPE_NONE`).
- [x] **Android Companion Remote Routing & Conflict Prevention (`StaffRepository.kt`)**:
  - Added `_phoneBrainActive = (command.hostType != 1)`: selecting `[LOCK PI 5]` on the phone deactivates local query interception and catalog push to prevent competing with the Raspberry Pi 5 node.
  - Verified authenticated command dispatch (`APP_COMMAND 0x20` with code `'H'`: `0 = Auto Mesh`, `1 = Pi 5`, `2 = Phone`).
- [x] **Hardware & Live Ecosystem Verification**:
  - Compiled and flashed `celestial_firmware.uf2` to physical Pico 2 W (`RP2350`, `COM9`, `192.168.178.101`).
  - Compiled and deployed `celestial_companion_v1.3.4.apk` to Pixel 9 Pro XL (`49051FDAS001YK`).
  - Verified telemetry reflects `Host: PI 5 BRAIN` and stays locked across multiple network intervals.

---

### 8.5. Showcase & Documentation Overhaul (README Simplification & Dual-View Web Pages) — ✅ COMPLETED (2026-11-14)

- [x] **Task 8.5.1: GitHub `README.md` Simplification & De-cluttering — ✅ Completed (2026-11-14)**:
  - Streamlined `README.md` from a 270-line dense document into a crisp, high-impact 155-line GitHub repository front page.
  - Highlighted the physical cyber-flask hero photo (`celestial_flask_physical.jpg`, `136 KB`), Dual-Mode GitHub Pages showcase links, 3 clean Mermaid diagrams, and a concise architecture summary.
  - Moved exhaustive pinout matrices, modular firmware breakdowns, and CI/CD pipelines into [`docs/architecture.md`](architecture.md), [`docs/wiring/pinout.md`](wiring/pinout.md), and [`docs/protocol.md`](protocol.md).
- [x] **Task 8.5.2: Dual-Mode GitHub Pages (`✨ Quick Demo` vs `🔬 Deep Engineering Lab`) — ✅ Completed (2026-11-14)**:
  - Implemented a persistent dual-mode view toggle (`#mode-btn-demo` / `#mode-btn-lab`) at the top of `docs/index.html` with URL (`?mode=lab`) and `localStorage` state persistence:
    - **`✨ Quick Demo` (Default / Lite)**: Fast, clutter-free showcase featuring the physical staff in action, in-browser Local LLM Oracle chat, 43-LED concentric dome animator, 8-bit chiptune jukebox, 14 SFX, and release downloads.
    - **`🔬 Deep Engineering Lab` (Full Technical View)**: Unlocks the 2D RP2350 Pinout & Breadboard CAD Explorer, Binary UDP v2 Packet Framing Inspector, Duplex PIO/DMA I2S Audio Architecture, Dual-Brain Edge AI topology, and the new **Modular Firmware & Android CI/CD** tab (`#tab-cicd`).
- [x] **Task 8.5.3: Documentation Parity & Modular Architecture Synchronization — ✅ Completed (2026-11-14)**:
  - Created [`docs/architecture.md`](architecture.md) documenting the Phase 8.1 modular firmware layout (`main.c` [148 lines], `event_dispatcher.c`, `ui_screens.c`, `led_patterns.c`, `chiptune_synth.c`, `ws2812_driver.c`, `max98357a_driver.c`), Dual-Core PIO/DMA pipelines, and Bidirectional Mesh Host Locking.
- [x] **Task 8.5.4: Document Android Companion CI/CD Build, GitHub Actions Auto-Release & ADB Deployment Pipeline — ✅ Completed (2026-11-14)**:
  - Documented the end-to-end Android Companion build and deployment architecture in [`README.md`](../README.md), [`docs/architecture.md`](architecture.md), [`docs/index.html`](index.html) (`#tab-cicd`), and [`.agents/skills/android-compose-development/SKILL.md`](../.agents/skills/android-compose-development/SKILL.md):
    1. **Cloud APK Build & Release Clobbering (`.github/workflows/android-build.yml`)**:
       - Triggered automatically on pushes to `android/**`, `release`, or manual `workflow_dispatch`.
       - Compiles `./gradlew assembleDebug --no-daemon` on `ubuntu-latest` (Temurin JDK 17 + Gradle cache), eliminating the need for a local Android Studio installation.
       - Extracts `versionName` from `android/app/build.gradle.kts`, renames `app-debug.apk` to `celestial_companion_v${ANDROID_VER}.apk`, uploads the workflow artifact (`celestial-companion-v${ANDROID_VER}-apk`), and on `master` automatically updates the latest GitHub Release (`gh release upload "$LATEST_TAG" "celestial_companion_v${ANDROID_VER}.apk" --clobber`).
    2. **Dynamic GitHub Pages Version Synchronization (`.github/workflows/pages.yml`)**:
       - Triggered on pushes to `docs/**` or `android/app/build.gradle.kts`.
       - Dynamically parses `versionName` from `android/app/build.gradle.kts` and runs `sed -i` on `docs/index.html` during deployment so APK download links (`celestial_companion_v*.apk`) and `Android 15 Compose v*` badges on the live site never drift out of sync.
    3. **Dual-Path Physical Smartphone Deployment (`Pixel 9 Pro XL` via `adb`)**:
       - **Path A (Cloud GitHub Actions -> `gh` CLI -> `adb`)**: Push changes to `android/`, monitor run via `gh run list --workflow=android-build.yml`, download compiled APK into git-ignored `scratch/apk/` (`gh release download --pattern "celestial_companion_*.apk" -D scratch/apk --clobber` or `gh run download <run-id> -D scratch/apk`), install via `adb -s 49051FDAS001YK install -r scratch/apk/celestial_companion_v*.apk`, launch via `adb shell am start -n com.celestial.companion/.MainActivity`, and remove `scratch/apk/`.
       - **Path B (Local Gradle -> `adb`)**: Run `cd android && .\gradlew assembleDebug` followed by `adb install -r android/app/build/outputs/apk/debug/app-debug.apk`.

---

## 9. SOTA Phase-Next Implementation Blueprint & Anti-Defect Playbook (For Smaller / Faster Models)

> **Purpose:** This section is a strict, zero-ambiguity handbook written specifically for smaller/faster coding models (`flash`, `flash_lite`, or delegated subagents). Every rule below was derived from real defects caught during the Phase 9 audit on branch `feat/sota-phase-next`. Follow every check literally before declaring a task done.

---

### 9.0. Anti-Defect ("Анти-Брак") Rules & Self-Audit Checklist for Smaller Models

Before marking any task complete, a model **MUST** audit its own diff (`git diff`) against these **7 Golden Anti-Defect Rules**:

1. **ZERO STUBS & ZERO SILENT FALLBACKS (`No Fake Implementations`):**
   - **FORBIDDEN:** Returning `null`, `emptyList()`, or `ByteArray(0)` with a comment like `// Placeholder for future model` or `// Delegate to fallback for now`.
   - **FORBIDDEN:** Calling a legacy fallback inside a new engine branch while returning the new engine's label (for example, calling `runMoonshineGreedyDecode()` inside the `SotaSttModel.SENSE_VOICE_SMALL` branch and labeling the result `"SenseVoice-Small INT8 ONNX"`).
   - **REQUIRED:** Every `OrtSession` (`preprocess`, `encode`, `uncachedDecode`, `cachedDecode`, `senseVoice`, `kokoro`) must be invoked via `session.run(inputs)`, its output tensors parsed, and its pure-Kotlin pre/post-processing helpers verified by a dedicated JUnit 4 unit test with non-trivial synthetic tensors.
2. **EXACT ONNX TENSOR TYPES (`INT32` vs `INT64`) & INPUT ORDERING:**
   - Many `sherpa-onnx` models (`Moonshine`, `SenseVoice`) declare `int32_t` tensor inputs (`features_len`, `tokens`, `seq_len`, `x_length`, `language`, `text_norm`), while HuggingFace Optimum models (`SmolLM2-135M`, `Kokoro-82M`) declare `int64_t` (`LongBuffer`).
   - **NEVER hardcode `LongBuffer` without checking the session's `TensorInfo.type`!** Passing `LongBuffer` (`INT64`) to an `INT32` ONNX input throws `OrtException: Unexpected input data type. Actual: (tensor(int64)) , expected: (tensor(int32))` at runtime.
   - **ALWAYS use `createTypedIntTensor(env, session, inputName, values, shape)`** (implemented in `OnDeviceSotaAudioEngine.kt`), which inspects `(session.inputInfo[inputName]?.info as? TensorInfo)?.type == OnnxJavaType.INT32` and creates either an `IntBuffer` or a `LongBuffer` automatically.
   - **ALWAYS verify exact input indices against the C++/Python exporter:**
     - `Moonshine uncached_decode.int8.onnx`: `input[0] = tokens` (`int32[1, 1] = [[1]]`), `input[1] = encoder_out` (`float32[1, T, D]`), `input[2] = seq_len` (`int32[1] = [1]`). Outputs: `output[0] = logits`, `output[1..K] = states` (KV cache).
     - `Moonshine cached_decode.int8.onnx`: `input[0] = tokens` (`int32[1, 1] = [[lastToken]]`), `input[1] = encoder_out`, `input[2] = seq_len` (`int32[1] = [step + 1]`), `input[3..K+2] = states` (`kvOffset = 3`).
     - `SenseVoice model.int8.onnx`: `x` (`float32[1, T_lfr, 560]` with `LFR m=7, n=6` + `CMVN neg_mean/inv_stddev`), `x_length` (`int32[1]`), `language` (`int32[1]`, `0=auto, 3=en`), `text_norm` (`int32[1]`, `14=with_itn, 15=woitn`).
     - `Kokoro kokoro-v1.0.int8.onnx`: `tokens` or `input_ids` (`int64[1, N + 2] = [[0, *phoneme_ids, 0]]`), `style` (`float32[1, 256]` extracted from row `min(N, 510) - 1` of `<voiceId>.npy` inside `voices-v1.0.bin`), `speed` (`float32[1]`).
3. **RP2350 EMBEDDED STACK & ISR SAFETY (`firmware/src/`):**
   - **NEVER allocate buffers `> 256 bytes` on the stack** inside lwIP UDP receive callbacks (`net_client.c: udp_rx_callback`), PIO/DMA ISRs (`audio.c`), or deep call chains on Core 0/Core 1.
   - Always declare large scratch arrays as `static` in BSS (e.g., `static int16_t adpcm_pcm[NET_MAX_PAYLOAD_SIZE * 2];`) when accessed only from the single-threaded lwIP callback context, and always compile with `-Wall -Wextra` (`0 warnings` policy).
4. **NEVER WEAKEN TESTS TO HIDE AN ALGORITHM BUG:**
   - **FORBIDDEN:** If an SNR test fails at `500 Hz` (`SNR < 28 dB`), **NEVER** lower the test frequency to `200 Hz` or lower the threshold to `18 dB` to make the test green.
   - **REQUIRED:** Fix the algorithm! (For example, in `adpcm_codec.{c,kt,py}`, initializing `step_index` in the 4-byte block header from the first sample delta `|pcm[1] - pcm[0]|` eliminated slope-overload distortion on `500 Hz` full-scale sine waves and achieved `SNR > 30 dB`).
5. **KOTLIN 2.0 & ANDROID JVM TEST COMPILER RULES:**
   - **Kotlin 2.0 Inline Lambda Rule:** Never place `break` or `continue` inside `.use { ... }` lambdas in Kotlin (causes `:app:compileDebugKotlin FAILED: The feature "break continue in inline lambdas" is experimental`). Always write explicit `val res = session.run(inputs); try { ... } finally { res.close() }`.
   - **JUnit 4 Backtick Method Names:** Never use `<`, `>`, `.`, `;`, `[`, `]`, or `/` inside backtick test method names (`fun \`...\`()`) in `android/app/src/test/` (causes `:app:compileDebugUnitTestKotlin FAILED: Name contains illegal characters`).
   - **Pure JVM JSON & Android Stubs:** Keep `testImplementation("org.json:json:20240303")` and `testOptions { unitTests.isReturnDefaultValues = true }` in `android/app/build.gradle.kts`.
6. **WEIGHTS & CERTIFICATES HYGIENE:**
   - **Never commit weights (`*.onnx`, `*.bin`, `*.apk`) to Git.** All ONNX models are downloaded on-demand into `context.filesDir/sota_models/<model_name>/` using atomic `.part` rename.
   - **Keep `android/app/celestial-debug.keystore` pinned** in `android/app/build.gradle.kts` so cloud CI APKs and local APKs share the exact same signature (`adb install -r` upgrades in-place without uninstalling or losing downloaded ONNX models).
7. **BRANCH & CLEAN WORKSPACE DISCIPLINE:**
   - Work on the feature branch (`feat/sota-phase-next`) and **NEVER merge into `master`** until the user explicitly commands it.
   - Never leave temporary files in the repository root; use `scratch/apk/` for temporary APK downloads and clean up only `scratch/apk/` (`Remove-Item -Recurse -Force scratch/apk`).

---

### 9.1. Completed Foundation Tasks on `feat/sota-phase-next` (`Tasks 9.1–9.4`)

- [x] **Task 9.1: Full Greedy Autoregressive Moonshine Decoder & SenseVoice CTC Decoder (`OnDeviceSotaAudioEngine.kt`) — ✅ Completed & Audited (`7645fde`)**:
  - Implemented `decodeCtcGreedyLogits`, `createTypedIntTensor` (auto-matching `INT32` vs `INT64`), `extractSenseVoiceLfrFeatures` (`80-bin` log-Mel filterbank + `LFR m=7, n=6` -> `560-dim` + `CMVN neg_mean/inv_stddev` from ONNX metadata), `runSenseVoiceCtcDecode`, and `runMoonshineGreedyDecode` (`uncached_decode.int8.onnx` -> `cached_decode.int8.onnx` with `seq_len` at `input[2]` and `kvOffset = 3`).
  - Covered by 14 JUnit 4 unit tests in `OnDeviceSotaAudioEngineTest.kt` (including `extractSenseVoiceLfrFeatures` dimension checks and `decodeCtcGreedyLogits`).
- [x] **Task 9.2: Genuine On-Device Kokoro-82M INT8 ONNX Neural Synthesizer (`KokoroOnnxSynthesizer.kt` & `StaffTtsSynthesizer.kt`) — ✅ Completed & Audited (`7645fde`)**:
  - Implemented on-demand download of `kokoro-v1.0.int8.onnx` (`88.1 MB`) and `voices-v1.0.bin` (`26.9 MB` NPZ archive) from `thewh1teagle/kokoro-onnx` (`model-files-v1.0`).
  - Implemented `parseNpyStyleRow` and `extractVoiceStyleFromNpzStream` to parse the 256-float style vector for token length `N` (`row = min(N, 510) - 1`) from `<voiceId>.npy` inside `voices-v1.0.bin`.
  - Embedded the 178-entry `KOKORO_VOCAB` (`config.json`), implemented `englishToKokoroTokens(text)` (`[[0, *tokens, 0]]`), wired `kokoroSession.run(inputs)` + `resampleFloat24kToPcm16kMono(samples24k, targetPeak = 22000)` into `StaffTtsSynthesizer.kt`, and added UI download progress to `DashboardScreen.kt`.
- [x] **Task 9.3: 4:1 IMA-ADPCM Low-Bandwidth Wireless Audio Codec (RP2350 C + Kotlin + Python Parity) — ✅ Completed & Audited (`7645fde`)**:
  - Implemented [`firmware/src/adpcm_codec.h`](../firmware/src/adpcm_codec.h), [`firmware/src/adpcm_codec.c`](../firmware/src/adpcm_codec.c), [`ImaAdpcmCodec.kt`](../android/app/src/main/java/com/celestial/companion/audio/ImaAdpcmCodec.kt), and [`tools/adpcm_codec.py`](../tools/adpcm_codec.py) with adaptive initial `step_index` in the 4-byte block header (`[int16 predictor][uint8 step_index][uint8 reserved=0]`), achieving `SNR > 30 dB` on a `500 Hz` full-scale sine wave (`960 samples` per `484-byte` UDP chunk).
  - Wired downlink negotiation (`NET_MSG_RESP_AUDIO_START` 8-byte payload `[uint32 sample_count][uint8 codec_id=1][3x0]` + `NET_MSG_RESP_AUDIO_CHUNK`) across `firmware/src/net_dispatcher.c`, `firmware/src/net_client.c` (using BSS `static int16_t adpcm_pcm[NET_MAX_PAYLOAD_SIZE * 2]`), `StaffRepository.kt`, and `tools/brain_transport.py`.
- [x] **Task 9.4: End-to-End Speech-to-Speech (`Omni`) Latency Harness (`OmniAudioSession.kt` & `tools/omni_benchmark.py`) — ✅ Completed & Audited (`7645fde`)**:
  - Implemented [`OmniAudioSession.kt`](../android/app/src/main/java/com/celestial/companion/ai/OmniAudioSession.kt) and [`tools/omni_benchmark.py`](../tools/omni_benchmark.py) with `OmniLatencyMetrics` (`TTFA`, `glassToGlassLatencyMs`, `firstSpellDispatchMs`, `meetsSub300msSla`) and 5 JUnit 4 unit tests in `OmniAudioSessionTest.kt`.

---

### 9.2. Foolproof Step-by-Step Verification Playbook (Copy-Paste Commands)

Every model must run **all 3 verification stages** below after making any code change and verify that the output matches the expected result word-for-word.

#### Stage 1: Local RP2350 Firmware Build, Host C/Python 47-Test Suite & Digital Twin Acceptance
Run this single chained PowerShell command from the repository root (`c:\projects\personal\hardware-01\celestial_engine_project`):
```powershell
cmake --build firmware/build --target celestial_firmware --parallel 4 ; python -m unittest discover -s tests -v ; python tools/test_twin_acceptance.py
```
- **Expected Output Criteria (MUST ALL PASS):**
  1. `celestial_firmware.uf2` compiles with **`0 warnings` and `0 errors`** under `-Wall -Wextra`.
  2. `python -m unittest discover -s tests -v` outputs **`Ran 47 tests` ... `OK`** (or `47+` as new tests are added), including `test_ima_adpcm_4to1_roundtrip_snr_and_Start_packet_framing ... ok`.
  3. `python tools/test_twin_acceptance.py` outputs **`ALL ACCEPTANCE CHECKS PASSED`**.

#### Stage 2: Cloud Android CI Compilation & Full JUnit 4 Test Suite (`44+` Tests)
Because Android builds run in GitHub Actions (`.github/workflows/android-build.yml`, which executes `./gradlew testDebugUnitTest assembleDebug --no-daemon`), push your commit to `feat/sota-phase-next` and watch the run to completion:
```powershell
git push origin feat/sota-phase-next
Start-Sleep -Seconds 4
$runId = (gh run list --workflow=android-build.yml --branch=feat/sota-phase-next --limit 1 --json databaseId -q '.[0].databaseId')
gh run watch $runId --exit-status
```
- **If the run fails (`exit code != 0`), inspect the exact compiler/test failure immediately:**
  ```powershell
  gh run view $runId --log-failed
  ```
- **Expected Output Criteria:**
  - `Run Unit Tests & Build Debug APK with Gradle` -> **`✓`** (`BUILD SUCCESSFUL`).

#### Stage 3: Live Hardware Flash & On-Device Verification (`Pico 2 W` + `Google Pixel 9 Pro XL`)
1. **Flash `celestial_firmware.uf2` to Pico 2 W (`RP2350` on `COM11`):**
   ```powershell
   C:\Users\AndrD\.pico-sdk\picotool\2.2.0-a4\picotool\picotool.exe load -f -x firmware/build/celestial_firmware.uf2
   ```
2. **Install the Newly Built Branch APK onto `Google Pixel 9 Pro XL` (`49051FDAS001YK`):**
   ```powershell
   $runId = (gh run list --workflow=android-build.yml --branch=feat/sota-phase-next --limit 1 --json databaseId -q '.[0].databaseId')
   Remove-Item -Recurse -Force scratch/apk -ErrorAction SilentlyContinue
   gh run download $runId -D scratch/apk
   $apk = (Get-ChildItem -Path scratch/apk -Filter "celestial_companion_*.apk" -Recurse | Select-Object -First 1).FullName
   adb -s 49051FDAS001YK install -r "$apk"
   adb -s 49051FDAS001YK shell am start -n com.celestial.companion/.MainActivity
   Remove-Item -Recurse -Force scratch/apk -ErrorAction SilentlyContinue
   ```
3. **Verify Live Neural Inference & 4:1 IMA-ADPCM Streaming in `adb logcat`:**
   ```powershell
   adb -s 49051FDAS001YK logcat -d | Select-String -Pattern "OnDeviceSotaAudio|OnDeviceLlmEngine|KokoroOnnxSynthesizer|StaffTtsSynthesizer|StaffRepository" | Select-Object -Last 40
   ```
   - Look for:
     - `OnDeviceSotaAudio: Moonshine Tiny INT8 ONNXsessions loaded` (or `SenseVoice-Small INT8 ONNX session loaded`)
     - `OnDeviceLlmEngine: SmolLM2-135M-Instruct generated ... tokens in ... ms (... tok/s)`
     - `KokoroOnnxSynthesizer: Kokoro-82M synthesized ... chars` -> `StaffTtsSynthesizer: Synthesized with On-Device Kokoro-82M INT8 ONNX`
     - `StaffRepository: Streaming TTS via 4:1 IMA-ADPCM (codec_id=1)`

---

## 10. Next-Step Implementation Specifications for Smaller Models (`Tasks 9.5–9.8`)

> **Instructions for the Implementing Model:** Implement Tasks `9.5`, `9.6`, `9.7`, and `9.8` in sequence on branch `feat/sota-phase-next`. Each task has an exact file list, function signature, algorithm specification, and verification command. Do **NOT** invent shortcuts, do **NOT** leave `TODO` stubs, and run the verification command after each task.

---

### Task 9.5: Uplink 4:1 IMA-ADPCM Microphone Compression (`Pico 2 W INMP441 -> Phone / Pi 5`)
- [x] **Objective**: Currently, **downlink** TTS audio (`Phone/Pi 5 -> Pico 2 W` via `NET_MSG_RESP_AUDIO_START` + `NET_MSG_RESP_AUDIO_CHUNK`) uses 4:1 `IMA-ADPCM` (`64 kbps`), while **uplink** microphone capture (`Pico 2 W -> Phone/Pi 5` via `NET_MSG_QUERY_START` + `NET_MSG_AUDIO_CHUNK`) still transmits raw 16-bit PCM (`256 kbps`). Extend 4:1 `IMA-ADPCM` to uplink microphone packets so both directions run at `64 kbps` with self-healing 4-byte block headers. — ✅ Completed on `feat/sota-phase-next` (2026-11-22)
- **Target Files**:
  1. `firmware/src/net_client.c`
  2. `android/app/src/main/java/com/celestial/companion/domain/repository/StaffRepository.kt`
  3. `tools/brain_transport.py`
  4. `tests/test_protocol_v2_parity.py`
  5. `android/app/src/test/java/com/celestial/companion/audio/ImaAdpcmCodecTest.kt`
- **Step-by-Step Implementation Spec**:
  1. **Firmware Uplink Negotiation (`firmware/src/net_client.c`)**:
     - In `net_client_send_query_start(uint32_t sample_rate, uint8_t persona_id)`:
       - Expand the `NET_MSG_QUERY_START` (`0x10`) payload from 5 bytes (`[uint32 sample_rate][uint8 persona_id]`) to **6 bytes**: `[uint32 sample_rate][uint8 persona_id][uint8 codec_id]` where `codec_id = ADPCM_CODEC_ID_IMA_4BIT` (`1`).
       - Keep backward compatibility: receivers that check `payload_len >= 6` read `codec_id = payload[5]`; if `payload_len == 5`, they default to `codec_id = 0` (raw PCM16).
     - In `net_client_send_audio_chunk(const int16_t *pcm_samples, uint16_t sample_count)`:
       - Use a `static uint8_t uplink_adpcm_buf[NET_MAX_PAYLOAD_SIZE];` in BSS (never on the stack!).
       - Encode `pcm_samples` (`sample_count` samples, e.g., `480` or `512` samples) via `uint16_t adpcm_bytes = ima_adpcm_encode_block(pcm_samples, sample_count, uplink_adpcm_buf, sizeof(uplink_adpcm_buf));`.
       - Send `uplink_adpcm_buf` (`adpcm_bytes` length) as the `NET_MSG_AUDIO_CHUNK` (`0x11`) payload.
  2. **Android Companion Uplink Decoding (`StaffRepository.kt`)**:
     - Add `@Volatile private var uplinkAudioCodecId: Int = 0`.
     - When handling `UdpProtocol.MSG_QUERY_START` (`0x10`):
       - Read `uplinkAudioCodecId = if (packet.payload.size >= 6) (packet.payload[5].toInt() and 0xFF) else 0`.
     - When handling `UdpProtocol.MSG_AUDIO_CHUNK` (`0x11`):
       - If `uplinkAudioCodecId == ImaAdpcmCodec.CODEC_ID_IMA_ADPCM` (`1`) and `packet.payload.size >= 4`:
         - Compute `val sampleCount = (packet.payload.size - 4) * 2`
         - Decode via `val pcmBytes = ImaAdpcmCodec.decodeBlockToPcm16Le(packet.payload, sampleCount)` and append `pcmBytes` to `incomingMicBuffer`.
       - Else append raw `packet.payload` as before.
  3. **Raspberry Pi 5 Uplink Decoding (`tools/brain_transport.py`)**:
     - In `parse_query_start(payload: bytes)`: return `(sample_rate, persona_id, codec_id)` where `codec_id = payload[5] if len(payload) >= 6 else 0` (keep a helper or default so existing callers unpacking `(sample_rate, persona_id)` don't break, or store `self.uplink_codec_id = codec_id`).
     - In `NET_MSG_AUDIO_CHUNK` handling in `tools/ai_brain_server.py` / `tools/brain_transport.py`: if `uplink_codec_id == ADPCM_CODEC_ID_IMA_4BIT` (`1`), decode `payload` via `ima_adpcm_decode_block(payload)` before appending to `audio_buffer`.
  4. **Unit Tests (`tests/test_protocol_v2_parity.py` & `ImaAdpcmCodecTest.kt`)**:
     - Add `test_uplink_query_start_and_adpcm_audio_chunk_roundtrip` in `tests/test_protocol_v2_parity.py` asserting that a 6-byte `NET_MSG_QUERY_START` (`codec_id = 1`) followed by a `244-byte` ADPCM `NET_MSG_AUDIO_CHUNK` decodes to `960 bytes` (`480 samples`) of 16 kHz PCM with `SNR > 28 dB`.
- **Verification Command**:
  ```powershell
  cmake --build firmware/build --target celestial_firmware --parallel 4 ; python -m unittest discover -s tests -v
  ```

---

### Task 9.6: Cross-Language Bit-Exact C $\leftrightarrow$ Python $\leftrightarrow$ Kotlin IMA-ADPCM Test (`tests/firmware_harness.c` & `tests/test_regressions.py`)
- [x] **Objective**: Guarantee that the C (`firmware/src/adpcm_codec.c`), Python (`tools/adpcm_codec.py`), and Kotlin (`ImaAdpcmCodec.kt`) implementations of `ima_adpcm_encode_block` and `ima_adpcm_decode_block` are **100% bit-for-bit identical** (same initial `step_index`, same nibble packing order, and same predictor clamping). — ✅ Completed on `feat/sota-phase-next` (2026-11-22)
- **Target Files**:
  1. `tests/firmware_harness.c`
  2. `tests/test_regressions.py`
  3. `android/app/src/test/java/com/celestial/companion/audio/ImaAdpcmCodecTest.kt`
- **Step-by-Step Implementation Spec**:
  1. **Expose `adpcm_codec.c` in `tests/firmware_harness.c`**:
     - `#include "../firmware/src/adpcm_codec.c"` inside `tests/firmware_harness.c` (alongside the existing firmware modules compiled into the host shared library `firmware_harness.dll` / `.so`).
     - Export two C functions with `HARNESS_API`:
       ```c
       HARNESS_API uint16_t harness_adpcm_encode(const int16_t *pcm, uint16_t count, uint8_t *out_buf, uint16_t max_out) {
           return ima_adpcm_encode_block(pcm, count, out_buf, max_out);
       }
       HARNESS_API uint16_t harness_adpcm_decode(const uint8_t *in_buf, uint16_t in_len, int16_t *out_pcm, uint16_t max_samples) {
           return ima_adpcm_decode_block(in_buf, in_len, out_pcm, max_samples);
       }
       ```
  2. **Add C $\leftrightarrow$ Python Bit-Exact Assertion in `tests/test_regressions.py`**:
     - Generate a deterministic 480-sample test signal: `pcm[i] = int(18000 * math.sin(2 * math.pi * 500 * i / 16000) + 6000 * math.cos(2 * math.pi * 1200 * i / 16000))`.
     - Encode the signal in C via `harness_adpcm_encode` and in Python via `tools.adpcm_codec.ima_adpcm_encode_block`.
     - Assert `c_encoded_bytes == py_encoded_bytes` (**byte-for-byte equality across all 244 bytes!**).
     - Decode `py_encoded_bytes` in C via `harness_adpcm_decode` and in Python via `ima_adpcm_decode_block`, and assert `c_decoded_samples == py_decoded_samples` (**sample-for-sample equality across all 480 `int16` samples!**).
  3. **Add Fixed-Vector Bit-Exact Assertion in `ImaAdpcmCodecTest.kt`**:
     - Assert the exact 4-byte header (`predictor`, `step_index`, `0`) and first 8 encoded bytes on a known 16-sample input array `[0, 2500, 4800, 6500, 7200, 6500, 4800, 2500, 0, -2500, -4800, -6500, -7200, -6500, -4800, -2500]` so Kotlin is locked to the exact same bit vector as C and Python.
- **Verification Command**:
  ```powershell
  python -m unittest discover -s tests -v
  ```

---

### Task 9.7: Multi-Clause Prosody Chunker & Domain Lexicon for `KokoroOnnxSynthesizer.kt`
- [x] **Objective**: Improve `KokoroOnnxSynthesizer.kt` naturalness on Arcane Oracle prophecies by adding (a) a curated domain IPA lexicon (`celestial`, `omnissiah`, `void`, `arcane`, `aegis`, `supernova`, `chrono`, `plasma`, `rune`, `staff`, `warp`, `nebula`, `quantum`, `flux`, `grimoire`, `prophecy`) in `englishToIpaPhonemes(text)` and (b) multi-clause prosody chunking (`splitTextIntoKokoroClauses`) so multi-sentence prophecies pause naturally at sentence boundaries and never exceed the 510-token Kokoro context window. — ✅ Completed on `feat/sota-phase-next` (2026-11-22)
- **Target Files**:
  1. `android/app/src/main/java/com/celestial/companion/audio/KokoroOnnxSynthesizer.kt`
  2. `android/app/src/test/java/com/celestial/companion/ai/OnDeviceSotaAudioEngineTest.kt`
- **Step-by-Step Implementation Spec**:
  1. **Add `splitTextIntoKokoroClauses(text: String, maxCharsPerClause: Int = 180): List<String>` in `KokoroOnnxSynthesizer.Companion`**:
     - Strip `[SPELL: ...]` tags if present, normalize whitespace, and split at major sentence/clause delimiters (`(?<=[.!? status;:])\\s+`).
     - If any clause exceeds `maxCharsPerClause`, sub-split at commas (`,`) or word boundaries.
     - Filter out blank clauses.
  2. **Add `DOMAIN_IPA_LEXICON: Map<String, String>` in `KokoroOnnxSynthesizer.Companion`**:
     - Map at least 30 frequent Celestial Engine / Oracle words (`"celestial" to "səlˈɛstiəl"`, `"omnissiah" to "ˌɑːmnɪsˈaɪə"`, `"arcane" to "ɑːɹkˈeɪn"`, `"grimoire" to "ɡɹɪmwˈɑːɹ"`, `"prophecy" to "pɹˈɑːfəsi"`, `"supernova" to "sˌuːpɚnˈoʊvə"`, `"aegis" to "ˈiːdʒɪs"`, `"fireball" to "fˈaɪɚbˌɔːl"`, `"CHRONO" to "kɹˈoʊnoʊ"`, `"nebula" to "nˈɛbjələ"`, `"plasma" to "plˈæzmə"`, `"machine" to "məʃˈiːn"`, `"stars" to "stˈɑːɹz"`, `"void" to "vˈɔɪd"`, `"light" to "lˈaɪt"`, `"shield" to "ʃˈiːld"`, etc.) before falling back to the rule-based G2P converter in `englishToIpaPhonemes`.
  3. **Synthesize Clauses Sequentially with `100 ms` Inter-Clause Pause in `synthesizeOrNull`**:
     - Split `text` via `splitTextIntoKokoroClauses(text)`.
     - Synthesize each clause to `16 kHz` `ByteArray` (`16-bit PCM LE`) and insert `3200 bytes` (`1600 samples = 100 ms` of silence `0x00`) between clauses.
  4. **Unit Tests in `OnDeviceSotaAudioEngineTest.kt`**:
     - Test `splitTextIntoKokoroClauses` on a 3-sentence prophecy and test `englishToKokoroTokens` confirming domain words like `"Omnissiah"` and `"supernova"` produce valid non-empty IPA token arrays where every ID is in `1..177`.
- **Verification Command**:
  ```powershell
  git push origin feat/sota-phase-next
  ```

---

### Task 9.8: Interactive On-Device SOTA Benchmark Card in `OracleTerminalScreen.kt`
- [x] **Objective**: Add a one-tap **`[⚡ Run On-Device SOTA Benchmark]`** button inside the Android Companion's **Oracle Terminal (`OracleTerminalScreen.kt`)** that runs a live self-test on the physical smartphone (`Moonshine/SenseVoice STT`, `SmolLM2-135M LLM`, `Kokoro-82M TTS`, `4:1 IMA-ADPCM`, and `OmniAudioSession` latency) and prints a structured telemetry report into the terminal log. — ✅ Completed on `feat/sota-phase-next` (2026-11-22)
- **Target Files**:
  1. `android/app/src/main/java/com/celestial/companion/ui/DashboardViewModel.kt`
  2. `android/app/src/main/java/com/celestial/companion/ui/screens/OracleTerminalScreen.kt`
- **Step-by-Step Implementation Spec**:
  1. **Add `fun runOnDeviceSotaBenchmark()` in `DashboardViewModel.kt`**:
     - Launch on `viewModelScope.launch(Dispatchers.Default)`:
       1. Measure **4:1 IMA-ADPCM Codec Speed & SNR**: Encode & decode a 1-second (`16,000 samples = 32,000 bytes`) 500 Hz test tone via `ImaAdpcmCodec`, recording encode/decode microseconds and compressed byte size (`8,004 bytes`).
       2. Measure **On-Device SOTA Audio (`OnDeviceSotaAudioEngine`)**: If ready, run a 1-second synthetic speech/laugh waveform through `transcribePcm16kMono` and record latency in `ms` (if not downloaded yet, report `"Model not downloaded yet — tap Download on Dashboard"`).
       3. Measure **On-Device Generative LLM (`OnDeviceLlmEngine`)**: If ready, run a short 12-token prompt and record `tok/s` and `TTFT ms`.
       4. Measure **On-Device Kokoro-82M TTS (`KokoroOnnxSynthesizer`)**: If ready, synthesize `"The Omnissiah blesses this circuit."` and record synthesis time (`ms`) and Real-Time Factor (`RTF = synthMs / audioDurationMs`).
       5. Post the formatted summary into the Oracle Terminal chat/log state (`appendTerminalLog(...)`).
  2. **Add `[⚡ Benchmark SOTA]` Chip/Button in `OracleTerminalScreen.kt`**:
     - Place a clean Cyber-Arcane action button next to the engine mode chips in `OracleTerminalScreen.kt` that triggers `viewModel.runOnDeviceSotaBenchmark()`.
- **Verification Command**:
  ```powershell
  git push origin feat/sota-phase-next
  Start-Sleep -Seconds 4
  $runId = (gh run list --workflow=android-build.yml --branch=feat/sota-phase-next --limit 1 --json databaseId -q '.[0].databaseId')
  gh run watch $runId --exit-status
  ```

---

## 11. Waveshare ESP32-S3-Touch-AMOLED-1.8 — Raspberry Pi 5 Satellite Screen, Speaker & Microphone (Non-Destructive Dual-Mode Firmware)

> **Purpose:** Turn the connected **Waveshare ESP32-S3-Touch-AMOLED-1.8** (`COM11`, Wi-Fi IP `192.168.178.121`, `368×448` CO5300 QSPI AMOLED, `FT3168`/`CST816S` capacitive touch, `ES8311` I2C `0x18` audio codec + speaker PA + onboard microphone, `AXP2101` PMU, `16 MB` Flash, `8 MB` PSRAM) into a wearable/desk **Touch Screen + Speaker + Push-to-Talk Microphone terminal for the Raspberry Pi 5 AI Brain Server** — **WITHOUT deleting or losing any existing firmware** (`ESP-Brookesia Phone OS` in `factory` at `0x110000` and the `12-Page Art Deco PC Monitor / Codex Pet / Vintage Radio` in `ota_0` at `0x690000`).

### 11.0. Hardware Pinout & Non-Destructive Partition Map (Verified on `COM11` / `192.168.178.121`)

| Subsystem | Chip / Controller | ESP32-S3 Pins / Address | Verified Status |
| :--- | :--- | :--- | :--- |
| **QSPI AMOLED Display (`368×448`)** | `CO5300` / `SH8601` | `SCLK=11, CS=12, D0=4, D1=5, D2=6, D3=7` (`x_offset=28`) | ✅ Working (`16-line` internal DMA SRAM flush) |
| **Capacitive Touch** | `CST816S` (`0x15`) / `FT3168` (`0x38`) | `I2C SDA=15, SCL=14, TP_INT=21` | ✅ Working (`0x15` detected on I2C scan) |
| **Audio Codec (Speaker DAC + Mic ADC)** | **`ES8311`** (`I2C 0x18`, ID `0x8311`) | `MCLK=16, BCLK=9, WS=45, DOUT=8 (Spk), DIN=10 (Mic), PA=46` | ✅ DAC working; ADC Mic on `GPIO 10` (`MIC1P/1N`) |
| **Power & GPIO Expander** | `AXP2101` (`0x34`) + `TCA9554` (`0x20`) | `I2C SDA=15, SCL=14`, `BOOT_BTN=0` | ✅ Working |
| **Partition `factory` (`0x110000`, `5.5 MB`)** | Original `ESP-Brookesia / XiaoZhi AI` | Flash offset `0x110000 .. 0x690000` | 🔒 **NEVER TOUCH / NEVER ERASE** |
| **Partition `ota_0` (`0x690000`, `3.0 MB`)** | Dual-Persona `Cricket 12-Page PC Monitor` + `Celestial Pi 5 Terminal` | Flash offset `0x690000 .. 0x990000` (`C:\projects\personal\cricket\firmware`) | 🔄 Backed up + auto-switches to Pi 5 Mode when Pi 5 starts |

---

### Task 11.1: Non-Destructive `ota_0` Backup & Dual-Persona Auto-Switch State Machine
- [x] **Objective**: Preserve a bit-exact backup of the current `ota_0` firmware before any modifications, and add a non-destructive runtime mode state machine (`AppPersonaMode`: `MODE_PC_MONITOR_12PAGE = 0` vs `MODE_CELESTIAL_PI5_BRAIN = 1`) that **boots into the normal 12-page PC Monitor by default** and **automatically switches to `MODE_CELESTIAL_PI5_BRAIN` whenever the Raspberry Pi 5 starts up** (via USB Serial `/dev/ttyACM0` or Wi-Fi UDP `:4210`). — ✅ Completed on `feat/sota-phase-next` (2026-11-22)
- **Target Files**:
  1. `C:\projects\personal\cricket\backup\backup_current_ota0.ps1` *(new script)*
  2. `C:\projects\personal\cricket\firmware\src\main.cpp`
- **Step-by-Step Implementation Spec**:
  1. **Save Current `ota_0` Binary Before Any Edit**:
     - Copy `C:\projects\personal\cricket\firmware\.pio\build\esp32-s3-touch-amoled-1-8\firmware.bin` to `C:\projects\personal\cricket\backup\cricket_pc_monitor_12page_ota0.bin` so the exact current build can be flashed back to `0x690000` in 3 seconds at any time.
  2. **Add `AppPersonaMode` & Auto-Attach Detection in `main.cpp`**:
     - Define:
       ```cpp
       enum AppPersonaMode {
           MODE_PC_MONITOR_12PAGE = 0,
           MODE_CELESTIAL_PI5_BRAIN = 1
       };
       static AppPersonaMode g_persona_mode = MODE_PC_MONITOR_12PAGE;
       static uint32_t g_pi5_last_heartbeat_ms = 0;
       static IPAddress g_pi5_ip(0, 0, 0, 0);
       static uint16_t g_pi5_udp_port = 5005;
       static bool g_pi5_via_usb_serial = false;
       ```
     - **Auto-Switch Trigger Rules**:
       1. **Wi-Fi UDP (`port 4210`)**: When a UDP JSON packet arrives containing `"cmd":"celestial_pi5_attach"` or `"host":"PI5-BRAIN"`, set `g_persona_mode = MODE_CELESTIAL_PI5_BRAIN`, store `g_pi5_ip = g_udp.remoteIP()`, `g_pi5_last_heartbeat_ms = millis()`, and update the screen immediately.
       2. **USB CDC Serial (`Serial` at `115200` / `921600`)**: When plugged into a Raspberry Pi 5 USB port, the Pi 5 daemon sends a line starting with `CELESTIAL_PI5_JSON:{...}\n`. When `Serial.available()` reads a line starting with `CELESTIAL_PI5_JSON:`, set `g_persona_mode = MODE_CELESTIAL_PI5_BRAIN`, `g_pi5_via_usb_serial = true`, parse the JSON state, and reply `CELESTIAL_ESP32_ACK:{"device":"ESP32-S3-AMOLED","mode":"PI5_BRAIN"}\n`.
       3. **Auto-Return / Manual Toggle**:
          - If `"cmd":"celestial_pi5_detach"` arrives (or the user taps **`[🖥️ PC MODE]`** on the top bar / HTTP `POST /api/mode?target=pc`), set `g_persona_mode = MODE_PC_MONITOR_12PAGE` and return immediately to the 12-page Cricket PC Monitor (`OVERVIEW`, `CPU`, `GPU`, `CODEX PET`, `ATELIER`, `RADIO`, `SLOP`, `SYSTEM`).
          - On Page 12 (`SYSTEM`) of the PC Monitor, add a quick touch button **`[🔮 PI 5 BRAIN MODE]`** so the user can also enter Pi 5 Mode manually at any time, while keeping **`[SWITCH TO PHONE OS]`** (`switchToFactoryPartition()`) intact.
- **Verification Command**:
  ```powershell
  Test-Path C:\projects\personal\cricket\backup\cricket_pc_monitor_12page_ota0.bin
  ```

---

### Task 11.2: Full-Duplex ES8311 Audio Driver (16 kHz I2S Speaker Out `GPIO 8` + 16 kHz I2S Microphone In `GPIO 10`)
- [x] **Objective**: Upgrade the ESP32-S3 audio subsystem from TX-only (`DOUT = GPIO 8`) to **Full-Duplex 16 kHz 16-bit I2S (`DOUT = GPIO 8`, `DIN = GPIO 10`)** and configure the **ES8311 ADC + Microphone PGA registers (`0x0E`, `0x14`, `0x15`, `0x17`, `0x1B`, `0x1C`)** so the ESP32-S3 works simultaneously as a **16 kHz Speaker** and a **16 kHz Push-to-Talk Microphone** for the Raspberry Pi 5. — ✅ Completed on `feat/sota-phase-next` (2026-11-22)
- **Target Files**:
  1. `C:\projects\personal\cricket\firmware\src\main.cpp`
- **Step-by-Step Implementation Spec**:
  1. **Define Microphone Pin & RX Channel Handle**:
     ```cpp
     #define I2S_DIN_PIN       10
     static i2s_chan_handle_t g_i2s_rx = nullptr;
     static volatile bool g_mic_recording = false;
     static volatile int16_t g_mic_peak = 0;
     static volatile uint32_t g_mic_tx_bytes = 0;
     ```
  2. **Enable ES8311 ADC & Microphone Input in `initEs8311()`**:
     - After the existing DAC register writes in `initEs8311()`, add the verified ES8311 ADC microphone registers:
       ```cpp
       i2cWriteReg(ES8311_ADDR, 0x0E, 0x02); // Enable analog PGA & ADC modulator
       i2cWriteReg(ES8311_ADDR, 0x14, 0x1A); // Select MIC1P/MIC1N + Analog PGA gain (+24dB)
       i2cWriteReg(ES8311_ADDR, 0x15, 0x40); // ADC ramp rate
       i2cWriteReg(ES8311_ADDR, 0x16, 0x24); // ADC mic gain scale
       i2cWriteReg(ES8311_ADDR, 0x17, 0xBF); // ADC volume (0dB, unmuted)
       i2cWriteReg(ES8311_ADDR, 0x1B, 0x0A); // ADC HPF / EQ config
       i2cWriteReg(ES8311_ADDR, 0x1C, 0x6A); // ADC DC block filter enabled
       ```
  3. **Initialize Full-Duplex I2S (`&g_i2s_tx, &g_i2s_rx`) in `initI2sAudio()`**:
     - Pass both `&g_i2s_tx` and `&g_i2s_rx` to `i2s_new_channel(&chan_cfg, &g_i2s_tx, &g_i2s_rx)`.
     - Set `.din = (gpio_num_t)I2S_DIN_PIN` (`GPIO 10`) in `std_cfg.gpio_cfg`.
     - Call `i2s_channel_init_std_mode(g_i2s_rx, &std_cfg)` and `i2s_channel_enable(g_i2s_rx)`.
  4. **Stream Captured 16 kHz Mono PCM / 4:1 IMA-ADPCM to Raspberry Pi 5**:
     - When `g_mic_recording == true` (while user holds the **`[🎙️ HOLD TO SPEAK]`** touch button on the AMOLED screen or toggles `[🎙️ TALK]`):
       - Read stereo 16-bit frames from `g_i2s_rx` (`i2s_channel_read`), extract the active microphone channel (`left` or max-energy channel) into a `240-sample` (`480-byte`, `15 ms`) mono `int16_t` frame, compute `g_mic_peak`, and stream it to the Raspberry Pi 5 over UDP (`g_pi5_ip`, port `4212` / `5005` with header `"CMIC"` + `uint16_t seq` + `480 bytes PCM16LE`).
     - Also allow `audioRxTask` (`TCP port 4211` and UDP audio packets `"CSPK"`) to play incoming `16 kHz 16-bit mono PCM` from the Raspberry Pi 5 (`Kokoro-82M` / `Piper TTS`) whenever `g_persona_mode == MODE_CELESTIAL_PI5_BRAIN` (even when `g_radio_on == false`).
  5. **Add `/api/mic_test` Diagnostic Endpoint**:
     - Record `200 ms` from `g_i2s_rx`, compute `mic_peak` and `rms`, and return `{"mic_ok":true,"mic_peak":...,"mic_rms":...}` so automated tests can verify live microphone capture over HTTP.
- **Verification Command**:
  ```powershell
  curl.exe -s http://192.168.178.121/api/mic_test
  ```

---

### Task 11.3: Cyber-Arcane `368×448` AMOLED Touch UI for Raspberry Pi 5 Brain (`renderCelestialPi5Screen()`)
- [x] **Objective**: Build a dedicated, high-contrast Cyber-Arcane touch interface (`renderCelestialPi5Screen()` and `handleCelestialPi5Touch()`) for `MODE_CELESTIAL_PI5_BRAIN` on the `368×448` AMOLED panel. — ✅ Completed on `feat/sota-phase-next` (2026-11-22)
- **Target Files**:
  1. `C:\projects\personal\cricket\firmware\src\main.cpp`
  2. `C:\projects\personal\cricket\emulator.py` *(keep 1:1 parity in the local Python preview emulator)*
- **Step-by-Step Implementation Spec**:
  1. **Define `Pi5BrainState` Struct in `main.cpp`**:
     ```cpp
     struct Pi5BrainState {
         char status[28] = "PI 5 BRAIN: STANDBY";
         char persona_name[24] = "MACHINE GOD";
         int persona_id = 0; // 0..5
         char stt_engine[20] = "MOONSHINE INT8";
         char tts_engine[20] = "KOKORO-82M";
         char last_heard[96] = "Tap & hold MIC below to speak to the Oracle...";
         char prophecy[220] = "The Omnissiah awaits your voice command.";
         char active_spell[24] = "NONE";
         uint8_t spell_r = 36, spell_g = 204, spell_b = 182;
         float pi5_temp_c = 48.0f;
         float pi5_ram_pct = 24.0f;
         int latency_ms = 0;
     };
     static Pi5BrainState g_pi5;
     ```
  2. **Layout of `renderCelestialPi5Screen()` (`368×448` RGB565)**:
     - **Header Bar (`y=12..50`)**:
       - Left: Live status diamond (Green = Connected to Pi 5, Cyan = Speaking, Red = Recording Mic) + `"PI 5 ORACLE"`.
       - Right: **`[🖥️ PC]`** button (`x=276..348, y=14..48`) to switch back to the 12-page PC Monitor with one tap.
     - **Card 1 — Persona & Pi 5 Telemetry Strip (`y=56..118`)**:
       - Left/Center: Active Persona badge (`< MACHINE GOD >` / `ARCHMAGE` / `MARVIN` / `CYBERPUNK` / `PUMPKIN LORD` / `CUSTOM`) — tap `<` or `>` to cycle persona on the Pi 5!
       - Sub-row: `STT: MOONSHINE | TTS: KOKORO | PI5: 49C`.
     - **Card 2 — Live Voice Transcript, Active `[SPELL]` & Oracle Prophecy (`y=124..288`)**:
       - Top line (`COL_CYAN`): `"YOU: "` + wrapped `g_pi5.last_heard` (`2 lines`).
       - Spell badge (drawn with `rgb565(g_pi5.spell_r, g_pi5.spell_g, g_pi5.spell_b)` border): `"SPELL: "` + `g_pi5.active_spell`.
       - Prophecy box (`COL_GOLD` / `COL_TEXT`): 4 wrapped lines (`26 chars` each) of `g_pi5.prophecy` + live 12-bar audio waveform visualizer (`g_audio_peak` / `g_mic_peak`).
     - **Card 3 — Giant Push-to-Talk / Toggle Microphone Button (`y=296..368`)**:
       - When `g_mic_recording == false`: Emerald/Gold bordered button **`[🎙️ TAP TO SPEAK TO PI 5]`** with live mic VU indicator.
       - When `g_mic_recording == true`: Glowing Crimson button **`[🔴 LISTENING... TAP TO SEND]`** streaming 16 kHz mic audio to the Pi 5.
     - **Bottom Control Bar (`y=376..434`)**:
       - **`[⚡ CAST]`** (`x=14..120`): Triggers a test spell / voice prophecy on Pi 5.
       - **`[V-]` / `75%` / `[V+]`** (`x=128..264`): Adjusts onboard ES8311 speaker volume (`0..100%`).
       - **`[🔊 BEEP]`** (`x=272..354`): Plays quick 440 Hz chime / repeats last TTS audio.

---

### Task 11.4: Raspberry Pi 5 Auto-Attach Bridge & Audio/HUD Streamer (`tools/pi5_esp32_satellite.py` & `tools/ai_brain_server.py`)
- [x] **Objective**: Create a clean Python bridge module [`tools/pi5_esp32_satellite.py`](../tools/pi5_esp32_satellite.py) (and hook it into [`tools/ai_brain_server.py`](../tools/ai_brain_server.py)) so that whenever the Raspberry Pi 5 brain server starts, it **automatically finds the ESP32-S3** (via USB `/dev/ttyACM*` / `COM*` or Wi-Fi UDP `:4210`), switches its screen into **Celestial Pi 5 Brain Mode**, receives 16 kHz microphone audio from the ESP32-S3, runs `Moonshine/SenseVoice STT -> Ollama/Offline LLM -> Kokoro-82M TTS`, updates the ESP32-S3 AMOLED display, and plays the synthesized voice out of the ESP32-S3's speaker! — ✅ Completed on `feat/sota-phase-next` (2026-11-22)
- **Target Files**:
  1. `tools/pi5_esp32_satellite.py` *(new file)*
  2. `tools/ai_brain_server.py`
  3. `tests/test_regressions.py`
- **Step-by-Step Implementation Spec**:
  1. **Create `Esp32SatelliteBridge` in `tools/pi5_esp32_satellite.py`**:
     - **Auto-Discovery (`discover_esp32()`)**:
       - Broadcasts `CRICKET_DISCOVER` to UDP port `4210` (and checks default IP `192.168.178.121` or `--esp-ip`), plus checks serial ports matching `VID_303A` (`PID_1001`).
     - **Auto-Attach on Startup (`attach()`)**:
       - Sends UDP JSON packet to `(esp_ip, 4210)`:
         ```json
         {
           "cmd": "celestial_pi5_attach",
           "host": "PI5-BRAIN",
           "persona": "MACHINE GOD",
           "stt_engine": "MOONSHINE INT8",
           "tts_engine": "KOKORO-82M",
           "pi5_status": "ONLINE :5005",
           "pi5_heard": "Ready for voice command",
           "pi5_prophecy": "The Omnissiah blesses this satellite node.",
           "pi5_spell": "NONE"
         }
         ```
       - Starts a background heartbeat thread sending live Pi 5 CPU temp, RAM %, active persona, last STT transcript, last prophecy, and active `[SPELL]` every `1.5s`.
     - **Receive 16 kHz Microphone Stream from ESP32-S3 (`UDP port 4212`)**:
       - Listens for `"CMIC"` packets (`4-byte magic "CMIC" + uint16 seq + 480 bytes 16 kHz PCM16LE`), accumulates them while the user is speaking on the ESP32-S3, and when `"CMIC_END"` arrives, passes the complete `16 kHz PCM` buffer directly into `LocalAiEngine.transcribe_audio_bytes()` -> `generate_prophecy()` -> `synthesize_speech_pcm()`.
     - **Stream Synthesized `Kokoro-82M` / `Piper` 16 kHz PCM to ESP32-S3 Speaker (`TCP port 4211`)**:
       - Connects to `(esp_ip, 4211)` and streams the `16 kHz 16-bit mono PCM` waveform in `512-byte` chunks so the ESP32-S3's `ES8311` speaker speaks the Oracle prophecy out loud while the RP2350 staff animates the spell!
     - **Auto-Detach on Shutdown (`detach()`)**:
       - Sends `{"cmd":"celestial_pi5_detach"}` on `SIGINT`/`SIGTERM` so the ESP32-S3 automatically returns to its 12-page Cricket PC Monitor mode when the Pi 5 server stops.
  2. **Unit Tests in `tests/test_regressions.py`**:
     - Add `test_esp32_satellite_attach_detach_and_cmic_framing` verifying JSON attach/detach packet generation, `"CMIC"` packet assembly into 16 kHz PCM, and full compatibility with `LocalAiEngine`.
- **Verification Command**:
  ```powershell
  python -m unittest discover -s tests -v
  ```

---

### Task 11.5: End-to-End Hardware Verification on Connected ESP32-S3 (`COM11` / `192.168.178.121`)
- [x] **Objective**: Compile and flash the updated dual-mode `ota_0` firmware to the connected **Waveshare ESP32-S3-Touch-AMOLED-1.8 (`COM11`)**, verify that both existing modes (`12-Page Cricket PC Monitor` and `Factory ESP-Brookesia Phone OS`) remain 100% intact, and test live **Pi 5 Satellite Screen + ES8311 Speaker + ES8311 Microphone** operation. — ✅ Completed on `feat/sota-phase-next` (2026-11-22)
- **Step-by-Step Verification Commands**:
  1. **Compile & Flash `ota_0` via PlatformIO (`COM11`)**:
     ```powershell
     cd C:\projects\personal\cricket\firmware
     pio run -t upload --upload-port COM11
     ```
  2. **Verify I2C, ES8311 Speaker & ES8311 Microphone via HTTP Diagnostics**:
     ```powershell
     curl.exe -s http://192.168.178.121/api/diag
     curl.exe -s http://192.168.178.121/api/mic_test
     ```
  3. **Verify Automatic Switch to Celestial Pi 5 Brain Mode (Screen + Speaker + Mic) & Automatic Return to 12-Page PC Monitor**:
     ```powershell
     cd c:\projects\personal\hardware-01\celestial_engine_project
     python tools/pi5_esp32_satellite.py --esp-ip 192.168.178.121 --demo
     ```
     - Confirm the ESP32-S3 AMOLED screen automatically switches to **`PI 5 ORACLE`** HUD, plays the synthesized Oracle voice through its onboard speaker, captures live microphone input from `ES8311 ADC (GPIO 10)`, and switches back to the **12-Page Art Deco PC Monitor** when `--demo` exits.
