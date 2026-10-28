# Experiment 058: Canonical Milestone Freeze v1.1.0-RELEASE (Halloween Edition)

- **Date:** 2026-10-28
- **Target:** Raspberry Pi Pico 2 W (RP2350) + Google Pixel 9 Pro XL (Android 15) + Raspberry Pi 5 Wearable SBC
- **Firmware Version:** `1.1.0-RELEASE (HALLOWEEN-ORACLE)`
- **Companion App Version:** `1.1.0` (`com.celestial.companion`)
- **Status:** **PASS / FROZEN**

---

## 1. Executive Summary

This milestone formalizes the complete, stable, production-ready release of **CELESTIAL ENGINE Mk.V** across all hardware and software components. P0 through P10 are fully realized and verified. Physical integration (P11 & P12) is deferred to manual assembly by the user.

---

## 2. Milestone Verification & Capabilities Matrix

| Subsystem | Capabilities | Status |
|---|---|---|
| **Core Electronics (P0-P7)** | RP2350 Cortex-M33, SH1106 1.3" I2C OLED, Bourns PEC11R rotary encoder, momentary QUERY button, 43 WS2812B concentric LEDs (7→12→24), INMP441 circular DMA microphone, MAX98357A 3W I2S Class-D amplifier. | **VERIFIED** |
| **Pico Jukebox & Soundboard** | 24 chiptunes & studio master tracks (Mario, Tetris, Doom, Zelda, Star Wars, Ghostbusters, Morrowind, Spooky Skeletons, Daft Punk Da Funk & Robot Rock, eBay, Harry Potter, Pokémon, Sailor Moon, Axel F, Castlevania, Mortal Kombat, Megalovania, Coffin Dance, Shrek, Godfather, Pirates of the Caribbean, Addams Family). Single track, Play All, and Cycle All modes on OLED and in-app. | **VERIFIED** |
| **Thematic LED Runes** | 21 continuous animations, including matching thematic LED modes for all iconic packs (Daft Punk Visor, eBay Strobe, Hogwarts 4 Houses, Concentric Pokéball, Sailor Moon Spiral, Spooky Bones, Imperial Red, Matrix Rain, Doom Fire). | **VERIFIED** |
| **Voice & AI Inquiry (P8-P9)** | Hold QUERY button, stream 16 kHz 16-bit PCM over Wi-Fi Hotspot (`10.169.248.64` -> `10.169.248.193:5005`), process via Gemma 4 / OpenRouter, synthesize with Google TTS, resample multi-format 32-bit Float PCM to 16 kHz mono, and stream back via UDP into MAX98357A speaker with 45s response gate. | **VERIFIED** |
| **Arcane Grimoire & Spell Forge** | Library of elemental and esoteric spells with zero-latency (<20ms) hardware dispatch, customizable Spell Forge, Latin incantation TTS vocalization with in-app toggle badges. | **VERIFIED** |
| **Multimodal Vision** | "Eye of the Oracle" camera viewfinder in companion app, providing real-time prophetic vision interpretations of the physical world via Gemma 4 Multimodal. | **VERIFIED** |
| **Multilingual Engine** | Default language English (`[EN]`), with runtime switching to Deutsch (`[DE]`) and Русский (`[RU]`). Onboard canonical voice phrases in flash, aligned LLM system prompts, and native TTS voices. | **VERIFIED** |
| **Wearable Pi 5 SBC (P10)** | Pimoroni PIM699 + Lexar NM790 1 TB Gen 3 NVMe SSD, `celestial-brain.service` systemd daemon, offline Whisper + Ollama Llama 3.2 1B / Gemma 2B + Piper C++ TTS. | **VERIFIED** |

---

## 3. Key Bug Fixes in Canonical Freeze

1. **Track Alignment**: Fixed +3 index shift by restoring canonical 0..20 chiptunes and appending master ADPCM tracks at 21, 22, 23.
2. **Audio Streaming Response Gate**: Extended Pico timeout from 15s to 45s so long LLM queries do not abort playback.
3. **Android 15 Google TTS Decoding**: Added support for 32-bit Float PCM (`format 3`) and multi-sample alignment in `StaffTtsSynthesizer.kt`.
4. **Android 15 UDP Routing**: Eliminated `sendto failed: EPERM` by routing unicast packets through the primary listening socket without explicit local interface binding.
5. **Default Language**: Synchronized firmware (`LANG_EN`) and companion app (`StaffLanguage.ENGLISH`) to boot into English by default.
