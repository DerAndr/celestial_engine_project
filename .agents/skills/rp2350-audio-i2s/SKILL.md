---
name: rp2350-audio-i2s
description: >-
  Configure and manage I2S audio input (INMP441 MEMS microphone) and I2S audio output
  (MAX98357A Class-D amplifier) on Raspberry Pi RP2350 using PIO state machines and DMA.
  Use when designing audio pin assignments, capturing raw PCM, playing WAV/PCM audio, or debugging I2S clock issues.
---

# RP2350 I2S Audio Subsystem Runbook

## Hardware Modules
- **Input**: INMP441 omnidirectional MEMS microphone (I2S digital output, 24-bit data in 32-bit slot).
  - Pins: SCK (Serial Clock), WS (Word Select / LRCLK), SD (Serial Data out), L/R (Channel select: GND = Left, VDD = Right).
  - VDD: 3.3 V strictly.
- **Output**: MAX98357A mono I2S Class-D DAC/Amp (3.2 W into 4 ohm).
  - Pins: BCLK (Bit Clock), LRC (Left/Right Clock), DIN (Data In), GAIN, SD_MODE (Shutdown/Channel select).
  - Power: 5 V (VBUS) recommended for full 3 W output (or 3.3 V for low-power tests). Logic inputs accept 3.3 V logic directly.

## RP2350 Architectural Considerations
1. **Clocking Requirements**:
   - Audio input (STT mic capture): Standard 16 kHz sample rate, 16-bit or 32-bit slot mono.
   - Audio output (TTS speech playback): 22.05 kHz, 24 kHz, or 44.1/48 kHz sample rate.
   - Because mic recording and playback may require different sample rates or independent clock domains, **do not share BCLK/LRCK** between the mic and the amp unless running a synchronous pass-through pipeline.
2. **PIO Allocation**:
   - RP2350 has 3 PIO blocks (PIO0, PIO1, PIO2) with 4 state machines each (12 state machines total).
   - Use PIO0 SM0 for I2S Mic RX (INMP441).
   - Use PIO1 SM0 for I2S Amp TX (MAX98357A).
   - Reserve PIO2 SM0 for WS2812 LED timing.
3. **DMA**:
   - Double-buffered ping-pong DMA channels ensure zero dropouts during Wi-Fi transmission or SD card reads.

## Recommended Pin Allocation
- **INMP441**:
  - GP10: SCK (Physical Pin 14)
  - GP11: WS (Physical Pin 15)
  - GP12: SD (Physical Pin 16)
  - L/R: Ground (Select Left channel)
- **MAX98357A**:
  - GP16: BCLK (Physical Pin 21)
  - GP17: LRC (Physical Pin 22)
  - GP18: DIN (Physical Pin 24)
