---
name: rp2350-audio-i2s
description: >-
  Provides guidelines to configure, maintain, and debug full-duplex 16 kHz I2S audio input (INMP441 MEMS microphone)
  and I2S audio output (MAX98357A Class-D amplifier, 64 KB ring buffer, 21 chiptunes, 14 SFX)
  on Raspberry Pi RP2350 using PIO state machines and DMA. Use this skill when the user asks to modify
  or debug staff microphone capture, speaker playback, chiptune synthesis, or audio ring buffers.
---

# RP2350 Full-Duplex I2S Audio & Synthesizer Runbook

## 1. Verified Hardware & Pin Allocation (`firmware/include/config.h`)
- **Audio Input — Yellow `INMP441` Omnidirectional I2S MEMS Microphone (Base of Staff)**:
  - `SCK` (Bit Clock, 512 kHz): `GP10` (`Pin 14`)
  - `WS` (Word Select / LRCLK, 16 kHz): `GP11` (`Pin 15`)
  - `SD` (Serial Data Out): `GP12` (`Pin 16`)
  - `VDD`: Strictly `3.3 V` (`Pin 36`). `L/R` tied to `GND` (Left channel).
- **Audio Output — `MAX98357A` I2S Class-D Mono Amplifier (`9 dB` gain) + `4 Ω 3 W` Speaker**:
  - `BCLK` (Bit Clock, 512 kHz): `GP16` (`Pin 21`)
  - `LRCLK` (Word Select, 16 kHz): `GP17` (`Pin 22`)
  - `DIN` (PCM Data In): `GP18` (`Pin 24`)
  - `VIN`: `5 V` (`VBUS Pin 40` rail with `1000 µF` bulk capacitor); logic inputs are 3.3 V tolerant.

## 2. Unified `16,000 Hz` Sample Rate & PIO / Ring-Buffer Architecture
1. **Unified `16 kHz` 16-bit Mono PCM Across Both Directions**:
   - Both microphone capture (`INMP441`) and speaker playback (`MAX98357A` TTS + procedural synth) run at **`16,000 Hz` (`16 kHz`)** 16-bit signed mono PCM (`little-endian`).
   - Do **not** share `BCLK/LRCLK` pins between mic and amplifier—keep `PIO0` (Mic RX + WS2812B LEDs) and `PIO1` (Speaker TX) independent.
2. **Uplink Capture (`STATE_LISTENING`, `GP9` Held)**:
   - `INMP441` outputs 24-bit data in a 32-bit I2S frame; Core 1 shifts/scales samples to 16-bit mono PCM with DC-offset removal and soft-clipping gain.
   - Streams `640-byte` (`320 samples` = `20 ms`) UDP v2 audio packets (`opcode 0x04`, FNV-1a magic `0x811C9DC5`, CRC-16-CCITT) to port `5005`.
   - Releasing `GP9` sends `PTT_RELEASE` (`0x05`) and transitions to `STATE_PROCESSING`.
3. **Downlink Playback (`STATE_SPEAKING`) & Lock-Free `64 KB` Ring Buffer**:
   - Host (`Android` or `Pi 5`) paces `1024-byte` (`512 samples` = `32 ms`) UDP PCM chunks back to the staff.
   - Core 0 writes incoming packets into a lock-free **`64 KB` circular ring buffer (`2.04 seconds` capacity)**; Core 1 drains the buffer via `PIO1 SM0` DMA so Wi-Fi jitter never causes clicks or underruns.
4. **Standalone Procedural Synthesizer & Phonetic VU Coupling (`firmware/src/audio.c`)**:
   - Synthesizes **21 8-bit chiptune melodies** and **14 cosmic/R2-D2/spell SFX** on Core 1 at `16 kHz`.
   - Real-time speaker amplitude feeds into `leds.c` (Phonetic VU Coupling) so spoken syllables modulate the 43 WS2812B LEDs during `STATE_SPEAKING`.
