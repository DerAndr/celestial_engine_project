---
name: android-compose-development
description: >-
  Expert guidelines and anti-regression playbook for the Celestial Engine Mk.V Android 15+
  Companion App (Kotlin, Jetpack Compose, Wi-Fi Hotspot UDP v2 server, Gemma 4 E2B, OpenRouter,
  Arcane Grimoire [SPELL] engine, and Multimodal Vision). Use when editing or building android/.
---

# Android 15 Companion App (`android/`) Development & Anti-Regression Playbook

## 1. Wi-Fi Hotspot UDP v2 Server & Tethering `EPERM` Bypass (`StaffRepository.kt`)
- **Phone as Primary Hotspot Host (`port 5005`)**:
  - In standard operation, the smartphone runs its mobile Wi-Fi Hotspot (`10.111.174.x` / `10.169.248.x`), and the Pico 2 W staff connects directly to it.
- **Preventing `ErrnoException: sendto failed: EPERM` on Android Tethering**:
  - Android's Netd firewall blocks UDP broadcast/wildcard sends on cellular interfaces when Hotspot tethering is active.
  - Always bind/send UDP packets via the active tethering/Wi-Fi `NetworkInterface` (`ap0`, `wlan1`, `swlan0`, or the learned source `InetSocketAddress` of the staff's heartbeat packet).
- **Socket Configuration Ordering**:
  - Construct `DatagramSocket(null)`, set `reuseAddress = true`, set `soTimeout = 2000`, and only THEN call `.bind(InetSocketAddress(5005))`.
  - Always include a backoff delay (`delay(500)`–`delay(2000)`) inside socket error catch blocks.
- **Foreground Service Persistence (`StaffBrainService`)**:
  - Keep `StaffBrainService` active (`ID = 2026`) so Android 15 does not throttle UDP audio capture/playback or LLM coroutines when the phone screen is locked.

## 2. LLM Engine (`GemmaModelEngine.kt`), Personas & `[SPELL: ...]` Dispatch
1. **Default Model & Fallback**:
   - Default variant must remain **`GemmaVariant.GEMMA_4_E2B`** (`google/gemma-4-e2b-it` / local LiteRT-LM `.litertlm`), with automatic fallback to `google/gemma-4-26b-a4b-it`.
2. **Concise Oral Response Contract (`<= 25 words`)**:
   - All 6 personas (`OraclePersona.kt`: `MACHINE_GOD`, `ARCHMAGE`, `MARVIN`, `CYBERPUNK`, `PUMPKIN_LORD`, `CUSTOM`) must enforce **1–2 short sentences (max 15–25 words, `max_tokens = 128`, strictly no markdown, asterisks, or emojis)** so TTS speech finishes in `< 3 seconds`.
   - **Mandatory Parity Check**: Whenever editing `OraclePersona.kt`, synchronize `docs/twin/oracle/personas.json` and run `python tools/test_twin_acceptance.py`.
3. **Real-Time `[SPELL: ...]` Stream Interceptor (`DashboardViewModel.kt`)**:
   - System prompts instruct the LLM to prepend `[SPELL: Name | ANIM: Animation | SOUNDS: Sfx1, Sfx2, ...] <Incantation text>`.
   - Intercept `[SPELL: ...]` in the earliest streaming chunk, strip the tag from spoken TTS text, and immediately dispatch UDP commands for the matching LED pattern (`28` effects) and paced SFX chain (`750 ms` between SFX across `14` effects).
4. **Universal `[STOP / ABORT]`**:
   - Clicking **STOP / ABORT** must immediately cancel the active LLM coroutine, stop Android TTS, send an abort/idle command to the staff, and reset UI state to `IDLE`.

## 3. Memory Safety, Bounded Logs & Build Discipline
1. **Bounded Telemetry & Chat Buffers (No OOM / UI Jank)**:
   - Keep `StaffRepository` live log buffer capped at `MAX_LOG_ENTRIES = 100` (sliding window).
   - Keep `DashboardViewModel._chatHistory` capped at `MAX_CHAT_HISTORY = 40`.
2. **Multimodal Camera & Gallery Vision Downscaling**:
   - Always decode photos via two-pass `decodeSampledBitmap` (`max 800px`, EXIF orientation applied, intermediate bitmaps recycled, JPEG Base64 `< 100 KB`) and wrap network calls in `catch (e: Throwable)` so large photos never crash the app.
3. **Build & Clean Workspace Rule**:
   ```powershell
   cd android
   .\gradlew assembleDebug
   ```
   - Output APK is at `android/app/build/outputs/apk/debug/app-debug.apk`.
   - **Never** create `temp_apk*`, `build_apk`, or screenshot dumps in the repository root.
