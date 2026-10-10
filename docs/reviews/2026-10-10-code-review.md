# Code review and debugging — 2026-10-10

Status: software fixes verified; physical regression pending. Firmware version: `0.7.1-P4`.

Follow-up: the user subsequently reported distorted/loud audio at V10 with GAIN disconnected. This report and its 27-test result are historical. The [audio review](2026-10-10-audio-review.md) documents the missed low-volume PCM defect and current diagnostic build; [experiment 009](../experiments/009-speaker-distortion.md) is the active hardware test.

## Scope and outcome

Reviewed the firmware application, all peripheral drivers and PIO programs, CMake configuration, scripts, voice conversion tooling, interactive map and engineering records against `PROJECT.md`. The existing uncommitted amplifier sleep work was retained and repaired. GPIO assignments and the hardware architecture were preserved. No firmware was flashed during this review.

The findings below are fixed in the working tree. P1 means a failure can block ordinary operation; P2 means a reproducible functional or reliability defect; P3 means tooling, portability or documentation correctness. Priorities describe the original defects, not unresolved findings.

## Fixed findings

| Priority | Location | Trigger and defect | Correction / evidence |
|---|---|---|---|
| P1 | `firmware/src/sh1106.c`, `main.c` | A disconnected or stalled OLED could block synchronous I2C indefinitely; frame failures were ignored and runtime recovery never activated. | Every transaction has a 10 ms bound; frame updates return failure and clear `oled_ready` so the existing recovery path can run. Timeout and initial frame-failure tests pass. |
| P2 | `firmware/src/query_button.c` | A brief release bounce ended a held query; a button held at startup could be missed. | Both edges require 30 ms stability, with initial raw state captured. Bounce and boot-held tests pass. |
| P2 | `firmware/src/encoder.c` | Polling missed complete quadrature cycles during OLED refreshes, causing lost navigation. | A/B edges are captured by GPIO IRQ with a single GPIO snapshot. Delta reads/resets are atomic; existing direction and threshold remain. A full cycle during a simulated display stall is retained. |
| P2 | `firmware/src/leds.c` | Nonblocking PIO FIFO writes dropped pixels when frames arrived rapidly. A reset delay could elapse before the previous frame finished transmitting. | Blocking FIFO writes preserve pixels; transmission drains before the final pixel and reset delay. Back-to-back frame test passes. |
| P2 | `firmware/src/leds.c`, `config.h` | The public brightness setter accepted 100% despite the documented low-power prototype cap. | Seven active pixels and an enforced 18% cap are centralized in configuration. This is a brightness bound, not a measured electrical current limiter. |
| P2 | `firmware/src/audio.c` | A short sound generated while the amplifier was awake could finish before `audio_update`, then inherit an expired sleep deadline and be muted before queued samples reached the speaker. | Every playback request refreshes the sleep deadline. The existing 80 ms drain allowance is retained. Short-blip regression passes. |
| P2 | `firmware/src/main.c` | The blocking 350 ms boot pause prevented chiptune sequencing from advancing, and the online voice replaced the unfinished chime. | Startup services the normal loop; the voice announcement is deferred until idle and playback has finished. Tests cover both initial deferral and eventual announcement. |
| P2 | `firmware/src/mic.c` | Negating `INT16_MIN` into a signed 16-bit value discarded full-scale negative peaks. Processing physical ring order introduced artificial filter transitions at the DMA wrap. | Magnitude calculation widens before saturation; samples are processed oldest first. Negative-peak and ring-position tests pass. |
| P2 | `firmware/src/mic.c` | DMA could overwrite the source while the metering window was read, mixing windows and corrupting filter history. | Copy to a snapshot; accept it only if the DMA cursor stays fixed and copying takes less than one ring period. Retry at most three times, otherwise skip the metric update before advancing the filter. Moving-cursor test passes. |
| P2 | `firmware/src/main.c` | The heartbeat used CYW43 after a failed initialization. | Wireless LED writes are guarded by successful initialization. Failure-path test passes. |
| P2 | `firmware/src/main.c` | Pressing the encoder switch while LISTENING could start a hidden jukebox. | Switch actions are restricted to IDLE. Listening-state regression passes. |
| P2 | `firmware/src/main.c` | Releasing QUERY displayed UPLOADING and reported completion although no query audio was retained or transmitted. | The current interaction is explicitly a local microphone test, returning to IDLE on release. False upload claims and the simulated delay were removed. Regression passes. |
| P2 | `firmware/src/sh1106.c` | A generic I2C scan selected any responding device, including an unrelated device ahead of the OLED. | Only supported OLED addresses `0x3c` and `0x3d` are considered. Unrelated-address test passes. |
| P2 | `docs/interactive_map.html` | The amplifier drawer instructed grounding GAIN while claiming the recorded default 9 dB setup; the verified experiment leaves GAIN/SD unconnected. | Drawer reconciled with experiment 004; actual board markings remain authoritative. No wiring changed. |
| P3 | `firmware/src/leds.c` | HSV sector interpolation overflowed an 8-bit remainder, producing discontinuous rainbow colors. | Remainder scales to 0–255 within each 60-degree sector. Hue regression passes. |
| P3 | `firmware/src/voice_data.c`, `tools/process_voices.py` | Negative voice identifiers could index before the table on implementations with signed enums. | Unsigned range check in the asset file and generator. Negative-ID test passes; this was a portability defect, not proof of an observed ARM failure. |
| P3 | `tools/process_voices.py` | Eight-bit stereo was not downmixed; unsupported sample widths generated misleading silent data; short clips did not fade both ends; importing the module performed asset generation. | Downmix supported PCM explicitly, reject unsupported formats, fade both endpoints and guard generation with `main`. Four conversion/import tests pass. Existing voice assets were not regenerated. |
| P3 | `docs/interactive_map.html` | A duplicate `code` property overwrote milestone identifiers with HTML in the drawer heading. Three controls relied on a browser-global `event`. | Drawer uses its milestone key; click handlers receive events explicitly. Four map JavaScript tests pass. |
| P3 | Pinout, inventory, experiments and map | Records disagreed about PIO ownership, completed milestones, arrivals, simulated uploads and current estimates. Seven pixels at 18% were incorrectly described as drawing at most 20–25 mA. | Reconciled to recorded observations and code. Under the project's 60 mA/pixel model, full white is approximately 75.6 mA for seven pixels, excluding quiescent current. Unknown outer-ring arrivals remain unknown. |

