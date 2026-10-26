# Experiment 053: Audio Clipping Distortion and Streaming Stutter Fix

## Date
2026-10-26

## Objective
Eliminate speaker distortion ("похрипывание") and audio stutter ("подтормаживание") during synthesized voice/speech streaming playback from Companion App on MAX98357A.

## Root Cause Analysis
1. **Severe Digital Clipping ("Похрипывание")**:
   - In `StaffTtsSynthesizer.kt`, the synthesized 16 kHz audio was normalized up to a peak of `28,000` (out of 32,767).
   - In `firmware/src/audio.c`, `stream_gain` was set to `1.60f`.
   - Resulting digital amplitude: $28,000 \times 1.60 = 44,800$, which drastically exceeded `AUDIO_PCM16_PEAK` (24,000.0f) by nearly +5.4 dB.
   - Every vowel peak clipped into flat square waves, creating harsh buzzing and distortion through the Class-D amplifier.

2. **Network Buffer Starvation & Underrun Chatter ("Подтормаживание")**:
   - In `StaffRepository.kt`, audio was sent in 240-sample (15 ms) chunks with `delay(14)`.
   - On Android Linux kernels, `delay(14)` in coroutines typically sleeps for 16–22 ms due to scheduler timer resolution.
   - The Pico consumed audio faster (15 ms per chunk) than the Android phone was physically transmitting (18–22 ms per chunk).
   - The initial small pre-buffer (0.375s) drained in under 2 seconds, leaving the Pico in permanent buffer starvation.
   - In `audio.c`, when the buffer ran dry, it immediately zeroed the output, then resumed on the next 15 ms packet without re-buffering. This caused rapid chattering between audio and silence (~60 times a second), producing stutter and loud acoustic pops.

## Applied Corrections
1. **Firmware (`firmware/src/audio.c`)**:
   - **Buffer Expansion**: Increased `AUDIO_STREAM_RING_SIZE` from 32,768 to 65,536 samples (131 KB = 4.096 seconds of audio).
   - **Gain Calibration**: Reduced `stream_gain` from `1.60f` to `0.85f`. Together with TTS normalization, peak digital amplitude is $22,000 \times 0.85 = 18,700 < 24,000$ (`AUDIO_PCM16_PEAK`), leaving ~2 dB of headroom and eliminating all digital clipping.
   - **Anti-Chatter Starvation Gate**: If the ring buffer runs completely dry before `audio_stream_eof`, streaming is paused (`audio_stream_active = false`), allowing the ring buffer to fill back up to 4,000 samples (0.25s) before DAC playback resumes.
   - Flashed cleanly to Pico 2 W via `picotool`.

2. **Companion App (`StaffRepository.kt` & `StaffTtsSynthesizer.kt`)**:
   - **Extended Pre-Buffer**: Increased initial fast burst to 80 chunks (19,200 samples = 1.20 seconds of audio delivered in ~160 ms).
   - **Drift-Free Pacing**: Replaced naive `delay(14)` with absolute timestamp-based scheduling (`targetTimeMs = steadyStartTime + chunkIndex * 15L`) to eliminate Android coroutine timer drift.
   - **Normalization Ceiling**: Calibrated TTS peak normalization to 22,000 in `StaffTtsSynthesizer.kt`.

## Phase 2: Resolving Remaining Micro-Stuttering ("Заикание")

### Root Cause Analysis (Phase 2)
1. **Overzealous DMA Refill Guard in `refill_completed_buffer()`**:
   - `refill_completed_buffer()` checked `transfer_count < AUDIO_BUFFER_WORDS / 2` (256 words = 16 ms).
   - If an IRQ was serviced when the peer DMA channel had transferred more than half its buffer (e.g. while the CPU was busy with OLED I2C or Wi-Fi packet processing for 16 ms), the function aborted without refilling the buffer (`late_refills++`).
   - When the peer channel finished, the un-refilled buffer replayed stale 32 ms audio from earlier, causing audible repeats/skips.
   - Since `fill_audio_buffer()` takes only ~0.2 ms (~30 words), a safe threshold is `< 8` words (0.5 ms), not 256 words.

2. **Abrupt Deactivation and Re-buffering Gap on Underrun**:
   - In `fill_audio_buffer()`, a momentary buffer dry state set `audio_stream_active = false`.
   - This immediately caused `audio_is_playing()` to return false.
   - After 80 ms of silence, `audio_update()` called `amp_sleep()`, violently aborting DMA channels, clearing PIO FIFOs, and disabling clocks.
   - When new UDP packets arrived, `audio_stream_push()` required 4,000 samples (250 ms) before waking the DAC back up.
   - This converted brief 5–10 ms Wi-Fi jitter into a jarring 250 ms gap with audible clicks/pops.

3. **Interrupt Preemption**:
   - `DMA_IRQ_0` ran at default priority, allowing Wi-Fi Cyw43 background processing and USB CDC to delay audio refills.

### Applied Corrections (Phase 2)
1. **Firmware (`firmware/src/audio.c`)**:
   - **DMA Refill Guard**: Reduced the skip threshold from `AUDIO_BUFFER_WORDS / 2` (256 words = 16 ms) to `8` words (0.5 ms), completely preventing dropped 32 ms buffers.
   - **Seamless Underrun Handling**: During momentary buffer dry periods, the stream remains active and simply outputs silence (zeroes) for the missing samples. Next incoming packets continue playback smoothly without sleeping the amp or re-buffering.
   - **Stream Pre-Buffer**: Adjusted initial start threshold to 1,600 samples (0.10s) for responsive start.
   - **Inactivity Watchdog**: Added a 1.5-second watchdog in `audio_update()` to cleanly finalize streams if a sender disconnects without EOF.
   - **DMA IRQ Priority**: Elevated `DMA_IRQ_0` priority to `0x40` so audio buffer refills are never starved by background communication.

## Verification
- Firmware compiled cleanly and flashed to Pico 2 W (`09457D708B309FED`).
- Live diagnostics confirmed: `late_refills: 0`, `restarts: 0`, `tx_stalls: 0`, `refill_max_us: 1158`.
- Device operating stably on Wi-Fi `10.169.248.64` linked to `PHONE BRAIN`.
