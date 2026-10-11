# Experiment 036 — response completion, cancellation and full-screen text

Date: 2026-10-11. Prepared: **1.0.16-RESPONSE-FIX + server protocol v2**. New physical result pending; user confirms the preceding setup generally works.

## Goal
Play each generated response through its final word, reject obsolete answers, and show a large marquee across the entire OLED using the current stable three-ring assembly.

## Parts
Existing Pico 2 W, SH1106, controls, mic, one MAX98357A/speaker, three rings and current PC brain server. Fourth 16-LED ring was discarded by the user; no replacement or extra parts needed.

## Power off
No wiring changes. Disconnect USB before any physical wiring work or manual BOOTSEL reconnection. Existing rails and pin assignments stay as recorded in the master pinout.

## Wiring table — existing connections only

| Module pin | Pico physical pin | GPIO / function | Voltage / note |
|---|---:|---|---|
| SH1106 SDA / SCL | 4 / 5 | GP2 / GP3, I2C1 | Existing 3.3 V logic |
| INMP441 SCK / WS / SD | 14 / 15 / 16 | GP10 / GP11 / GP12, PIO RX | Existing 3.3 V |
| MAX98357A BCLK / LRC / DIN | 21 / 22 / 24 | GP16 / GP17 / GP18, PIO TX | Existing 3.3 V data; speaker only OUT+/OUT− |
| First ring DIN data path | 20 | GP15 / PIO0 SM1 | Existing chain 7→12→24; never apply 5 V to GPIO |

## Code/build changes
Native stream bounds and separate server PCM gain; request IDs/ACK/order/retry and cancellation; progress/deadlines; full-screen 8× text; unique WAV per worker. Main remains 43 LEDs, 18% cap, V35 default, SDK 2.3.1. GPIO and established local audio controls are unchanged. [Review](../reviews/2026-10-11-streaming-review.md).

## Commands — repository root

```powershell
cmake --build firmware/build --target celestial_firmware --parallel 4
.\scripts\check-audio.ps1
python -m unittest discover -s tests -p test_brain_transport.py -v
python tools/ai_brain_server.py
```

Stop the old server before restarting it; only one process can bind port 5005. Load the new `firmware/build/celestial_firmware.uf2` using the established BOOTSEL/UF2 procedure. Firmware and server must be updated together; old v1 is rejected. Software verification above uses no cloud queries; manual AI playback uses the existing configured providers. To isolate transport first, use `python tools/mock_brain_server.py` instead of AI, never both simultaneously.

## Expected result
SYSTEM shows server READY. Hold QUERY ≥1 s, speak, release: PROCESSING remains active while generation reports progress; SPEAKING displays large moving response letters with no header/equalizer. Entire audio plays once through the last word, then IDLE. Make a new request during the old operation: the old answer must not arrive afterwards. Local modes and three-ring lighting remain available without a brain connection. Corrected server scaling may sound quieter than the previously clipped signal; use the existing VOLUME setting.

## If it fails
1. Verify both sides are updated: Pico `1.0.16-RESPONSE-FIX`, server protocol 2; stop any old/background server using port 5005.
2. Confirm SYSTEM READY, existing server IP and Wi-Fi. Compare mock and AI to separate transport from external-provider generation.
3. Keep the server request ID/result log and Pico timeout/failure log for the same request; note whether its final word, display or state failed. Do not change soldering or I2S framing for a server-generation issue.

## Documentation update
Inventory/pinout/PROJECT/tasks/map now show three rings and the discarded fourth. [Protocol](../protocol.md) and [Pi 5 handoff](../pi5-next-stage.md) describe the paired update and next step. Pause the next physical milestone until the user reports this new observation; no agent flash or physical PASS is claimed.

## Software verification result
- Native C audio/control/display checks PASS, including the existing 17 probes and new stream bounds, dynamics, V0 completion and response ordering/cancellation regressions.
- Three Python transport checks PASS. The 14 s response test delivers all 448000 PCM bytes despite deliberately losing a packet and its ACK; no cloud calls.
- Pico SDK build PASS for `pico2_w` / `rp2350-arm-s` / Release / SDK 2.3.1. ELF text 513172 bytes, BSS 177052 bytes; 2 s microphone RAM retention is 64 KB, not the historical 5 s/160 KB description.
- Python files and interactive-map JavaScript parse successfully. New physical observation still pending.

Main local artifact: `firmware/build/celestial_firmware.uf2`; versioned copy `celestial_response_fix_1.0.16.uf2`. Git keeps source/docs; build artifacts stay local. User-confirmed preceding source milestone is preserved in commit `1e9ba65`.

## Iteration update (Oct 11, 2026 — Speech & Audio Tuning)
1. **Marquee Speed & Layout**: Restored the dashboard marquee box with dancing equalizer audio bars and boosted scroll rate from 45 ms/px to 15 ms/px (~11 chars/s) matching natural spoken cadence.
2. **Audio Volume Baseline**: Default boot volume set to 30% (`AUDIO_DEFAULT_VOLUME 30`), with a 26 dB curve (`AUDIO_VOLUME_RANGE_DB 26.0f`) and 1.15f stream gain, providing a clear, punchy baseline at 30% without clipping. Optional hardware boost: wire MAX98357A GAIN pin to VIN (5V) for 15 dB analog boost.
3. **Response Length & Kin-dza-dza Flavor**: Expanded prompts to 2-3 lively sentences (~35-50 words) infused with retrofuturistic Gravitsappa hyperdrive enthusiasm, green tsak, and Douglas Adams wit.
4. **Russian Synthesis & STT**: Added `gTTS` fallback to eliminate Edge-TTS 403 failures on Russian/German, and matched output dialect to detected Cyrillic in the generated response.
5. **Galaxy, Earth & Halloween LED Animations**: Added `LED_ANIM_GALAXY` (Sagittarius A* core and orbiting Sol star), `LED_ANIM_EARTH` (rotating azure oceans, emerald land, polar aurora), and `LED_ANIM_HALLOWEEN` (Jack-o'-lantern candle flame, witch purple, toxic ectoplasm green).
6. **Techno-Fantasy Logo**: Replaced derpy face with a detailed 24x24 celestial astrolabe runic sigil featuring cardinal energy spires and an arc-reactor diamond core.


