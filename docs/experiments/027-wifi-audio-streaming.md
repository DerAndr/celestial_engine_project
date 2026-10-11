# 027 — Wi-Fi Audio Streaming between Pico 2 W and Mock Brain Server (P9)

Date: 2026-10-11. Target: Pico 2 W / RP2350. Main firmware: **1.0.12-WIFI-STREAM**.

## Goal
Stream 16 kHz 16-bit PCM voice captured from INMP441 over Wi-Fi UDP to a Python mock Brain server on PC upon QUERY press, save it as `query.wav`, and receive/play back a streaming audio response through MAX98357A.

## Parts
The existing assembled staff: Pico 2 W, INMP441 microphone, MAX98357A amplifier + speaker, momentary QUERY button (GP9), SH1106 OLED. No new parts or wiring.

## Power off
The board is currently disconnected. No wiring modifications are required. Plug USB back into PC when ready to test or flash.

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
1. `firmware/src/net_protocol.h`: Lightweight binary packet definition over UDP (magic `CELE`, version 1, msg types: QUERY_START, AUDIO_CHUNK, QUERY_END, RESP_START, RESP_CHUNK, RESP_END).
2. `tools/mock_brain_server.py`: Python UDP server on port 5005 that receives PCM packets, saves `tools/query.wav`, and streams back a synthesized test audio response.
3. `firmware/src/wifi_credentials.h`: Local SSID and password configuration (git-ignored for security).
4. `firmware/src/net_client.h` & `firmware/src/net_client.c`: Non-blocking UDP audio client using `pico_cyw43_arch_lwip_threadsafe_background`.

## Commands — repository root
```powershell
python tools/mock_brain_server.py
```

## Expected result
1. `mock_brain_server.py` runs on PC and prints `Listening on UDP port 5005`.
2. Pico 2 W boots, connects to Wi-Fi in background, OLED displays `WIFI: ONLINE` and assigned IP.
3. Holding QUERY sends audio chunks over Wi-Fi; PC logs incoming packet stream.
4. Releasing QUERY completes transfer; PC saves `tools/query.wav` and sends back response audio.
5. Pico receives response packets and plays them through speaker.

## Live continuous streaming (up to 60 seconds) & UI Redesign
1. In experiment 026, recording was restricted to 5.0 seconds because audio was stored strictly in Pico internal RAM (160 KB). In 027 (Wi-Fi streaming), microphone samples are now streamed live in real time as 256-sample (16 ms) UDP packets over Wi-Fi while QUERY is held. The host server (PC / Raspberry Pi 5) buffers incoming speech without microcontroller RAM limits, supporting queries up to 60.0 seconds.
2. Dashboard menu redesigned from a cluttered single screen into 8 spacious, dedicated paginated cards (Main Oracle, Volume, LED Core, Mic VU Meter, Chiptune Jukebox, Voice Phrases, Cosmic SFX, System/Diagnostics).
3. Added a smooth, 20 FPS marquee ticker at the bottom of the OLED for contextual info and commands that exceed screen width.
4. Updated '!67!' easter egg trigger: removed premature 6->7 transition; now triggers strictly at rotation position 67.

## Verification and build status
Release build compiled cleanly with Pico SDK 2.3.1 for RP2350 ARM Secure. All native audio/LED regression tests passed.

| File | SHA256 |
|---|---|
| `firmware/build/celestial_firmware.uf2` | `42DC6649CB917227EE1CC5BBEE775418C5126D01C844B74FCF658EE2FE8941A5` |

