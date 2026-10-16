# Experiment 045: Chronos Timezone Sync, Audio Stutter Fix, and High-Speed Pi 5 AI

## Date
2026-10-16

## Objectives
1. Fix Chronos clock synchronization from Android Companion to Pico 2 W so the staff clock matches the local timezone (UTC+2) rather than UTC.
2. Eliminate the repetitive "Voice... Voice..." audio stutter when receiving responses from Raspberry Pi 5 or smartphone.
3. Benchmark and deploy ultra-fast local LLM models on Raspberry Pi 5 to eliminate latency bottlenecks when querying local AI.

## Root Cause Analysis
1. **Chronos Timezone Offset**:
   - `StaffCommand.SyncTime` sent `System.currentTimeMillis() / 1000` (UTC epoch).
   - Pico's `clock_base_seconds = (epoch_seconds % 86400)` resulted in GMT/UTC, making the staff clock 2 hours behind local time in UTC+2.
   - Solution: added local timezone offset `(now + TimeZone.getDefault().getOffset(now)) / 1000`.
2. **Audio Stutter ("Voice...")**:
   - In `firmware/src/net_client.c`, `NET_MSG_RESP_TEXT` was received but when unsolicited, `acknowledge_response(&hdr, addr, port)` was not invoked.
   - Pi 5 server retransmitted unacknowledged text packets every 200 ms.
   - In `firmware/src/main.c`, receiving unsolicited text triggered `audio_play_voice(VOICE_ORACLE)`, which starts with "Voice Oracle activated."
   - Every retransmission interrupted playback and restarted the clip, causing the loop: "Voice... Voice... Voice...".
   - Solution: Added ACK reply for unsolicited text and replaced the voice trigger with `audio_play_blip()`.
3. **Pi 5 Local Model Latency**:
   - `gemma4:e2b` takes ~9.0 seconds per inference on Pi 5 CPU.
   - Direct benchmark on Pi 5 showed:
     - `qwen2.5:0.5b`: **0.90 seconds** (instant response, fluent Russian).
     - `qwen2.5:1.5b`: **5.88 seconds**.
     - `llama3.2:1b`: **9.98 seconds**.
   - Solution: Configured `qwen2.5:0.5b` (Qwen 0.5B Flash) and `llama3.2:1b` into Pi 5 brain server and Pico firmware catalog.

## Verification
- Pico 2 W firmware recompiled and flashed via `picotool`.
- Pi 5 brain service restarted with `qwen2.5:0.5b` as default local model.
- Android APK rebuilt and installed on Google Pixel 9 Pro XL.
