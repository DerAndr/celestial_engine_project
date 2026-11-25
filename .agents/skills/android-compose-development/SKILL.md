---
name: android-compose-development
description: >-
  Provides architecture guidelines and an anti-regression playbook for the Celestial Engine Mk.V Android 15+
  Companion App (Kotlin, Jetpack Compose, Wi-Fi Hotspot UDP v2 server, Gemma 4 E2B, OpenRouter,
  Arcane Grimoire [SPELL] engine, and Multimodal Vision). Use this skill when the user asks to edit,
  build, or debug the Android application in android/.
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
3. **Build & Deployment Pipeline (Cloud GitHub Actions CI/CD + `adb` — No Local Android SDK on Host)**:
   - **CRITICAL AGENT RULE (Do NOT Search for Local Android SDK)**: This Windows workstation does **NOT** have a local Android SDK (`Android\Sdk`) installed. **NEVER** waste `run_command` turns searching the filesystem for `ANDROID_HOME` or running `.\gradlew` locally. Instead:
     1. Run `python tools/preflight_check.py` locally (`< 1s`) to verify Kotlin 2.0 and JUnit 4 bytecode invariants before pushing.
     2. Push the branch and let GitHub Actions (`.github/workflows/android-build.yml`) run `./gradlew testDebugUnitTest assembleDebug --no-daemon --stacktrace`.
   - **Single Source of Truth for Version**: `versionName` and `versionCode` in `android/app/build.gradle.kts` drive the entire release and web ecosystem.
   - **Pinned Debug Keystore (`android/app/celestial-debug.keystore`)**: Keep `signingConfigs.getByName("debug")` pointing to `celestial-debug.keystore` so cloud-built APKs always upgrade in-place via `adb install -r` without `INSTALL_FAILED_UPDATE_INCOMPATIBLE` or wiping downloaded ONNX models.
   - **Cloud Build & Physical Smartphone Deployment (`Pixel 9 Pro XL` `49051FDAS001YK`)**:
     ```powershell
     git push origin <branch>
     Start-Sleep -Seconds 4
     $runId = (gh run list --workflow=android-build.yml --branch=<branch> --limit 1 --json databaseId -q '.[0].databaseId')
     gh run watch $runId --exit-status
     Remove-Item -Recurse -Force scratch/apk -ErrorAction SilentlyContinue
     gh run download $runId -D scratch/apk
     $apk = (Get-ChildItem -Path scratch/apk -Filter "celestial_companion_*.apk" -Recurse | Select-Object -First 1).FullName
     adb -s 49051FDAS001YK install -r "$apk"
     adb -s 49051FDAS001YK shell am start -n com.celestial.companion/.MainActivity
     Remove-Item -Recurse -Force scratch/apk -ErrorAction SilentlyContinue
     ```
   - **Zero Root Clutter Rule**: **Never** create `temp_apk*`, `build_apk`, or screenshot dumps in the repository root. Always use git-ignored `scratch/apk/` and delete only `scratch/apk/`.

## 4. Kotlin 2.0, JUnit 4 & ONNX Runtime Anti-Defect Rules
1. **No `break` or `continue` Inside `.use { ... }` Lambdas (Kotlin 2.0 Error)**:
   - Kotlin 2.0 rejects non-local `break`/`continue` inside inline `.use { ... }` lambdas (`The feature "break continue in inline lambdas" is experimental`). Always write explicit `val res = session.run(inputs); try { ... } finally { res.close() }`.
2. **No Illegal JVM Characters in JUnit 4 Backtick Test Names**:
   - Android JVM/DEX bytecode forbids `<`, `>`, `.`, `;`, `[`, `]`, and `/` inside backtick method names (`@Test fun \`...\`()`). Write `"at most 25 words"` instead of `"<= 25 words"`.
3. **Dynamic ONNX Integer Tensor Typing (`INT32` vs `INT64`)**:
   - `sherpa-onnx` models (`Moonshine`, `SenseVoice`) expect `OnnxJavaType.INT32` (`IntBuffer`), while HuggingFace Optimum models (`SmolLM2-135M`, `Kokoro-82M`) expect `INT64` (`LongBuffer`). Always inspect `(session.inputInfo[name]?.info as? TensorInfo)?.type` via `createTypedIntTensor` rather than hardcoding `LongBuffer`.
4. **Pure JVM Unit Test Dependencies**:
   - Keep `testImplementation("org.json:json:20240303")` and `testOptions { unitTests.isReturnDefaultValues = true }` in `android/app/build.gradle.kts` so `org.json.JSONObject` and `android.util.Log` work in pure JVM tests.
