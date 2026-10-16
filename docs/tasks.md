# Tasks — Reliable Voice Exchange, Then Pi 5

Updated 2026-10-11 after new commits and user observations. The staff operates stably overall; voice queries, AI response generation, and synthesized speech playback are fully implemented on PC/Cloud.

## Current State

- **Fourth LED ring discarded by user** due to ongoing hardware instability. Working ring chain: 7→12→24, 43 LEDs total, 18% power limit. Jumper repair is preserved history, not current build.
- Voice recording and live Wi-Fi upload up to 60 s; language and model preset selected directly on Pico. The server executes STT → LLM → TTS and streams audio back. Locally in Pico RAM, only the first 2 s (64 KB) are retained.
- User base version is 1.0.15; prepared version 1.0.16 resolves response overflow, delayed responses, and fullscreen text display. See [036](experiments/036-response-review-and-fullscreen-text.md), [review](reviews/2026-10-11-streaming-review.md).

## Priorities

1. **Verify new pair Pico 1.0.17 + v2 server on PC/Cloud.** Full lengthy response, completion of final word, fullscreen text line across SH1106 OLED, subsequent QUERY handling, and purging stale responses.
2. **Universal "Brain" and Mobile Hotspot (Zero-App).** Complete independence from wearable Pi 5 and external Android apps:
   - Smartphone operates strictly as a standard Wi-Fi Hotspot (no APK, Termux, or background daemon services required).
   - Server `ai_brain_server.py` runs on PC, cloud (Docker / VPS), or laptop.
   - Dual network profile support (Home Wi-Fi + Mobile Hotspot) with automated failover.
3. **Onboard Staff Resilience (Standalone Offline Oracle).** In the absence of network connectivity or server reachability, holding QUERY triggers local audio sensing, concentric LED vortex animation, and delivers a retro-futuristic prophecy accompanied by local flash-stored audio cues (English default, Deutsch, Russian). The staff never bricks when disconnected.
4. **Astronomical Chronometer and Sci-Fi Demo Suite.** [038](experiments/038-celestial-chronometer-clock.md), [039](experiments/039-multi-clock-r2d2-astromech.md):
   - **`DIAL CHRONOS`**: 12-hour directional dial with golden hand and comet indicator.
   - **`AETHER HOURGLASS`**: Orientation-independent fluid accumulator (1..12H in gold, 0..24 LED minutes in cyan, 0..6 LED seconds in amethyst).
   - **`CHROMA PRISMATIC`**: Complete spectral time encoding independent of orientation: middle ring illuminates current hour hue (12 spectral tints), outer ring shifts color every 10 minutes across 6 decades, inner ring illuminates spectral gemstones every 10 seconds.
   - **`CELESTIAL ORRERY`**: Kinetic planetary orrery with orbits and revolving celestial bodies.
   - **Intuitive Navigation**: 1-click style switching with live preview on LED rings, dedicated `SET TIME` option, exit via QUERY button.
   - **Interactive Sci-Fi Player `[11] COSMIC FX`**: 10 sound effects including whistles, chirps, staccato chatter, and panicked screams of droid R2-D2. Oracle languages are clean real dialects (EN default, AUTO, DE, RU).
5. **Robustify queries under unstable Wi-Fi.** Uplink is currently best-effort: add acknowledgment/recovery for incoming audio chunks, out-of-order handling, and packet loss metrics. Validate actual Wi-Fi drop/reconnect and physical device cancellation.
6. **Local Settings / UI.** Persistent storage for language, model, volume, effect; consistent reset/cancellation and full-line readability.
7. **Parallel Pi 5 Track (Optional autonomous wearable node).** Pimoroni PIM699 (NVMe Base) migration to Lexar NM790 1 TB. Does not block staff utilization or field testing.
8. **Power and Mechanics.** Staff power isolated from Pi 5 battery pack; casing/mounting following sustained voice exchange milestone. No new purchase lists without a proven blocker.

## Before Next Hardware Milestone

- [ ] Verify full response without truncation or repeats on 1.0.17 + v2 server.
- [ ] Verify Standalone Offline Oracle on physical staff with server powered down.
- [ ] Verify automated fallback switching to smartphone hotspot (`WIFI_FALLBACK_SSID`).
- [ ] Verify cloud / remote deployment of `ai_brain_server.py` (via Docker / VPS).
- [x] Complete clean install of Bookworm Lite 64-bit on MicroSD and isolate NVMe read-only root cause per [037](experiments/037-rpi5-nvme-stand-alone-migration.md).
- [x] Install **Pimoroni PIM699 (NVMe Base)** adapter, confirm 3.3V/4A power stability with write stress test (393 MB/s with fsync, 0 errors) per [037](experiments/037-rpi5-nvme-stand-alone-migration.md).
- [x] Deploy and verify `ai_brain_server.py` server on autonomous Raspberry Pi 5 in local network (`celestial-brain.service` active on `192.168.178.51:5005`, OpenRouter DeepSeek V3 + Edge-TTS 7.3.1 verified).
- [ ] Flash Pico 2 W with built firmware `celestial_firmware.uf2` (pointing to `192.168.178.51`) and verify voice query cycle on physical staff.

Rewriting LED lighting code **for four rings is removed from active planning**, as the fourth ring has been permanently removed. Cosmetic improvements to the three rings remain in the backlog and do not block the Pi track.