Additional hardening: audio state shared with DMA IRQs is explicitly volatile, and compound volume/stop/playback updates use interrupt protection. `-Wall -Wextra` now runs for the firmware target; two display-format buffers were enlarged to eliminate truncation warnings. Private Wi-Fi credentials and Python caches are ignored by Git. These changes passed compilation and inspection; host stubs do not prove arbitrary IRQ interleavings.

## Reviewed behavior that was not a bug

- RP2350 DMA transfer count `0xffffffff` encodes endless transfers. It is not a finite count that exhausts after a few days. The microphone now calls the SDK's `dma_encode_endless_transfer_count()` to make that intent clear. See the [official SDK implementation](https://github.com/raspberrypi/pico-sdk/blob/master/src/rp2_common/hardware_dma/include/hardware/dma.h) and [RP2350 datasheet](https://pip-assets.raspberrypi.com/categories/1214-rp2350/documents/RP-008373-DS-2-rp2350-datasheet.pdf?disposition=inline).
- Current PIO resources are mic PIO0 SM0, LEDs PIO0 SM1 and speaker PIO1 SM0. Installed SDK 2.3.1's CYW43 allocator searches from the highest PIO instance. The earlier documentation's LED PIO2 SM0 assignment was incorrect; changing the driver to match it would risk wireless ownership. Drivers were not moved.
- The I2S PIO clock calculations match their instruction sequences. The reviewed microphone alignment accounts for its sampled sign bit. No demonstrated blocker justified changing the physically verified PIO programs.

## Remaining project gaps

These are staged work, not features implemented by this bug-fix pass:

1. **P5 is the earliest incomplete prototype.** Only seven LEDs are driven. The 59-pixel logical ring abstraction, full-array power budget/current control and verified shifter integration are absent. Arrival of the other rings is not established by the current inventory. A brightness percentage alone does not validate USB or board power capacity.
2. **P8 integration is partial.** `staff_state.h` defines states and `main.c` orchestrates local behavior, but LED animations are not consistently driven by one authoritative staff state. Most transport states remain unused.
3. **P9 wireless transport is absent.** Current firmware initializes wireless hardware but does not connect to a brain service or implement a versioned audio protocol. A heartbeat LED is not evidence of Wi-Fi connectivity.
4. **P10 query capture and brain pipeline are absent.** A 256-sample window supports metering, not recording a held query. There is no retained query, Pi STT/agent/TTS service or streamed response path in the repository.

## Verification

Run from repository root:

```powershell
python -m unittest discover -s tests -v
cmake --build firmware/build --parallel 4
git diff --check
```

The host suite executes the actual C application and drivers with hardware stubs using installed MSVC `/O2`, tests voice conversion with generated WAV fixtures, and runs the actual map script under Node.js. **27 tests pass.** The real Pico SDK build targets `pico2_w`, `rp2350-arm-s`, Release, using SDK 2.3.1 and ARM GCC 15.2. Output: `firmware/build/celestial_firmware.uf2`.

Hardware stubs do not verify electrical connections, real PIO waveforms, audible output, USB power margins, interrupt latency or DMA scheduling under wireless traffic. In particular, physical checks must establish that coherent microphone snapshots are accepted often enough, that audio buffers meet their 32 ms refill deadline, and that encoder direction/detent behavior is unchanged. The map tests verify JavaScript behavior, not visual layout in every browser.

The next action is [experiment 007](../experiments/007-code-review-regression.md) on the existing assembly. Record actual results and debug any failure before progressing to P5. Historical hardware passes were retained; the revised firmware has no new physical pass yet.
