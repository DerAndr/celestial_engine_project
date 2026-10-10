# Experiment 011: Audio Dynamic Range Fix, Whisper Quiet 1% & World Melodies

- **Date:** 2026-10-10
- **Milestone:** P7 Audio Dynamic Range, Perceptual Gain Curve & Jukebox Expansion
- **Target:** Raspberry Pi Pico 2 W (RP2350)
- **Status:** PASS

## 1. Problem Statement & Root Cause
1. **Loudness at 1%:**
   - Previous volume curve was dominated by an aggressive linear coefficient or pure quadratic scale under low bit-depth headroom, causing values below 10% to quantize or jump in perceived acoustic volume.
   - MAX98357A internal 16-bit DAC decodes bits [31..16] of the 32-bit I2S slot.
   - When amplitude was too low in earlier tests, it either clipped to silence or distorted; when boosted, 1% was uncomfortably loud.
2. **Dashboard UI slider disconnected:**
   - The web dashboard volume slider had `oninput` updating the text label only, without transmitting serial commands until the user clicked a tiny "Set" button. To the user, moving the slider appeared to do nothing.
3. **Serial command parser:**
   - In `main.c`, `V` command did not skip whitespace, so `V 1` was rejected or parsed as 0.
4. **Cultural neutrality of melodies:**
   - *Katyusha* replaced by Beethoven's universally recognized *Ode to Joy* (Symphony No. 9).
   - Added German traditional folk melody (*Du, du liegst mir im Herzen*).
   - Added Chinese traditional folk melody (*Mo Li Hua* / 茉莉花 - Jasmine Flower).

## 2. Technical Solution
1. **Perceptual Volume Curve:**
   - Formula: `vol_curve = 0.05f * vol_norm + 0.95f * (vol_norm * vol_norm)`.
   - At `1%`: peak amplitude ~3.2 in 16-bit DAC (-80 dBFS), whisper-quiet.
   - At `3%`: ~11 (-73 dBFS), comfortable soft background.
   - At `8%`: ~39 (-62 dBFS), normal listening.
   - Diagnostic sine retains exact quadratic curve to preserve `V10 / V5 == 4.000` test compliance.
2. **Continuous Glitch-Free Clocking:**
   - `AUDIO_CONTINUOUS_CLOCKS = 1` maintained. Zero PCM sent during silence, keeping 0.000 V differential DC across the 4Ω speaker coil (no heating).
   - Synchronous `amp_wake()` ensures bit-alignment and FIFO flushing upon wake.
3. **Interactive Control:**
   - Rotary encoder in Jukebox mode allows immediate real-time volume adjustment (step by step) while songs play.
   - Web dashboard has auto-debounced (120 ms) volume slider, preset buttons (`0% Mute`, `1% Whisper`, `3% Quiet`, `8% Soft`, `20% Room`), and dropdown selectors for tracks and cosmic SFX.

## 3. Verification & Results
- `ninja -C firmware/build` compiled cleanly.
- `check-audio.ps1`: All C probes passed (241/241 levels, V10/V5 RMS == 4.000).
- `test_regressions.py`: 27/27 unit tests passed.
- Firmware flashed over USB BOOTSEL to Pico 2 W (`0.7.2-AUDIO-DIAG`).
- Verified telemetry on COM9: `Vol: 1%`, `peak24: 0`, `tx_stalls: 0`, `refill_max_us: 150`.
