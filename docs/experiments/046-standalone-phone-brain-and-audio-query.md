# Experiment 046: Standalone Phone Brain Operation, Audio Query Packaging & Hardware Telemetry HUD

## 1. Goal
Fix voice query rejection bug on phone companion app, enable explicit host locking on Pico 2 W (Phone vs Pi 5 vs Auto Mesh), allow completely standalone operation of the staff with the phone without Raspberry Pi 5 or home Wi-Fi, and display comprehensive live hardware telemetry in the companion app.

## 2. Parts
- Raspberry Pi Pico 2 W (RP2350)
- Pixel 9 Pro XL running Celestial Companion App
- MAX98357A I2S DAC + 4 Ohm Speaker
- INMP441 I2S MEMS Microphone
- SH1106 I2C 128x64 OLED
- WS2812 43-LED concentric array

## 3. Root Cause Analysis
1. **Audio Query Rejection ("Пустой запрос / нет записи")**:
   - `GemmaModelEngine.kt` previously sent a dummy textual marker `"$prompt (Voice audio stream attached: ${audioBytes?.size} bytes)"` to the LLM backend without actually packaging the PCM audio buffer. The LLM saw text referencing an audio attachment without raw audio bytes and replied that the audio was empty.
   - Fixed by converting raw 16 kHz 16-bit mono PCM into valid RIFF WAV (`pcm16ToWav`) and passing it as base64 audio inside OpenRouter / Gemini standard `input_audio` schema.
2. **Pico Heartbeat / Pairing Chicken-and-Egg without Pi 5**:
   - Pico previously checked `ip_addr_cmp(addr, &companion_ip_addr)` before accepting heartbeats from non-Pi5 hosts, but `companion_ip_addr` was uninitialized (0.0.0.0). If Pi 5 was powered off, Pico never learned the phone's IP and kept sending UDP queries to the offline Pi 5 (`192.168.178.51`).
   - Relaxed heartbeat reception so any valid protocol heartbeat from a non-Pi5 IP immediately pairs `companion_ip_addr`, `companion_port`, and activates `HOST_TYPE_PHONE`.
3. **Explicit Host Lock**:
   - Added command `'H'` handling in firmware and added a 3-way toggle button group (`LOCK PHONE`, `LOCK PI 5`, `AUTO MESH`) in both `DashboardScreen` and `OracleTerminalScreen`.

## 4. Hardware Telemetry Card
Added a dedicated `LIVE HARDWARE TELEMETRY` card in `DashboardScreen.kt`:
- Target Endpoint (`192.168.178.101:5005` with click-to-edit IP dialog)
- AI Host Route (`PHONE BRAIN` vs `PI 5 BRAIN`)
- Lock Mode (`LOCKED PHONE`, `LOCKED PI 5`, `AUTO MESH`)
- Staff State (`LISTENING`, `PROCESSING`, `SPEAKING`, `IDLE`)
- DAC Audio State (`PLAYING (STREAM/TTS)` vs `STANDBY`)
- Audio Volume (`${volumePercent}%`)
- Clock Style (`${clockStyle.displayName}`)
- TTS Dialect (`${language.displayName}`)
- UDP Packet Tick counter
- Real-time INMP441 Mic VU Level meter bar (0..100%)

## 5. Standalone Hotspot Guide
Created `docs/guides/mobile-hotspot-standalone.md` documenting field deployment with Android Mobile Hotspot (2.4 GHz, `192.168.43.1`).
