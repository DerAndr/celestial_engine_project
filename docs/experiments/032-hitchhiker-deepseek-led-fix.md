# Experiment 032 — Hitchhiker's Guide Theme, DeepSeek V3 Fast Default, Svetlana TTS & Reactive LED Fix

## Date
2026-10-11

## Objective
1. Fix LED ring animation sticking/not running during speech interaction states.
2. Resolve Microsoft Edge-TTS HTTP 403 Forbidden on Russian voice (`ru-RU-DmitryNeural`) by switching to stable `ru-RU-SvetlanaNeural` and adding an automatic fallback voice cascade.
3. Transform the OLED interface into the style of Douglas Adams' "The Hitchhiker's Guide to the Galaxy" ("DON'T PANIC", "SECTOR ZZ9", "EARTH: MOSTLY HARMLESS", "DEEP THOUGHT", "BABEL FISH", "42").
4. Configure DeepSeek V3 (`deepseek/deepseek-chat`) as the default AI model for fast (<1.5s), cheap (~$0.14/M tokens), and witty responses.

## Root Cause Analysis
1. **LED Animation Sticking**: In [firmware/src/leds.c](file:///c:/projects/personal/hardware-01/celestial_engine_project/firmware/src/leds.c), line 770 previously read:
   ```c
   int current_mode = led_menu_active ? selected_anim_idx : active_anim_mode;
   ```
   When outside the LED menu, `led_menu_active` is `false`, so `selected_anim_idx` (which carried `anim_to_run` for `LED_ANIM_VOICE`, `LED_ANIM_VORTEX`, `LED_ANIM_PLASMA`) was discarded. Fixed by setting `current_mode = selected_anim_idx;`.
2. **Edge-TTS 403 Forbidden**: Microsoft's consumer speech endpoint has restricted `ru-RU-DmitryNeural`. Testing verified that `ru-RU-SvetlanaNeural` synthesizes cleanly without 403 errors. In [tools/ai_brain_server.py](file:///c:/projects/personal/hardware-01/celestial_engine_project/tools/ai_brain_server.py), `synthesize_speech_pcm` now replaces Dmitry with Svetlana and includes an automatic multi-voice retry cascade.

## Changes Made
1. [firmware/src/leds.c](file:///c:/projects/personal/hardware-01/celestial_engine_project/firmware/src/leds.c):
   - Set `current_mode = selected_anim_idx;` in `leds_update()`.
   - Suppressed unused parameter warning with `(void)led_menu_active;`.
2. [firmware/src/net_protocol.h](file:///c:/projects/personal/hardware-01/celestial_engine_project/firmware/src/net_protocol.h):
   - Reordered `staff_ai_model_id_t`: `AI_MODEL_DEEPSEEK = 0` (Default: Deep Thought), `AI_MODEL_MISTRAL = 1` (Eddie), `AI_MODEL_GROK = 2` (Grok 4.5), `AI_MODEL_LLAMA = 3` (Marvin).
3. [firmware/src/main.c](file:///c:/projects/personal/hardware-01/celestial_engine_project/firmware/src/main.c):
   - Re-themed menus and screens to Hitchhiker's Guide: `DON'T PANIC`, `SECTOR ZZ9`, `SOL SYSTEM`, `EARTH: HARMLESS`, `SHIP COMPUTER`, `BABEL FISH`, `PAN-GALACTIC VOL`, `IMPROBABILITY`, `SUB-ETHA MIC`, `8-BIT JUKEBOX`, `THE ORACLE`, `GUIDE SYSTEM`.
   - Recording screen: `[ DON'T PANIC ]`, `SUB-ETHA RECORDING...`, `RELEASE TO CONSULT GUIDE`.
   - Thinking screen: `[ DEEP THOUGHT ]`, `CONSULTING THE GUIDE...`, `HAVE A CUP OF TEA...`.
   - Speaking screen: `[ THE GUIDE SPEAKS ]` / `[ SHARE & ENJOY! ]`, `SUB-ETHA AUDIO PLAYBACK`.
   - Russian voice label updated to `VOX : RU-RU (SVETLANA)`.
4. [tools/brain_config.json](file:///c:/projects/personal/hardware-01/celestial_engine_project/tools/brain_config.json):
   - Default model set to `deepseek/deepseek-chat`.
   - Default Russian TTS voice set to `ru-RU-SvetlanaNeural`.
   - Added Douglas Adams witty Hitchhiker persona prompts (2-3 sentences, 30-40 words, no markdown/emojis).
5. [tools/ai_brain_server.py](file:///c:/projects/personal/hardware-01/celestial_engine_project/tools/ai_brain_server.py):
   - Updated `MODEL_PRESETS` (0: Deep Thought, 1: Eddie, 2: Grok, 3: Marvin).
   - Added multi-voice retry fallback in `synthesize_speech_pcm` to ensure 0-byte audio never occurs.
   - Updated offline fallback responses to Hitchhiker's Guide lore ("DON'T PANIC", answer is 42).

## Verification
- `scripts/check-audio.ps1`: All 17 C test harness tests passed.
- `cmake --build firmware/build`: Built cleanly without errors or warnings.
- `picotool load -f -x firmware/build/celestial_firmware.uf2`: Successfully flashed to Pico 2 W flash.
- Serial verification over COM9:
  `[INFO] [HEARTBEAT] Tick #22 | OLED: ONLINE | State: IDLE | Mode: [DON'T PANIC] | Vol: 35% | Net: ONLINE (192.168.178.101) | Brain: READY | QRY: UP`.
