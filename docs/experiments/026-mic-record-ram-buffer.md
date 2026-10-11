# 026 — Microphone Voice Capture into RAM and Local Playback Test

Date: 2026-10-11. Target: Pico 2 W / RP2350. Main firmware: **1.0.11-VOICE-CAPTURE**.

## Goal
Capture continuous 16 kHz 16-bit PCM speech from the INMP441 microphone into Pico 2 W RAM while holding the QUERY button, and immediately play it back through the MAX98357A speaker upon release for instant hardware verification.

## Parts
The existing assembled staff: Pico 2 W, INMP441 microphone, MAX98357A amplifier + speaker, momentary QUERY button (GP9), SH1106 OLED. No new parts.

## Power off
No wiring changes are required for this milestone. Disconnect USB before modifying connections. Reconnect USB when ready to flash.

## Wiring table — unchanged
| Module | Module Pin | Pico Pin | GPIO / Function | Voltage / Note |
|---|---|---|---|---|
| INMP441 Mic | SCK | Pin 14 | GP10 / PIO0 SM0 | 3.3 V |
| INMP441 Mic | WS | Pin 15 | GP11 / PIO0 SM0 | 3.3 V |
| INMP441 Mic | SD | Pin 16 | GP12 / PIO0 SM0 | 3.3 V input |
| INMP441 Mic | L/R | GND | Common GND | Left channel selected |
| MAX98357A Amp | BCLK | Pin 21 | GP16 / PIO1 SM0 | 3.3 V |
| MAX98357A Amp | LRC | Pin 22 | GP17 / PIO1 SM0 | 3.3 V |
| MAX98357A Amp | DIN | Pin 24 | GP18 / PIO1 SM0 | 3.3 V |
| QUERY Button | Terminal 1 | Pin 12 | GP9 | 3.3 V pull-up, Active-LOW |
| QUERY Button | Terminal 2 | GND | Common GND | Ground |

## Code and build changes
1. `firmware/src/mic.h` & `firmware/src/mic.c`: Add `mic_record_start()`, `mic_record_stop()`, `mic_get_recorded_samples()`, and continuous circular draining into an 80,000-sample (5.0 s, 160 KB) RAM buffer.
2. `firmware/src/audio.h` & `firmware/src/audio.c`: Add `audio_play_pcm16()`, `audio_stop_pcm16()`, and `audio_is_pcm16_playing()` to support streaming and playback of raw 16-bit PCM buffers through the existing double-buffered I2S DMA pipeline.
3. `firmware/src/main.c`: Connect QUERY press to `mic_record_start()`, and QUERY release to `mic_record_stop()` followed by `audio_play_pcm16()`. Show `[ MIC ] RECORDING` on OLED during hold, and `[ SPK ] PLAYBACK` during replay before returning to `STATE_IDLE`.

## Commands — repository root
```powershell
.\scripts\check-audio.ps1 -RingExperiment
cmake --build firmware/build --target celestial_firmware --parallel 4
```

## Expected result
1. Press and hold QUERY: soft beep sounds, OLED displays `[ MIC ] RECORDING`, timer counts up, and live VU meter reflects voice level.
2. Speak clearly into the INMP441 microphone (e.g. "Testing staff microphone one two three").
3. Release QUERY: OLED switches to `[ SPK ] PLAYBACK`, and the speaker plays back your exact spoken voice with proper loudness and clarity.
4. When playback completes, OLED returns to normal dashboard (`IDLE`).
5. USB serial prints sample count, duration, and buffer size.

## Verification and actual result
1. Release build compiled cleanly with Pico SDK 2.3.1 for RP2350 ARM Secure. All regression suites passed.
2. Initial physical test confirmed capture and playback functioned as expected, but audio was quiet due to low raw MEMS microphone conversational amplitude (~1500–3000).
3. Added +14 dB digital pre-amp boost (`pcm16_gain = 5.0f`) in `fill_audio_buffer()` and raised default master volume to 35% (`AUDIO_DEFAULT_VOLUME = 35`).
4. Re-flashed and physically re-tested: user confirmed speech playback is loud, clear, and clearly audible ("теперь лучше!").
5. Milestone P8 (local microphone capture and speaker loopback) is **FULLY VERIFIED AND COMPLETE**.

| File | SHA256 |
|---|---|
| `firmware/build/celestial_firmware.uf2` | `805B73FC53837EB2FD4DC58B110431FEDAFEDA7B61D76141CC47A74E7530B7D1` |

Memory layout (ELF size):
- Flash: 492,624 bytes (~481 KB)
- RAM: 206,512 bytes (~201 KB out of 520 KB; ~318 KB free)

## Flashing command
```powershell
& 'C:/Users/AndrD/.pico-sdk/picotool/2.3.1/picotool/picotool.exe' load -v -x .\firmware\build\celestial_firmware.uf2 --ser 09457D708B309FED -f
```

## If it fails
1. If silence or faint playback: check volume setting (increase volume with encoder in volume menu) and speak within 5–10 cm of the INMP441 microphone hole.
2. If distorted or clipped: adjust distance or verify that volume is at comfortable level (e.g. 10–30%).
3. If no playback starts: check serial log for QUERY pressed / released messages and recorded sample count.

## Next milestone
Proceed to Milestone P9: Wi-Fi audio streaming protocol and mock Brain server on PC before Raspberry Pi 5 deployment.

