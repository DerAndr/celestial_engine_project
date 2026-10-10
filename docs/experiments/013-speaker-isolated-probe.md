# Experiment 013 — Isolate ERR-AUDIO-02 / ERR-AUDIO-03

Date: 2026-10-10. Hardware status: FAILED, unresolved user report. Probe built; physical result pending.

**Historical procedure, superseded by [normal application fix 014](014-audio-application-fix.md).** User reports full silence. Status-only A on COM9 confirmed probe 01, min16:0 max16:16 and remaining:0. The user requested application fixes instead of further listening tests; no B/2 was sent. Application 0.9.0-AUDIO-FIX is now flashed/verified. The current separate probe source has identity 02 and 32-bit slots; instructions below describe the historical probe 01 only.

## Goal

Check whether a small, explicitly bounded PCM waveform becomes loud/distorted through the existing I2S amplifier chain, without application synthesis or peripheral interaction.

## Parts

Pico 2 W, one existing MAX98357A module, ONE QUARKZMAN nominal 4 ohm / 3 W speaker with two-pin JST-PH2.0, USB cable and PC. User supplied the product name; listing fields such as Stereo/Touch are not a measured electrical specification. User confirms one speaker connected only to amplifier +/−, neither wire to GND. No new parts required.

## Power off

Disconnect USB and all other supplies before inspecting/touching wires. Do not run the tone with both 4-ohm speakers wired in parallel to one amplifier. Keep GAIN unconnected and SD/MODE in its existing arrangement. Actual board labels take precedence; stop if uncertain.

## Wiring table

Reference only; this experiment does not relocate any connection:

| Module pin | Pico physical pin | Function | Voltage / note |
|---|---:|---|---|
| MAX98357A VIN | 40 | VBUS | Existing USB 5 V supply; not GPIO |
| MAX98357A GND | 23 | GND | Common ground |
| BCLK | 21 | GP16, PIO1 SM0 | 3.3 V logic, expected 512 kHz during tone |
| LRC | 22 | GP17, PIO1 SM0 | 3.3 V logic, expected 16 kHz during tone |
| DIN | 24 | GP18, PIO1 SM0 | 3.3 V logic, signed 16-bit stereo |
| GAIN | — | Unconnected | User's current setting; no jumper added |
| SD/MODE | Existing arrangement | Board dependent | No change |
| Speaker two wires | — | OUT+ / OUT− | One speaker only; neither wire to GND/GPIO |

## Actual user observation

- ERR-AUDIO-02 remains OPEN: very loud buzzing/grinding/distorted audio instead of a clean tone/music.
- ERR-AUDIO-03 remains OPEN: encoder adjustment from 1% through 100% does not change perceived output level.
- Previous observation: V0 silent. GAIN remains unconnected.
- User confirms one speaker only; both speaker wires go exclusively to amplifier +/− outputs. Two parallel speakers and a speaker-output connection to ground are excluded by this report; no new audio PASS follows from it.
- Module marking supplied: MAX98357A I2S MONO AMP, DEFAULT GAIN 9DB, L+R/2 OUT.
- Speaker identified by user: QUARKZMAN pack of two, 3 W / 4 ohm, JST-PH2.0. Manufacturer sensitivity/frequency-response data not supplied or established by the product listing.
- Passive COM9 read in this pass shows live heartbeats with `Vol:1%`, OLED ONLINE, IDLE. No playback command or flash was sent by the agent. These heartbeat lines do not identify the running firmware version or measure sound.

## Evidence and limits

