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

### 🎯 Active Priority: Raspberry Pi 5 Wearable Node Modernization & Full Parity (Актуализация Raspberry Pi 5)
> **Goal:** Bring the Raspberry Pi 5 Wearable Node (`tools/ai_brain_server.py` & `tools/local_ai_engine.py`) to 100% parity with the companion app and canonical v1.1.0 firmware, enabling seamless standalone offline AI with personas, spells, and concise speech.

- [ ] **1. AI Persona Presets & System Prompt Modernization (`tools/ai_brain_server.py`, `tools/local_ai_engine.py`)**:
  - Port the 6 interactive AI Personas (*Machine God, Wise Archmage, Marvin Android, Cyberpunk Netrunner, Jack Pumpkin King, Custom Prompt*) into the Python server.
  - Enforce concise, punchy speech: 1–2 sentences, maximum 15–25 words (audio playback < 3s).
  - Strict guardrails: no markdown, asterisks, bullet points, or emojis (for clean Piper TTS speech).
  - Default language: English (`[EN]`), with native EN/DE/RU system prompt templates.
- [ ] **2. Arcane Spells & Incantations Hardware Dispatch (`[SPELL: ...]`)**:
  - Add prompt injection and regex token interceptor in `ai_brain_server.py` for:
    `[SPELL: Name | ANIM: Animation | SOUNDS: Sfx1, Sfx2, ...] <Incantation text>`
  - Dispatch binary `NET_MSG_APP_COMMAND` (0x20) packets over UDP to staff to trigger corresponding 21 LED rune animations and 24 soundboard tracks.
- [ ] **3. Dynamic Model Catalog & Protocol Alignment**:
  - Update `NET_MSG_CATALOG_RESP` on Pi 5 with current models: `gemma4:e2b`, `qwen2.5:0.5b`, `llama3.2:1b`, `deepseek-r1:1.5b`.
  - Align with canonical v1.1.0 45s audio streaming response gate and sequence numbering.
- [ ] **4. Pi 5 Deployment & Service Verification**:
  - Power on Raspberry Pi 5, pull latest `master` commits (`git pull`) at `/home/andr/celestial_engine_project`.
  - Update virtual environment packages in `.venv` (`tools/requirements-brain.txt`).
  - Verify and restart systemd service `celestial-brain.service`.
- [ ] **5. Wearable Network Profile (Offline Field Mobility)**:
  - Configure Pi 5 client auto-connect to phone hotspot (`Pixel_9_Pro_XL`), OR configure autonomous Pi 5 AP/Hotspot (`Celestial-Brain-AP`) so the staff connects directly without home Wi-Fi.
- [ ] **6. End-to-End Voice Cycle Verification (Staff ↔ Pi 5)**:
  - Switch staff to `[HOST] -> PI 5 BRAIN`.
  - Verify voice query via INMP441, offline transcription via Whisper.cpp, Ollama local inference, Piper TTS, and clean playback via MAX98357A speaker.


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

---

## 5. Backlog & Future Themed Packs (Planned / Not for Current Session)

> **User Requests (2026-10-26):**
> 1. *"Таска на будущее - не делать сейчас - сделай как одну из визуализаций на посохе - в духе eBay для светодиодов и смешных звуков. Так же сделай темы в духе гарри поттера и pokenom И сейлор мун!"*
> 2. *"про звуки - добавь 8-bit мелодии к тем темам мв задачи"*

- [ ] **1. eBay Chaos Auction Mode (Visuals, 8-bit Bidding Chiptune & Hilarious SFX)**:
  - **LED Animation**: 4-color bouncy eBay strobe palette (Red, Blue, Yellow, Green) racing around the 3 concentric rings in accelerating bid steps, culminating in an energetic rainbow auction flash.
  - **8-Bit Chiptune Track**: Retro arcade "Supermarket Sweep / Crazy Auction" fast-paced 8-bit square wave melody with escalating BPM as the bid counter ticks down to zero.
  - **Sound Effects**: Cash register "cha-ching!", rapid-fire auctioneer gavel strikes, outbid buzzer, comical cartoon "SOLD!" fanfare.
  - **OLED Text**: Retro 80s marketplace marquee: `$$$ WINNING BID! $$$`, `BID ACCEPTED`, `OUTBID!`.
  
