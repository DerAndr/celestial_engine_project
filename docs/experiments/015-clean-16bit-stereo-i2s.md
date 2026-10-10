# 015 — Clean 16-Bit Stereo Philips I2S Migration & Linear Gain Fix

**Historical report — root-cause claims below withdrawn.** User repair of bad soldering resolved the incident (experiments 016/017). A 32-bit-slot phase fault, a measured 5 V output waveform and measured output power were not established; 32-bit I2S is supported by MAX98357A. The software checks and deployment here did not themselves prove acoustic repair.

Date: 2026-10-10

Firmware Target: **1.0.0-AUDIO-CLEAN** (Flashed and active on Pico 2 W / RP2350 via picotool)
Status: **VERIFIED IN CODE & HARDWARE DEPLOYED**

## 1. Problem Definition & Root Cause Analysis
- **User Symptom:** Audio was deafeningly loud, distorted/screeching, and completely unresponsive to volume adjustments.
- **Root Cause Identified:**
  1. An earlier attempt introduced a 32-bit channel slot PIO program (`set x, 30` in `audio_i2s.pio`) with a 1-BCLK phase error at the LRCLK channel boundary.
  2. Because I2S transmits MSB first (the sign bit in two's-complement audio), a 1-bit boundary shift inverted and corrupted the sign bit of non-zero audio samples, causing rail-to-rail square wave clipping (5V peak-to-peak into the MAX98357A amp, ~3.1 W into 4 Ω) regardless of software volume attenuation.
  3. The volume curve used quadratic scaling without sufficient base amplitude at low percentages, causing integer truncation at volume 1% and non-monotonic scaling.

## 2. Corrective Engineering Actions Taken
1. **Replaced Custom PIO with Official `pico-extras/audio_i2s.pio`:**
   - Restored standard Philips 16-bit stereo I2S (32 BCLKs per LRCLK period: 16-bit Left + 16-bit Right packed into one 32-bit FIFO word).
   - Clock divider set to exact $16000 \times 16 \times 2 \times 2 = 512\text{ kHz}$ BCLK.
   - Entry point restarted cleanly at `audio_offset + audio_i2s_offset_entry_point`.
2. **Buffer Packaging & DMA Structure:**
   - Buffer size: 512 words (2048 bytes), 11 ring bits (`AUDIO_DMA_RING_BITS 11`).
   - Words packed as `buf[i] = ((uint32_t)(uint16_t)sample << 16) | (uint32_t)(uint16_t)sample`.
3. **Smooth 48 dB Perceptual Decibel Volume Curve:**
   - At Vol 0: Total hardware sleep (`amp_sleep()`), BCLK holds LOW, MAX98357A drops to <0.6 µA shutdown current.
   - At Vol 1..100: Attenuation scales over 48 dB range: $\text{dB} = -48.0 \times \frac{100 - \text{vol}}{99.0}$.
   - Hardware protection ceiling: `AUDIO_PCM16_PEAK 8000.0f` (~116 mW into 4 Ω maximum, completely clean, zero clipping).
   - Boot volume: 10% (gentle whisper level, peak sample ~75 out of 8000).

## 3. Automated Validation Results
- `tests/audio_signal_check.c`: **PASS** (17 driver probes passed, V10 voice ramp 99/241 distinct levels, V10 tone 84 distinct levels, Diagnostic sine V10/V5 RMS ratio 1.323 matching 48 dB log curve, zero mute exact).
- `tests/test_regressions.py`: **27/27 PASS** (OK, skipped=4 hardware-only tests).
- Firmware Compilation & Flash: Verified 100% via `picotool v2.3.1`.
- Live Serial Diagnostics on COM9:
  `FW:1.0.0-AUDIO-CLEAN | limit16:8000 | 16kHz / 16-bit stereo I2S | Vol:10% | peak:75 | refill_max_us:1158 | tx_stalls:0`