[MAX98357A datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/max98357a-max98357b.pdf): floating GAIN is the nominal 9 dB setting and 16/24/32-bit I2S inputs are supported. Earlier claims that 32-bit slots inherently caused saturation were not demonstrated. [Manufacturer I2S timing guidance](https://ez.analog.com/audio/a/documents/do11655/should-the-lrclk-clock-be-tied-high-or-low-for-mono-over-i2s) requires the one-bit delay after LRCLK transition.

New native C verification decodes actual pioasm instructions using rising BCLK edges and the Philips delay. Positive and negative words round-trip correctly; a deliberately wrong shift direction is rejected. This proves the instruction model, not physical wiring, actual GPIO edges, module identity or acoustic behavior. It does not establish the cause of either error.

The current normal application is 0.8.0-AUDIO-RESTORE with a 2400 digital ceiling, 16-bit slots and automatic startup sounds. Software probe PASS does not establish that ERR-AUDIO-02/03 is fixed. Its V1 diagnostic sine rounds to zero because its unrounded peak is only 0.24 equivalent 16-bit units; a V1 "silence" result alone cannot validate that path. The old experiment 012's 8000-ceiling peak predictions are historical.

## Code/build changes

- Separate target `speaker_probe`; normal app audio logic, ring layout and pin assignments are preserved.
- Unique identity `CELESTIAL-SPEAKER-PROBE-01`; boot silent, audio output pins low; no startup sounds, encoder, OLED, microphone, LEDs or Wi-Fi initialization.
- Same current 16-bit PIO program, 16 kHz frames. Precomputed mono waveform duplicated into L/R and sent by one bounded DMA transfer; no DMA IRQ/refill/chaining or software volume curve.
- First command U: 256 ms of 500 Hz, amplitude 8 equivalent PCM16 units, offset upward so every sample is nonnegative (range 0..16). Soft edges and 2 ms zero tail. At end DMA drains and clocks stop. A 300 ms active-time guard also stops an incomplete transfer.
- Later commands B (signed -8..8) and 2 (positive 0..32) are available to investigate sign/alignment and a twofold amplitude change, but are NOT part of this first step. Do not proceed to them after a failed U observation.
- `test-speaker-probe.ps1` requires verification of one speaker isolated at OUT+/OUT−, the unique binary identity and a silent-startup observation before sending only U. It stops after collecting DONE/status.
- Native C checks cover the probe's real packed waveform ranges/channels/tail/stop and the compiled PIO wire model. No Python tests added/run.

## Commands

From repository root:

```powershell
cmake --build firmware/build --target speaker_probe --parallel 4
.\scripts\check-audio.ps1
```

Flash ONLY `firmware/build/speaker_probe.uf2`, not `celestial_firmware.uf2`. Disconnect USB, reconnect holding BOOTSEL, release when the Pico drive appears. Then:

```powershell
$speakerProbeVolumes = @(Get-Volume | Where-Object { $_.DriveLetter -and $_.FileSystemLabel -in @('RP2350', 'RPI-RP2') })
if ($speakerProbeVolumes.Count -ne 1) { throw 'Expected exactly one Pico BOOTSEL drive; inspect mounted drives before copying.' }
Copy-Item -LiteralPath '.\firmware\build\speaker_probe.uf2' -Destination ('{0}:\' -f $speakerProbeVolumes[0].DriveLetter)
Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Name
```

Close dashboard/serial monitors. With one speaker correctly isolated at OUT+/OUT− and an observed Pico port of COM9:

```powershell
.\scripts\test-speaker-probe.ps1 -Port COM9
```

Do not use dashboard's normal application flasher for this target. It points to the other UF2. The probe does not light the LED rings or update the OLED; that is expected during isolation.

## Expected result

- Status contains `FW:CELESTIAL-SPEAKER-PROBE-01`. Startup stays silent.
- U prints `POSITIVE`, `amplitude16:8`, `min16:0 max16:16`, then DONE and `active:0`.
- At most one 256 ms tone, followed by silence. With a correctly decoded stream, it should not be loud; it may be too faint to hear. No acoustic level is certified in advance.

## If it fails

1. Loud/distorted U or noisy startup: disconnect USB and stop. Do not send B/2 or raise amplitude.
2. Wrong identity or no DONE: check that `speaker_probe.uf2` was flashed, the actual COM port and the complete log. No root cause is assigned yet.
3. If digital range is 0..16 but U is loud/distorted, prioritize actual wiring/module/power/clock measurements. Obtain a clear photo showing Pico pins, amplifier labels and both speaker wires. Do not keep reducing application volume as a substitute for this check.
4. If U is silent/faint and clean, report that result and all PROBE lines first. The next milestone can compare signed samples and two amplitudes; it does not automatically establish either error fixed.

## Documentation update

PROJECT, README, inventory, incident report and HTML mark ERR-AUDIO-02/03 OPEN. QUARKZMAN identification is recorded without assuming Touch/Stereo listing fields describe an active speaker. Claims of 5 V clipped waveforms, measured output power, acoustic safety and a resolved root cause were withdrawn where unsupported.

| Check | Result |
|---|---|
| Isolated Pico 2 W / RP2350 build | PASS |
| Native C probe waveform and actual PIO decode | PASS; wrong-direction control rejected |
| Existing native C generator/control checks | PASS; 17 existing driver/control probes executed |
| Agent flashed probe / played sound | Not performed |
| Single speaker / isolated terminals | Confirmed by user: one speaker, only amplifier +/−, no GND connection |
| U observation | User reports full silence; status confirmed probe 01, range 0..16, remaining 0. No normal-audio PASS established |
