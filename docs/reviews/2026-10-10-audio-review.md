# Audio review after physical regression — 2026-10-10

The user reports badly distorted, excessively loud audio at V10 on all sounds, with GAIN disconnected. This overrides the earlier recorded comfort/pass claim. Current hardware status: unresolved audio fault. Software target: `0.7.2-AUDIO-DIAG`.

## Findings and corrections

| Priority | Finding | Evidence / correction |
|---|---|---|
| P1 | Digital attenuation destroyed PCM resolution. Tone and voice were truncated to integers after being reduced to a handful of 16-bit units. | Actual production C reproduced V10 voice ramp 7/241 values and five tone values. Float mixing followed by one PCM24 rounding step now preserves 241/241 ramp values and 208 tone values in the same checked window. |
| P2 | The volume preview changed pitch with volume. | `main.c` used `440 + volume * 3`; now fixed 440 Hz. UI adjustment is 1 percentage point in both volume and jukebox modes. |
| P1 | A late IRQ could leave chained DMA reading beyond its audio buffer. RP2350 reloads the transfer count on each trigger, but does not reset the read pointer. | Each 4096-byte buffer is now aligned to a 4096-byte boundary and uses a hardware read ring; count/address rearming is unnecessary. Skip refill when that channel is already busy and report missed refill deadlines. Native C checks verify ring configuration/alignment and the active-buffer guard; physical deadlines remain unmeasured. |
| P2 | The S command did not cancel an active chime. | Stop-all now clears chime, track, tone and voice sources. Queued DMA samples still take up to about 64 ms to drain; disconnect power if playback is uncomfortable. |
| Diagnostic limit | Previous host checks verified control logic, not low-volume PCM quality or actual acoustics. | New native C signal check exercises the production generator, without a Python runner. Build success alone does not establish good sound. |
| Diagnostic limit | Clock-stop sleep can alter wake behavior and obscure fault isolation. | Diagnostic build keeps silent I2S frames running, starts muted and disables automatic startup sounds. This isolates clock gating; it is not a claim that clock gating caused the continuous distortion. |
| Diagnostic limit | IRQ-sampled FIFO flags can miss a permanently stopped stream. | A logs live FIFO flag and DMA state as well as sampled observations. Maximum handler time is not IRQ latency. |

Packing/timing review: signed 32-bit channel words contain PCM24 in bits 31..8; left and right receive identical words. PIO emits 32 BCLK cycles/channel, 64/frame; 128 PIO instructions/frame at 2.048 MHz produce 16 kHz frames and 1.024 MHz BCLK. Each DMA buffer contains 1024 words = 512 frames = 32 ms. Pin assignments remain unchanged. The compiled PIO program and driver calculations agree; physical signals have not been measured.

DMA semantics were verified against the installed SDK's RP2350 register definitions and the [RP2350 datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf). An earlier host model incorrectly assumed that a completed peer had an exhausted reload count; that stopped-pair explanation was discarded. The explicit-idle restart check is only a defensive recovery case. Read rings guarantee memory bounds; missing a refill deadline can still replay stale or partly refilled PCM. Refill is skipped if the peer has fewer than 512 words remaining (approximately 16 ms). `late_refills` flags this skip or a busy channel at refill entry/exit, but is not a complete timing proof. Actual maximum handler duration must stay comfortably below 16000 microseconds.

For the MAX98357A chip, GAIN unconnected selects nominal 9 dB, while GAIN tied directly to GND selects 12 dB. Removing the user's GAIN wire is not by itself evidence of a damaged amplifier or speaker. Actual breakout components/part markings still need verification if the test fails. [Manufacturer datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/max98357a-max98357b.pdf).

No new voice assets or external TTS requests were generated. Existing user audio/dashboard work was preserved. No Python test files were added in this pass.

## Validation and next observation

```powershell
.\scripts\check-audio.ps1
cmake --build firmware/build --parallel 4
```

Native C check passes: equal channels, exact V0 silence, fixed-frequency V10/V5 RMS ratio 4.001, correct 512-frame buffer count, full ramp resolution, stop-all, muted boot, read-ring configuration/alignment, active-buffer guard and defensive idle-pair recovery. It also executes 17 existing C control/driver probes. Pico 2 W / RP2350 SDK Release build passes. The native check models hardware calls; it does not certify DMA deadlines, speaker output or supply voltage on the real board.

Run [experiment 009](../experiments/009-speaker-distortion.md) and stop after the physical V0/V5 observations. No P5/full-array integration until audio revalidation passes. If clean PCM and uninterrupted clocks still produce loud distortion, investigate actual board identity, wiring, power and speaker mechanics from observations; do not keep changing DSP blindly.