- **Firmware Version**: `1.0.12-WIFI-STREAM`
- **Codename**: `AURA-STREAM`
- **Target**: `RP2350 (ARM-S)`
- **Emblem**: 24x24 Cyber-Arcane Mandala & Radiant Celestial Star bitmap (`CELESTIAL_LOGO_24x24`)
- **Cyberpunk / Techno-Fantasy HUD Overhaul**:
  - Telemetry & Utilitarian Aesthetics: Crisp monospace layout, reticle corner ticks, rune brackets, real-time bus status (`[AETH:OK]`, `RUNE: 4R-59L [OK]`, `AETH: I2S-DMA 16k`, `FLUX: %d%% | 133MHz`, `NODE: %s`).
  - Arcane-Cyber Mode Designations: `AURA-CORE`, `SOL-3 ORRERY`, `FLUX: VOLUME`, `RUNE: AURA LED`, `SENS: VOX MIC`, `WAVE: CHIPTUNE`, `VOX: LITURGY`, `AETH: COSMIC FX`, `GRID: NET TELEM`, `KERN: SYS STATUS`.
  - Marquee Telemetry Tickers with Clarke's Third Law ("Any sufficiently advanced technology is indistinguishable from magic").
- **Full 4-Ring LED Integration (59 LEDs: 7 + 12 + 24 + 16)**:
  - Eliminated arbitrary 5-second diagnostic RGB color flashes; restored smooth, continuous animations across all 4 concentric physical rings.
  - `COSMIC SNAKE`: Serpent body now glides continuously through all 59 LEDs without truncating at Ring 2.
  - `STARRY NIGHT`: Diamond starlight twinkles seamlessly across all 59 LEDs with bounded glimmer (no harsh 255 spikes).
  - `RAINBOW WAVE`: Full concentric chromatic spiral across all 4 rings with pure saturated hues (`s=255, v=180`) preventing white washout.
  - `PULSAR`: Four-stage outward shockwave ripple (`Core -> Ring 0 -> Ring 1 -> Ring 3 -> Ring 2`) with true black background between heartbeats.
  - `SOLAR ORRERY`: Full 4-ring planetary system: Sun (Core) -> Earth & Moon (Ring 1) -> Mars & Asteroids (Ring 2) -> Jupiter & Galilean Moons Io/Europa (Ring 3).
  - `SOLAR PLASMA`: Living undulating solar corona fire with smooth harmonic synthesis across all 59 LEDs.
  - `VOICE REACTIVE`: Concentric VU progression across all 4 rings (`7 -> 12 -> 16 -> 24`).
- **Short / Empty Query Gate**:
  - If QUERY hold is < 1.0 s or speech peak amplitude is < 350 (noise floor only): query is immediately aborted locally, `NET_MSG_CANCEL` is sent to server (server discards buffer and does NOT write WAV), and staff emits a soft cancel blip without entering upload mode.
- **Pre-Recording Readiness Gate**:
  - Automatically pings Mock Brain Server via UDP heartbeats every 1.0 s.
  - If server is OFFLINE or Wi-Fi is disconnected when pressing QUERY: prevents recording into the void, sounds a 220 Hz warning buzz, and displays diagnostic screen (`[! BRAIN UNREADY !]`).
  - If Brain is READY: sounds a crisp 880 Hz prompt chirp (A5) and starts streaming immediately.
- **Upload Progress HUD (Zero Echo)**:
  - User voice is NEVER echoed through the staff speaker.
  - Upon releasing QUERY, enters `STATE_PROCESSING` and displays animated `[+ ASTRAL UPLOAD +]` screen with progress bar.
  - Completes with a harmonic completion chime and returns to `STATE_IDLE`.
- **Query Recording Storage**: Audio streamed live via UDP and saved to `tools/query.wav` and archived into `tools/recordings/query_YYYYMMDD_HHMMSS.wav`.
- **Direct Flash**: Programmed autonomously via `picotool` (100% written, 100% verified, rebooted cleanly).

## Flashing command
```powershell
& 'C:/Users/AndrD/.pico-sdk/picotool/2.3.1/picotool/picotool.exe' load -v -x .\firmware\build\celestial_firmware.uf2 --ser 09457D708B309FED -f
```

## If it fails
1. If Wi-Fi fails to connect: verify SSID and password in `firmware/src/wifi_credentials.h`. Check serial logs for connection state.
2. If PC doesn't receive packets: check Windows Firewall for UDP port 5005.
3. If no audio plays: ensure Pico speaker volume is non-zero (35% default).

## Documentation update
Record connection speed, packet loss, and audio fidelity in this document.