- [ ] **2. Harry Potter Arcane Pack (House Runes & 8-bit Hedwig's Theme)**:
  - **Hogwarts House Runes**: Preset 43-LED color themes for Gryffindor (scarlet & gold), Slytherin (emerald & silver), Ravenclaw (sapphire & bronze), Hufflepuff (amber & obsidian).
  - **8-Bit Chiptune Track**: Iconic **8-bit "Hedwig's Theme"** (`chime_note_t` frequency score in RP2350 audio subsystem: E4, G4, F#4, E4, B4, A4, F#4...) played on retro square wave synth with synchronized mystical LED ripples.
  - **Signature Spells**:
    - *Lumos Maxima*: Concentric core flare from center LED outwards to a steady radiant warm white lantern.
    - *Expecto Patronum*: Silvery-white shimmering ethereal vortex with high-pitch chime glissando.
    - *Avada Kedavra*: Intense emerald lightning flash across outer ring with ominous deep drone.
    - *Expelliarmus*: Bright crimson shockwave ripple imploding inward.
  - **Incantations & Soundscape**: Wand swooshes, mystical choral harmonics, Latin incantations spoken with dramatic cadence.

- [ ] **3. Pokémon Arcane Pack (Pokéball Ring & 8-bit Battle / Center Chiptunes)**:
  - **Pokéball Concentric Topology**: Top half of 24-LED outer ring shines crimson red, bottom half shines pure white, middle ring dark separator, center LED pulses glowing white like the capture button.
  - **8-Bit Chiptune Tracks**:
    - *Track A: 8-bit Wild Pokémon Battle Theme* (energetic Game Boy chiptune arpeggios synchronized with rapid red/white LED flashes).
    - *Track B: 8-bit Pokémon Center Healing Jingle* (sweet 6-note restorative melody with warm green healing ripple).
  - **Capture Sequence Animation**: Center LED pulses 3 times (`click... click... click...`) then the entire ring blooms with yellow stars and capture fanfare.
  - **Sound Effects**: "Pikachu!" acoustic sample, Pokéball capture click/ding, Victory fanfare, Level-up chime.
  - **OLED Text**: Retro GameBoy 8-bit pixel font: `A wild AI appeared!`, `Pikachu used Thunderbolt!`, `Gotcha!`.

- [ ] **4. Sailor Moon / Magical Girl Arcane Pack (Prism Power & 8-bit Moonlight Densetsu)**:
  - **Moon Prism Power Transformation Sequence**: Concentric sparkling waves of pastel pink, magenta, gold, and iridescent cyan swirling outward.
  - **8-Bit Chiptune Track**: Iconic **8-bit "Moonlight Densetsu" (ムーンライト伝説)** opening melody transcribed for RP2350 audio engine, with starry stroboscopic pink/gold pulse sync.
  - **Sound Effects**: Harp glissandos, magical girl transformation chimes, cosmic bell arpeggios, celestial wand sparkles.
  - **OLED Text**: `🌙 Moon Healing Escalation!`, glittering crescent moon & star ASCII runes.

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

## 6. GitHub Repository Polish & Interactive GitHub Pages Simulator (Public Showcase)

> **User Request (2026-10-26):**
> *"так же добавь к задачам в конце оформить гитхаб + сделать в тему этого репозитория гитхаб пейджес чтобы люди могли посмотреть на имитацию того что в итоге собрано."*

### 6.1. GitHub Repository Polish & Presentation Showcase
- [ ] **Hero README & Presentation Overhaul**:
  - Add techno-arcane hero banner art, badges (RP2350 Pico 2 W, Android 15, CI build status, Python test pass).
  - High-res photo/video gallery of the physical staff: 43 WS2812B LEDs in action, SH1106 OLED prophecies, Google Pixel 9 Pro XL Companion App.
  - Clean architectural overview diagram (Staff Pico 2 W ↔ Wi-Fi Hotspot UDP 5005 ↔ Android Companion Brain ↔ Optional Pi 5).
  - Comprehensive Bill of Materials (BOM) with pinout wiring diagrams and assembly guide.
  - One-click GitHub Releases with pre-built UF2 firmware binaries (`celestial_firmware.uf2`) and Android Companion APKs (`app-debug.apk`).

### 6.2. Interactive GitHub Pages Simulator (Web Digital Twin of Celestial Engine Mk.V)
- [ ] **Themed Web Application hosted on GitHub Pages (`gh-pages` / `docs/`)**:
  - **Aesthetics & Theme**: 100% immersive celestial cyberpunk design system (`DeepVoid #0B0E14`, `AetherCyan #00E5FF`, `RunicGold #FFD700`, `NebulaPurple #7C4DFF`, sleek glassmorphism, glowing neons).
  - **Interactive 43-LED Staff Head Simulation**:
    - Canvas / WebGL rendering of the exact physical concentric topology (Center: 1, Inner: 6, Middle: 12, Outer: 24 LEDs).
    - Realistic optical bloom, persistence of vision, and full support for all 21 lighting animations (Solar Plasma, Galaxy Vortex, Radial Shockwave, Pulsar, Rainbow Wave, eBay auction strobe, Pokéball pulse).
  - **Virtual SH1106 OLED Display Emulator (128×64)**:
    - Retro monochrome pixel-perfect display rendering live menus, status telemetry, and animated text marquees.
  - **Web Audio API Chiptune & Sound Engine**:
    - Synthesizes 8-bit retro gaming chiptunes (Super Mario, Tetris, Hedwig's Theme, Pokémon Battle & Center, Sailor Moon) directly in the browser using square/triangle oscillators.
    - Plays all 8 cosmic sound effects (SFX 0..7) and combo sequences.
  - **Interactive Physical Controls**:
    - Clickable momentary QUERY button with hold-to-speak simulation.
    - Interactive Bourns PEC11R rotary encoder with rotation wheel and center-press switch to browse menus.
  - **Virtual Companion App & Spell Forge Sandbox**:
    - Embedded interactive companion phone UI allowing visitors to test the Grimoire, create custom spells in the Spell Forge, toggle `[ ⚡ SILENT ]` vs `[ ⚡ CHANT CAST ]`, and hear browser Web Speech API vocal chants.




