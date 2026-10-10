# Experiment 011: Audio Dynamic Range Fix, Whisper Quiet 1% & World Melodies

- **Date:** 2026-10-10
- **Milestone:** P7 Audio Dynamic Range, Perceptual Gain Curve & Jukebox Expansion
- **Target:** Raspberry Pi Pico 2 W (RP2350)
- **Status:** AUDIO REVALIDATION FAILED — USER REPORT SUPERSEDES EARLIER PASS

The user reports that volume still does not work normally and GAIN remains unconnected. Follow [experiment 012](012-gain-volume-research.md). Earlier software/serial results below do not prove acoustic comfort or speaker temperature.

## 1. Problem Statement & Root Cause
1. **Loudness at 1%:**
   - Previous volume curve was dominated by an aggressive linear coefficient or pure quadratic scale under low bit-depth headroom, causing values below 10% to quantize or jump in perceived acoustic volume.
   - The earlier claim that MAX98357A only decodes 16 bits was unsupported. Its datasheet specifies 16/24/32-bit I2S input; do not discard PCM precision or raise gain based on that claim. [Manufacturer datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/max98357a-max98357b.pdf).
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
   - With the current ceiling of 8000, unshaped musical peaks are 4.76 equivalent 16-bit units at 1%, 18.84 at 3%, and 80.64 at 8%; frequency rolloff and envelope further modify them. These values do not establish listening comfort.
   - Voice and diagnostic sine use a different, quadratic curve. The diagnostic sine at V1 peaks at 0.8 equivalent 16-bit units (`peak24` approximately 205); V5 is 20 (`peak24` approximately 5120).
   - Diagnostic V10/V5 amplitude ratio is four; this check does not validate the musical gain curve or the physical amplifier.
2. **Continuous Glitch-Free Clocking:**
   - `AUDIO_CONTINUOUS_CLOCKS = 1` sends zero PCM during silence. No differential voltage or temperature measurement is recorded, so exact zero volts/no heating cannot be claimed.
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
