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

### Task 1: Phone + Pico Default Mode & Firmware/App Alignment
- [ ] **Firmware (`net_client.c`)**:
  - Change default `preferred_host` from `HOST_TYPE_NONE` to `HOST_TYPE_PHONE`.
  - Default to `DEFAULT_PHONE_CATALOG` on startup without waiting for Pi 5 discovery timeout.
  - Set default server IP to Phone Hotspot gateway (`192.168.43.1` or dynamically resolved gateway from lwIP `netif_default->gw`).
- [ ] **Android App (`StaffRepository.kt` & `MainActivity.kt`)**:
  - Lock default `ConnectionMode` to `PHONE_BRAIN_SERVER`.
  - Automatically acquire `MulticastLock` and bind UDP port 5005 on application launch.
  - Run foreground service `StaffBrainService` to prevent OS sleep/throttling when phone screen is locked.
- [ ] **Preserve Raspberry Pi 5 Switchability**:
  - Keep menu item `[HOST]` on OLED with choices: `PHONE BRAIN (Default)`, `PI 5 BRAIN`, and `AUTO MESH`.
  - Keep Companion App dashboard toggle to hand off brain execution to Pi 5.

### Task 2: Mobile Hotspot First Network Flow (Zero External Wi-Fi)
- [ ] **Firmware (`wifi_credentials.h` & `net_client.c`)**:
  - Primary network profile (`WIFI_SSID` / `WIFI_PASSWORD`): Phone Hotspot credentials.
  - Fallback profile (`WIFI_FALLBACK_SSID` / `WIFI_FALLBACK_PASSWORD`): Home network (Fritz!Box).
  - Dynamic Gateway Discovery: Use `ip4_addr1(&netif_default->gw)` so if phone assigns IP via DHCP, Pico automatically knows the host IP without hardcoding.
- [ ] **Android App Hotspot Helper**:
  - Add Hotspot status banner on Dashboard showing: `Hotspot: Active (192.168.43.1) -> Staff Linked`.

### Task 3: Universal Emergency Stop / Abort Button (Pico + Android)
- [ ] **Android Companion App**:
  - Add prominent **[STOP / ABORT]** button on Dashboard when state is `LISTENING`, `PROCESSING`, or `SPEAKING`.
  - On press:
    1. Cancel LLM generation coroutine (`activeGenerationJob?.cancel()`).
    2. Abort TTS audio synthesis (`StaffTtsSynthesizer.stop()`).
    3. Send UDP `NET_MSG_CANCEL` (0x08) to Pico.
    4. Reset app state immediately to `IDLE`.
- [ ] **Pico Physical Staff Interrupt**:
  - During `STATE_PROCESSING` or `STATE_SPEAKING`:
    - Clicking the rotary encoder button (PEC11R SW) or pressing QUERY immediately triggers `net_client_cancel()`.
    - `net_client_cancel()` stops speaker DMA audio, clears `response_gate`, sends `NET_MSG_CANCEL` to phone, and resets OLED/LEDs to `STATE_IDLE`.

### Task 4: AI Spells & Dynamic Arcane LED Incantations
- [x] Initial specification and brainstorming documented in [ai_spells_and_dynamic_lighting.md](architecture/ai_spells_and_dynamic_lighting.md).
- [ ] **2D Polar Kinematics Engine in `leds.c` (43 LEDs: 7→12→24)**:
  - `SPELL_ANIM_SPIRAL_SNAKE`: Multi-ring Ouroboros serpent coiling through 24 -> 12 -> 7 with core flare.
  - `SPELL_ANIM_RADIAL_WAVE`: Concentric ripples & shockwaves propagating outward (or imploding inward).
  - `SPELL_ANIM_COLLIDING_WAVES`: Counter-rotating wave crests colliding at 0°/180° with white constructive flash.
  - `SPELL_ANIM_ARCHIMEDES_SPIRAL`: Hypnotic rotating spoke with radial phase lag across concentric rings.
  - `SPELL_ANIM_SACRED_POLYGON`: Stroboscopic geometric runes (Triquetra, Hexagram / Merkaba) rotating at harmonic ratios.
  - Phonetic VU Coupling: Real-time amplitude modulation from speaker DMA (words glow, plosives shockwave).
- [ ] **LLM Prompt Arcane Tag Injection (`GemmaModelEngine.kt`)**:
  - System prompt instructs LLM to prepend structured spell tag to voice incantations:
    `[SPELL: Name | ANIM:Type | DIR:Dir | LEN:n | SPEED:ms | HEAD:#Hex | BODY:#Hex | TAIL:#Hex | VU:Mode] Text...`
- [ ] **Fast Dispatch Protocol**:
  - Android parser extracts tag from first stream chunk and dispatches `NET_MSG_SPELL_CAST` instantly so Pico lights up during speech generation.

---

## 3. Active Debugging: BUG-VOICE-01 (End-to-End Voice Cycle)
- Verify phone captures audio from Pico over Hotspot UDP 5005.
- Verify OpenRouter / Gemini streams tokens into `DashboardViewModel`.
- Verify Android TTS generates 16 kHz PCM and `streamAudioToStaff` transmits chunks back.
- Verify Pico speaker plays the speech response clearly.
