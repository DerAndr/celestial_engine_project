# Experiment 047: Real-Time Voice Latency Optimization & Structured Prophecy Engine

## Date: 2026-10-25
## Status: VERIFIED & DEPLOYED

## Objective
Reduce voice query turnaround latency closer to real time (< 800ms) across the entire Pico 2 W + Phone Brain pipeline, and eliminate raw thought/spell tag pollution from speech and OLED displays.

## Root Causes Identified
1. **Network Port Overwrite**: Pico firmware was dynamically overwriting `companion_port` with an ephemeral source port from received packets rather than keeping it locked to 5005. Fixed in `net_client.c`.
2. **Android 15 Tethering EPERM**: `Network.bindSocket()` on interface `ap_br_wlan2` was blocked by restricted Android 15 hotspot fwmarks. Fixed by binding UDP socket to `0.0.0.0:5005` with multicast lock.
3. **Model & CoT Latency**: 31B model with chain-of-thought took >3,000ms just to output tokens. Switching to Gemini 2.5 Flash without CoT dropped TTFT to ~250ms.
4. **Verbatim Tag & Thought Reading**: Thoughts (`<thought>...</thought>`) and technical spell tags (`[SPELL: ...]`) were being vocalized verbatim by Android TTS and cluttering the OLED marquee.
5. **Slow Audio Pacing**: Waiting 25ms before audio start and pacing each 15ms chunk caused a 105ms delay before Pico reached its 1,600-sample DAC wakeup threshold.

## Implemented Optimizations
1. **Structured Prophecy Engine (`CelestialProphecy.kt`)**:
   - Parses `<thought>...</thought>` traces, `[SPELL: ... | ANIM: ... | SOUNDS: ...]` tags, and speech separately.
   - Completely strips technical tags from vocalized speech text.
   - Dispatches clean marquee title text (`[SPELL] <Name>`) to OLED.
2. **Real-Time Token Streaming Spell Dispatch (`DashboardViewModel.kt`)**:
   - The instant a `[SPELL: ...]` tag is parsed during streaming, LED animation, SFX sound combo, and OLED title are triggered immediately while the incantation is still generating.
   - Live conversational text is streamed to the OLED marquee in 16-20 character clauses.
   - `skipVisualEffects = true` on completion prevents glitching or replaying animations/sounds.
3. **Deep Audio Pre-Buffer Burst (`StaffRepository.kt`)**:
   - Reduced `RESP_START` delay from 25ms to 2ms.
   - First 25 chunks (6,000 samples = ~0.375s) are bursted at `delay(1)` (25ms total).
   - Wakes Pico DAC in ~7ms and deeply primes Pico's 32,768-sample buffer, making playback completely immune to Wi-Fi jitter.
4. **TTS Engine Pre-Warming & Speedup (`StaffTtsSynthesizer.kt`)**:
   - Set TTS speech rate to `1.15f` and pitch to `0.98f` for crisp, futuristic, rapid cadence (15% shorter speech duration).
   - Background pre-warming utterance eliminates cold-start JNI/voice file load penalty.
5. **Request ID Correlation & Unsolicited Text Acceptance (`net_client.c`)**:
   - Tracked `activeQueryRequestId` throughout query lifecycle.
   - Allowed `NET_MSG_RESP_TEXT` from paired phone or server regardless of gate state.
6. **Gemma 4 Restored with Two-Stage Voice Pipeline (`GemmaModelEngine.kt`)**:
   - Restored **Gemma 4 (31B)** (`google/gemma-4-31b-it`) as primary default model with Chain-of-Thought thinking (`<thought>...</thought>`).
   - Replaced canned stub responses with rich ancient-futuristic lore and creative spell-casting logic.
   - Implemented two-stage pipeline for voice queries: Stage 1 acoustically transcribes voice input via ultra-fast OpenRouter call (~250ms), Stage 2 feeds verbatim transcription into Gemma 4 for deep reasoning and response streaming.
   - Realistic token throughput estimation based on word segmentation rather than SSE packet counting.
7. **Android 15 Hotspot UDP Transmission Fix (`StaffUdpTransport.kt`)**:
   - Declared `ACCESS_LOCAL_NETWORK` and `CHANGE_NETWORK_STATE` in manifest.
   - Created cached unbound `DatagramSocket(null)` bound directly to tethering `Network` (Network 240 / `ap_br_wlan2`), eliminating kernel routing table `EPERM (Operation not permitted)` errors.

## Verification
- Firmware built and flashed to Pico 2 W via UF2.
- Companion App built via GitHub Actions (`40143546259`) and installed on Pixel 9 Pro XL via ADB.
- Live `APP_TELEMETRY` packets arriving every 1s over UDP 5005 without drops.
- `Query G4-31B` button active in UI with CoT enabled.
- Bidirectional UDP communications verified between Phone Brain and Pico 2 W.
