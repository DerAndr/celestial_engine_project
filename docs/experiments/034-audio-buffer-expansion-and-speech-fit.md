# Experiment 034 — Audio Buffer Expansion (12.0s), TTS Pace Acceleration & Complete Voice Playback

## Date
2026-10-11

## Objective
Prevent voice response truncation on longer spoken answers (e.g., 20+ words / >9 seconds) and guarantee that the entire synthesized sentence is played to the very last syllable on the staff speaker.

## Root Cause Analysis
In [firmware/src/net_client.c](file:///c:/projects/personal/hardware-01/celestial_engine_project/firmware/src/net_client.c), `RESP_BUFFER_MAX_SAMPLES` was previously defined as `144000` (9.0 seconds at 16 kHz = 288 KB).
When Edge-TTS synthesized the 20-word Russian sentence:
> *"Курица переходит дорогу, чтобы доказать петуху, что она не курица, а философ. Ирония в том, что петух даже не заметил её исчезновения."*

The total duration was 10.78 seconds (172,416 samples). All samples beyond 9.0 seconds were dropped at packet reception, cutting off the last 1.78 seconds ("...исчезновения"). Furthermore, standard Edge-TTS added ~0.5s of dead trailing silence.

## Implementation Details
1. **[firmware/src/net_client.c](file:///c:/projects/personal/hardware-01/celestial_engine_project/firmware/src/net_client.c)**:
   - Expanded `RESP_BUFFER_MAX_SAMPLES` from `144000` to `192000` (12.0 seconds = 384 KB).
   - RP2350 total BSS increased to 494.9 KB, leaving 37.5 KB of pure free SRAM for stack and heap.
2. **[tools/ai_brain_server.py](file:///c:/projects/personal/hardware-01/celestial_engine_project/tools/ai_brain_server.py)**:
   - Configured Edge-TTS speaking rate to `rate="+12%"` (crisp, natural conversational tempo).
   - Added ffmpeg filter `silenceremove=stop_periods=-1:stop_duration=0.25:stop_threshold=-45dB` to eliminate dead air padding.
   - Together with `rate="+12%"`, the 20-word sentence compressed from 10.78s down to 8.36s (267,658 bytes), fitting inside the 12.0s buffer (384,000 bytes) with 3.64s of margin.
   - Enforced prompt length in `query_llm`: 1–2 punchy sentences strictly under 20–25 words, with `max_tokens=85`.
   - Added safety clamp to guarantee audio never exceeds 380,000 bytes.
3. **[tools/brain_config.json](file:///c:/projects/personal/hardware-01/celestial_engine_project/tools/brain_config.json)**:
   - Calibrated English, German, Russian, and Auto prompts to 1–2 concise punchy sentences (under 20–25 words).

## Verification
- `scripts/check-audio.ps1`: All 17 tests passed.
- `cmake --build firmware/build`: Clean compilation.
- `arm-none-eabi-size`: `text: 511276`, `bss: 494964`, `dec: 1006240` (safe SRAM margins on RP2350).
- `picotool load -f -x firmware/build/celestial_firmware.uf2`: Flashed and rebooted.
- COM9 serial log verified:
  `[INFO] [HEARTBEAT] Tick #24 | OLED: ONLINE | State: IDLE | Mode: [DON'T PANIC] | Vol: 35% | Net: ONLINE (192.168.178.101) | Brain: READY | QRY: UP`.
