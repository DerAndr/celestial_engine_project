# Tasks — Final Setup: Phone + Pico 2 W Core, Hotspot First, & AI Spells

Updated 2026-10-23. The project baseline locks the primary operating topology to **Pico 2 W (Staff) + Android Companion Phone (Mobile Brain)**, with an optional connection profile to **Raspberry Pi 5** when needed.

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
- [ ] **Android Background Service**:
  - Run foreground service `StaffBrainService` to prevent OS sleep/throttling when phone screen is locked.
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
- [ ] **2D Polar Kinematics Engine in `leds.c` (43 LEDs: 7→12→24)**:
  - `SPELL_ANIM_SPIRAL_SNAKE`: Multi-ring Ouroboros serpent coiling through 24 -> 12 -> 7 with core flare.
  - `SPELL_ANIM_RADIAL_WAVE`: Concentric ripples & shockwaves propagating outward (or imploding inward).
  - `SPELL_ANIM_COLLIDING_WAVES`: Counter-rotating wave crests colliding at 0°/180° with white constructive flash.
  - `SPELL_ANIM_ARCHIMEDES_SPIRAL`: Hypnotic rotating spoke with radial phase lag across concentric rings.
  - `SPELL_ANIM_SACRED_POLYGON`: Stroboscopic geometric runes (Triquetra, Hexagram / Merkaba) rotating at harmonic ratios.
  - Phonetic VU Coupling: Real-time amplitude modulation from speaker DMA (words glow, plosives shockwave).

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

### Phase 1: Workspace Sanitation & Housekeeping (Immediate)
- [ ] **Purge Root Clutter**:
  - Remove stale build directories: `temp_apk*`, `build_apk`, `android_artifact`, `android_bin`.
  - Archive/move temporary test screenshots (`screen_*.png`, `app_screen_*.png`) to `docs/assets/archive/` or delete obsolete captures.
  - Clean up root `.gitignore` to prevent generated artifacts and test images from dirtying the working tree.
- [ ] **Configuration & Script Hygiene**:
  - Standardize all script paths (`tools/`, `scripts/`) to avoid hardcoded absolute paths or obsolete IP addresses.

### Phase 2: Android Companion App Architecture Decoupling
*Target: Break down monolithic files (`StaffRepository.kt` ~600+ lines, `GemmaModelEngine.kt` ~680 lines, `DashboardViewModel.kt` ~800+ lines) into clean, decoupled layers.*

- [ ] **Binary Protocol Codec Layer (`network/protocol/`)**:
  - Extract packet serialization and deserialization out of `StaffRepository.kt` into dedicated, pure Kotlin codecs:
    - `StaffPacketCodec.kt` (Encode/Decode `APP_TELEMETRY`, `AUDIO_STREAM`, `RESP_START`, `RESP_TEXT`, `RESP_AUDIO_CHUNK`, `RESP_END`, `CONTROL_MSG`).
    - Add comprehensive unit tests (`StaffPacketCodecTest.kt`) to guarantee byte-level parity with Pico C structs (`net_protocol.h`).
- [ ] **Network Transport Layer (`network/transport/`)**:
  - Isolate raw Android 15 socket binding, hotspot network discovery (`ap_br_wlan2`), and coroutine packet loops into `StaffUdpTransport.kt`.
  - Remove all business logic, audio pacing, and state mutation from the transport layer.
- [ ] **AI & Speech Subsystem Modularization (`ai/`)**:
  - Split `GemmaModelEngine.kt` into focused services:
    - `AcousticTranscriptionService.kt`: Handles fast audio-to-text transcription (~250ms helper).
    - `GemmaModelService.kt`: Handles LLM prompt construction, model endpoint streaming (OpenRouter / Edge), and SSE token reading.
    - `GemmaVariantRegistry.kt`: Centralized enum and configurations for `gemma-4-E2B`, `gemma-4-31b`, etc.
    - `CelestialProphecyParser.kt`: (Already extracted) dedicated unit testing for tag stripping and spell extraction.
- [ ] **Domain UseCases & State Reducers (`domain/usecase/`)**:
  - Extract business flows out of `DashboardViewModel.kt` and `StaffRepository.kt`:
    - `ProcessStaffVoiceQueryUseCase`: Coordinates Mic Audio -> Transcription -> LLM Stream -> Spell Dispatch -> TTS Synthesis -> Audio Stream back to staff.
    - `SyncStaffTelemetryUseCase`: Manages heartbeat ticks, digital twin parity, and signal quality.
    - `EmergencyAbortUseCase`: Clean instant cancellation across AI coroutines, TTS synthesis, and Pico audio playback.

### Phase 3: Pico 2 W Firmware Modularization & Cleanup (`firmware/src/`)
*Target: Streamline `net_client.c` (~800 lines) and eliminate accumulated patches and legacy workarounds.*

- [ ] **Modular Packet Dispatcher (`net_dispatcher.c`)**:
  - Replace sprawling `switch(msg_type)` and nested `if` statements with a clean, lookup-table-based message dispatch table.
  - Enforce strict message boundary checks and clean error reporting.
- [ ] **Network State Machine Decoupling (`net_client.c`)**:
  - Separate connection lifecycle (Wi-Fi join, DHCP, IP resolution, Heartbeat) from query streaming and audio reception.
  - Remove deprecated port overwriting, temporary debug logs, and hardcoded legacy fallback branches.
- [ ] **Audio DMA & Buffer Separation (`audio_subsystem.c`)**:
  - Decouple MAX98357A I2S DMA ring buffer management from network chunk reception.
  - Implement clean FIFO buffer abstraction with explicit underflow/overflow recovery hooks.
- [ ] **Strict Compiler Cleanliness**:
  - Enable `-Wall -Wextra` in CMake and resolve all compiler warnings.

### Phase 4: Integration Test Suite & Protocol Regression Prevention
- [ ] **Automated Protocol Packet Round-Trip Tests**:
  - Python test runner (`tests/test_protocol_v2_parity.py`) verifying binary layout parity across C (`net_protocol.h`), Kotlin (`StaffPacketCodec.kt`), and Python (`tools/brain_transport.py`).
- [ ] **Mock Voice Loop End-to-End Test**:
  - CLI test tool simulating a complete staff query without physical hardware to catch regressions in CI.

