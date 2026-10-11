# Experiment 029 — Multilingual AI Voice Integration & Cloud Brain Server

**Date:** 2026-10-11  
**Status:** VERIFIED & READY TO FLASH  
**Target:** Raspberry Pi Pico 2 W (RP2350) + Python Cloud AI Brain Server  

---

## 1. Objectives
1. Implement multi-language dialect support in the Celestial Engine firmware and SH1106 OLED display:
   - `[EN]` — English (Default dialect)
   - `[AUTO]` — Dynamic language auto-detection and mirror matching
   - `[DE]` — German (Deutsch)
   - `[RU]` — Russian (Русский)
2. Transmit dialect code in `NET_MSG_QUERY_START` UDP payload (1 byte).
3. Confirm continuous, instantaneous audio streaming over UDP (256-sample / 16 ms chunks buffered during button press).
4. Integrate fast, low-cost cloud LLMs (DeepSeek V3 / Llama 3.3 70B / Gemini 2.0 Flash) with Edge-TTS (`en-US-AriaNeural`, `de-DE-ConradNeural`, `ru-RU-SvetlanaNeural`).
5. Ensure graceful offline fallback speech when API key is unconfigured or during network dropouts.

---

## 2. Firmware Architecture (`firmware/src/main.c`, `net_protocol.h`)
- Added `MODE_LANGUAGE` (`"VOX: DIALECT"`) into `staff_mode_t`.
- Added dedicated `draw_language_screen()` list selection UI:
  - Turn encoder to navigate languages (`> EN`, `> AUTO`, `> DE`, `> RU`).
  - Press encoder switch (SW) to confirm active dialect (`audio_play_chime()`).
  - Press QUERY button to exit without changes.
- Added dialect status badge `[EN]`, `[AUTO]`, `[DE]`, `[RU]` to all dashboard card header bars and inside `draw_listening_screen()`.
- Updated `net_client_send_query_start((uint8_t)active_lang_idx)` to pass the active dialect ID to the server.

---

## 3. Cloud Brain Server (`tools/ai_brain_server.py`, `tools/brain_config.json`)
- **Pipeline:**
  1. **STT:** `SpeechRecognition` using Google Web Speech API. If `[AUTO]`, detects Cyrillic, German, or English phonetics. If dialect pinned (`EN`/`DE`/`RU`), enforces that language.
  2. **LLM:** Configurable in `tools/brain_config.json` (OpenRouter / DeepSeek / Mistral / OpenAI compatible).
     - Recommended fast models: `deepseek/deepseek-chat` ($0.14/1M tokens), `meta-llama/llama-3.3-70b-instruct`, `google/gemini-2.0-flash-001`.
     - System prompts tailored per dialect.
  3. **TTS:** `Edge-TTS` with automatic retry backoff. Converts output to 16 kHz 16-bit mono PCM via `ffmpeg`.
  4. **Streaming:** 512-byte PCM chunks streamed over UDP to Pico 2 W `NET_MSG_RESP_CHUNK`.
  5. **Playback:** Pico 2 W routes samples directly into `audio_play_pcm16()` -> MAX98357A I2S DAC.

---

## 4. Verification & Diagnostics
- MSVC native tests: `.\scripts\check-audio.ps1` -> `Audio PCM check: PASS` (17 driver probes passed).
- Pico 2 W build: `cmake --build firmware/build --target celestial_firmware` -> `celestial_firmware.uf2` generated successfully (1,006,592 bytes).
- Multi-language TTS verification: `en-US-AriaNeural`, `de-DE-ConradNeural`, `ru-RU-SvetlanaNeural` tested and verified producing 16 kHz PCM.
