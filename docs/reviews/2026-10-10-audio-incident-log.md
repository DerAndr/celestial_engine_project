# Audio Subsystem Incident Log: MAX98357A

**Summary: ERR-AUDIO-02/03 RESOLVED.** The user repaired a cold solder joint and confirmed audio playback and volume regulation working. The specific defective pin was not identified by the agent. Follow-up — a minor +1.94 dB level increase in `1.0.2-AUDIO-LEVEL`, [experiment 017](../experiments/017-audio-solder-repair-level.md). Historical OPEN notes below reflect prior diagnostic stages.

**Update:** Working `0.9.0-AUDIO-FIX` was built, flashed, and verified in flash. Addressed DMA stop order per RP2350-E5 errata, stale buffers at V0/wake, low-level PCM quantization, and differing volume curves. COM9 confirmed new version and V0/DMA stopped. See [014](../experiments/014-audio-application-fix.md). ERR-AUDIO-02/03 remained OPEN until physical validation of standard playback. The historical 0.8.0 version log is preserved below.

**Historical Status: ERR-AUDIO-02 and ERR-AUDIO-03 OPEN.** The user reported harsh screeching and lack of volume scaling from 1% to 100%. Prior claims of resolution were rescinded. Next step was [isolated experiment 013](../experiments/013-speaker-isolated-probe.md). Build/host test passes below represent software validations, not physical hardware confirmation.

**Date:** 2026-10-10  
**Firmware Version:** `0.8.0-AUDIO-RESTORE` (superseded `0.7.2-AUDIO-DIAG`)  
**Hardware:** Raspberry Pi Pico 2 W (RP2350), MAX98357A amplifier (9 dB gain, L+R/2 out), QUARKZMAN 4 Ohm 3W speaker, JST-PH2.0.

---

## 1. Documented Issues and Symptoms

| Error ID | User Symptom | Physical Manifestation |
| :--- | :--- | :--- |
| **ERR-AUDIO-01** | *"Quiet background hiss"* | Continuous high-frequency hiss and noise in speaker during idle state (when no audio is playing). |
| **ERR-AUDIO-02** | *"Sound playback was deafeningly loud"* | Piercing, screeching crackle/distortion instead of pure sine wave or music. Waveform (square wave/clipping) unmeasured. |
| **ERR-AUDIO-03** | *"Volume adjustment does not change volume at all"* | Rotating encoder in `[ VOLUME ]` menu from 1% to 100% did not alter loudness; sound remained overwhelmingly loud. |

---

## 2. Technical Root Cause Analysis

### ERR-AUDIO-01 (Idle Noise): Observations and Constraints
Previous implementation used continuous I2S clocks; `AUDIO_CONTINUOUS_CLOCKS 0` halts clocks via `amp_sleep()` after idle timeout. User confirmed silence at V0. Power ripple, output FET switching, and current draw were not measured; explaining noise solely by supply rail was unproven.

### Cause of ERR-AUDIO-02 and ERR-AUDIO-03
User confirmed symptoms, but oscillograms of DIN/BCLK/LRC and amp output were unavailable. Claims of sign-bit inversion, 5V swing, or 3W power were unverified by instruments. MAX98357A supports 16/24/32-bit I2S: 32-bit slots were not a demonstrated failure cause. LRCLK transitioning on the final bit of the prior channel conforms to Philips I2S standard delay and does not prove clock desynchronization.

A C test decodes actual pioasm instructions as an I2S receiver: signed 16-bit words are reconstructed accurately, and inverted shift directions are rejected. This validates the software model, not wiring or amp silicon. An isolated diagnostic build transmits low positive sample amplitudes; further tests depend on physical findings.

---

## 3. Codebase Changes

### 1. `firmware/src/audio_i2s.pio`
- Restored standard 16-bit stereo Philips I2S:
  ```pio
      set x, 14              side 0b01 ; BCLK=1, LRC=0 (delay before MSB)
  left_loop:
      out pins, 1            side 0b00 ; BCLK=0, LRC=0
      jmp x--, left_loop     side 0b01 ; BCLK=1, LRC=0
      out pins, 1            side 0b10 ; BCLK=0, LRC=1 (last bit of left, swap LRC)

      set x, 14              side 0b11 ; BCLK=1, LRC=1 (delay before MSB)
  right_loop:
      out pins, 1            side 0b10 ; BCLK=0, LRC=1
      jmp x--, right_loop    side 0b11 ; BCLK=1, LRC=1
      out pins, 1            side 0b00 ; BCLK=0, LRC=0 (last bit of right, swap LRC)
  ```
- OSR autopull configured for 32 bits (`sm_config_set_out_shift(&c, false, true, 32)`). Exactly one 32-bit word read from FIFO per stereo frame (16-bit Left + 16-bit Right).

### 2. `firmware/src/audio.c`
- PIO clock frequency: `16000 * 16 * 2 * 2 = 1,024,000 Hz`. BCLK frequency is 512 kHz (32 BCLK cycles per 16 kHz LRCLK period).
- Double DMA ring buffer: 1024 words (4096 bytes, `AUDIO_DMA_RING_BITS 12`), providing 64 ms continuous playback per buffer.
- In `fill_audio_buffer`: synth samples scaled to `int16_t` and packed into single word:
  ```c
  buf[i] = ((uint32_t)(uint16_t)s16 << 16) | (uint32_t)(uint16_t)s16;
  ```
- In `audio_update`: restored sleep transition `amp_sleep()` after >80 ms idle. On playback start, `amp_wake()` cleanly resets PIO state machine and aligns DMA to buffer start.

### 3. `firmware/src/config.h`
- `#define AUDIO_I2S_SLOT_BITS 16`
- `#define AUDIO_CONTINUOUS_CLOCKS 0`
- `#define AUDIO_PCM16_PEAK 2400.0f` (digital ceiling ~ -22.7 dBFS).
- `#define AUDIO_DEFAULT_VOLUME 10` (starting volume 10%).
- `#define AUDIO_DIAGNOSTIC_BOOT 0` (normal boot with chime and voice greeting).
- `#define FIRMWARE_VERSION "0.8.0-AUDIO-RESTORE"`

### 4. `tests/audio_signal_check.c` & `scripts/test-speaker.ps1`
- PCM generator test adapted for 16-bit quantization (validates ramp levels, sine wave, and RMS ratio V10/V5).
- In `test-speaker.ps1`, version validation updated to accept 0.8.x series.

---

## 4. Verification and Results

| Test | Command | Result | Notes |
| :--- | :--- | :--- | :--- |
| **C Audio Signal Check** | `.\scripts\check-audio.ps1` | **PASS** | 49 ramp levels, 43 sine levels, RMS ratio 3.955, DMA 4096-byte buffers OK |
| **Regression Test Suite** | `python tests/test_regressions.py` | **PASS** | Prior automated tests pass |
| **Firmware ELF/UF2 Build** | `cmake --build firmware/build --parallel 4` | **PASS** | Binary `celestial_firmware.uf2` built cleanly |
| **Physical Flashing** | `Copy-Item ... D:\` | **Completed by user** | 0.8.0-AUDIO-RESTORE written to Pico 2 W flash |

---

## 5. Physical Observations and Diagnostics
V0 is silent, but ERR-AUDIO-02/03 remained open prior to physical solder rework. In 16-bit diagnostic sine testing, V1 quantizes to zero: silence at V1 did not prove audio path health.

A separate `speaker_probe.uf2` was assembled: quiet boot, single positive PCM tone U for 256 ms spanning 0..16, and auto-stop. C checks of physical buffer and PIO instruction decoding passed. User resolved the issue by repairing a cold solder joint.
