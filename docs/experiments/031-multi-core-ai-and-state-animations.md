# Experiment 031 — Multi-Core AI Presets, Reactive Ring Animations, Non-Blocking Server & 35s Timeout

## Date: 2026-10-11
## Status: Verified & Flashed

## Objectives
1. Fix dropped/unplayed speech response caused by 16s timeout during LLM/TTS generation.
2. Fix intermittent Wi-Fi connection drops caused by blocked single-threaded Python heartbeat replies.
3. Add reactive LED ring animations synchronized with staff states:
   - `LISTENING`: dynamic voice VU halo responding to live microphone level.
   - `PROCESSING`: spinning cosmic galaxy vortex (thinking mode).
   - `SPEAKING`: radiant solar plasma / living fire while voice is playing.
   - `IDLE`: user selected effect preset.
4. Add live AI Core Preset selector on OLED (`[05] AI MODEL`):
   - `0: GROK 2` (`x-ai/grok-4.5`)
   - `1: DEEPSEEK V3` (`deepseek/deepseek-chat`)
   - `2: LLAMA 3.3` (`meta-llama/llama-3.3-70b-instruct`)
   - `3: MINISTRAL 8B` (`mistralai/ministral-8b-2512`)
5. Elevate TTS and STT quality:
   - Russian voice: `ru-RU-DmitryNeural` (deep, majestic, clear arcane voice).
   - DC offset cancellation on microphone PCM before speech recognition.
   - Expand answer length to 2-3 sentences (35-45 words).
   - Accelerate UDP audio transmission from 12 ms down to 2.5 ms per chunk with duplicate start/end packets.

## Firmware Updates
- `firmware/src/net_protocol.h`:
  - Added `staff_ai_model_id_t` enum for synchronized AI core selection.
- `firmware/src/net_client.h` & `firmware/src/net_client.c`:
  - `net_client_send_query_start()` transmits `(lang_code, model_preset)`.
  - Added auto-start on chunk arrival and auto-finalize on 1500ms timeout for UDP packet loss resilience.
  - Heartbeat timeout tolerance increased to 8000 ms.
- `firmware/src/main.c`:
  - Added `MODE_AI_MODEL` to card menu (`[05] TARGET AI CORE`).
  - Added dual badge on dashboard headers: `[LANG|CORE]` (e.g. `[RU|GROK]`).
  - Implemented dynamic state-driven LED animations in main loop.
  - Increased `STATE_PROCESSING` timeout to 35 seconds and added active packet reception keep-alive.
- `tests/firmware_harness.c`:
  - Updated mock for `net_client_send_query_start`. All 17 probes PASS.

## Server Updates
- `tools/ai_brain_server.py`:
  - Re-architected with `threading.Thread`: main thread replies to heartbeats in 0.1 ms without blocking during LLM/TTS.
  - Integrated OpenRouter model routing for Grok 2, DeepSeek V3, Llama 3.3, and Ministral 8B.
  - Microphone PCM DC offset cancellation.
  - High-speed UDP streaming (2.5 ms pacing) with redundant start/end packets.
- `tools/brain_config.json`:
  - Configured DmitryNeural voice and extended 35-45 word prompts.

## Outcome
- Built cleanly and flashed to Pico 2 W via `picotool.exe load -f celestial_firmware.uf2`.
- Device boots with `[RU|GROK]` active by default.
