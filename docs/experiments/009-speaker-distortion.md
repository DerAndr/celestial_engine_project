# Experiment 009 — Distorted / uncomfortable speaker output

Status: FAILED hardware regression reported; software correction built, physical retest pending.

Follow-up: the current working tree raised the ceiling from 400 to 8000 without changing the version string. The peak predictions below describe the earlier 400-ceiling build. The user now confirms V0 silence but very loud sound when enabled with the encoder; use [experiment 012](012-gain-volume-research.md), starting at V1, for current observations and expected peaks.

## Goal

Separate a proven PCM precision defect from an amplifier, clock, wiring or speaker fault using silence and one quiet fixed-frequency tone.

## Parts

Existing Pico 2 W, MAX98357A breakout, tested 4 ohm / 3 W speaker, USB data cable and PC. Keep the existing assembly; no new parts or wiring changes are required.

## Power off

Disconnect USB and every other supply before touching wires or speaker terminals. The user removed the GAIN wire; keep it unconnected for this test. Do not reconnect it based on a generic breakout diagram. Actual board markings and photos must resolve any uncertain connection before powering.

## Wiring table

Reference against the existing wiring, not an instruction to relocate parts:

| Module pin | Pico physical pin | GPIO / function | Voltage / note |
|---|---:|---|---|
| MAX98357A VIN | 40 | VBUS | Existing 5 V USB supply; not GPIO |
| MAX98357A GND | 23 | GND | Common ground |
| MAX98357A BCLK | 21 | GP16, PIO1 SM0 | 3.3 V logic; diagnostic BCLK 1.024 MHz |
| MAX98357A LRC | 22 | GP17, PIO1 SM0 | 3.3 V logic; LRCLK 16 kHz |
| MAX98357A DIN | 24 | GP18, PIO1 SM0 | 3.3 V logic; PCM24 in 32-bit channel slots |
| MAX98357A GAIN | — | Unconnected | Wire removed by user; no replacement connection |
| MAX98357A SD/MODE | Existing arrangement | Board-dependent | Do not change; actual board configuration must be verified if diagnostics fail |
| Speaker + / − | — | Amplifier OUT+ / OUT− | Both go only to speaker terminals; neither goes to GND or GPIO |

For the MAX98357A IC, an unconnected GAIN selects nominal 9 dB; direct GND selects 12 dB. A received breakout can add components, so the chip table does not establish its actual board wiring. [Manufacturer datasheet, Table 8](https://www.analog.com/media/en/technical-documentation/data-sheets/max98357a-max98357b.pdf).

## Actual user observation

- All audio, including chiptunes and short sounds, is badly distorted and uncomfortable/loud at VOLUME 10.
- Turning volume down seems to remove frequencies rather than noticeably lowering loudness.
- GAIN wire was removed because there was no space to connect it.
- Running firmware banner and actual board photographs have not yet been supplied. No component defect is proven.

## Code/build changes

Firmware `0.7.2-AUDIO-DIAG`:

1. Fix quantization: previously V10 turned a 241-value voice ramp into seven output levels; tones had only five levels in the checked window. Preserve fractions through mixing, round to signed PCM24 and transmit it left aligned in separate 32-bit I2S channel slots.
2. Keep 16 kHz frames and 32 ms buffers. PIO now emits 32 bits/channel; DMA transfers 1024 words for 512 frames. No GPIO changes.
3. Boot muted at V0 with automatic chime/voice disabled. Keep both clocks running with zero PCM during this investigation; clock-stop sleep is bypassed.
4. Fix the volume preview: it previously changed frequency with volume (`440 + volume * 3`). It now uses fixed 440 Hz; encoder steps are one percentage point.
5. `D` plays a one-second pure 440 Hz sine at the current digital volume. `S` cancels every source, including the chime. `A` prints generated peak, maximum IRQ handler duration, FIFO-stall observations, live DMA status and restart count.
6. Bound each DMA read address to its aligned 4096-byte buffer with a hardware ring. RP2350 reloads counts on chain triggers, so a late IRQ previously risked reading beyond a buffer. Skip refilling a channel already busy or within 16 ms of handoff, and count `late_refills`; retain a defensive restart if both channels are unexpectedly idle. Missing a deadline can still repeat stale or partly refilled audio, so this needs actual timing logs.

Current embedded voice assets remain 8-bit source audio. Preserving their resolution through volume scaling does not make them studio-quality recordings. A digital percentage does not certify acoustic comfort on the actual speaker.

## Commands

From repository root:

```powershell
.\scripts\check-audio.ps1
cmake --build firmware/build --parallel 4
```

The audio check compiles production C and existing C hardware fixtures with installed MSVC. No Python runner or new Python tests are involved. It checks PCM resolution, channel equality, exact mute, fixed sine gain ratio, frame count, muted boot, DMA ring configuration/alignment, active-buffer guard and defensive idle-pair recovery. Actual I2S waveforms and refill deadlines still require hardware validation.

Flash only the built diagnostic UF2: disconnect USB, hold BOOTSEL while reconnecting, release it once the Pico drive mounts, then run:

```powershell
$speakerVolumes = @(Get-Volume | Where-Object { $_.DriveLetter -and $_.FileSystemLabel -in @('RP2350', 'RPI-RP2') })
if ($speakerVolumes.Count -ne 1) { throw 'Expected exactly one Pico BOOTSEL drive; inspect drives before copying.' }
Copy-Item -LiteralPath '.\firmware\build\celestial_firmware.uf2' -Destination ('{0}:\' -f $speakerVolumes[0].DriveLetter)
Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Name
```

Close the dashboard and any serial monitor before opening the observed Pico port. If it is the previously recorded COM9:

```powershell
.\scripts\test-speaker.ps1 -Port COM9 -Volume 5
```

For a different observed Pico DeviceID, substitute that value. The script first sends stop/V0, shows serial output and requires the `FW:0.7.2-AUDIO-DIAG` response before continuing. Enter `YES` only if V0 has no loud buzz/crunch. It then plays one second at V5, prints diagnostics and returns to V0 in its cleanup. Do not increase volume if the result is distorted or uncomfortable. If V0 is noisy, stop this test and disconnect power; do not proceed to tone playback.

## Expected result

- Banner reports `0.7.2-AUDIO-DIAG`; startup is silent, with V0 and no automatic speech/chime.
- V0 produces exact zero PCM and no loud distorted output.
- At V5, a faint, smooth 440 Hz tone lasts about a second. It ends automatically; there is no voice or music mixed into it.
- `peak24` is approximately 256 for the V5 diagnostic sine (1 equivalent 16-bit unit), approximately 1024 at V10. These are generated PCM measurements, not microphone/acoustic measurements.
- Expected healthy stream: `tx_stalls:0`, `stall_pending:0`, `restarts:0`, `late_refills:0`, one DMA channel active. `refill_max_us` should stay comfortably below the 16000 microsecond refill budget. It measures IRQ execution time, not time spent waiting for the IRQ to run. `tx_stalls` counts sampled sticky-flag observations, not every stalled cycle.

## If it fails

1. If V0 is loudly noisy or V5 is uncomfortable, disconnect USB and stop. Do not compensate by raising volume or changing GAIN connections.
2. Save the version banner and both `[AUDIO]` diagnostic lines. Nonzero stalls/restarts/late_refills or a stopped DMA pair direct investigation toward streaming/clock failures.
3. With clean streaming diagnostics but distorted tone, power off and inspect actual amplifier labels, solder joints, common ground and the speaker's two isolated output connections. Supply sag and wrong part variant remain possibilities, not diagnoses.
4. Send a clear photo of the actual amplifier and speaker markings plus the V0/V5 observations. Confirm whether the same fault occurs before any tone is requested. Do not substitute an Amazon pinout for the received board.
5. Do not proceed to P5 or declare the speaker defective until this milestone is resolved.

## Documentation update

The previous P7 pass is retained as historical evidence; the current assembly fails revalidation. Inventory notes record GAIN disconnected and sound distortion without labelling the speaker/amp itself FAILED. Pinout and HTML show the diagnostic firmware and blocked integration.

| Check | Actual result | Status |
|---|---|---|
| Native C PCM / control / recovery checks | PASS; V10 voice 241/241 values, tone 208 values, V10/V5 RMS ratio 4.001 | SOFTWARE PASS |
| Pico 2 W SDK Release build | PASS | SOFTWARE PASS |
| Diagnostic firmware flashed / banner | Not performed by agent | PENDING |
| V0 silence | Awaiting user | PENDING |
| V5 440 Hz tone / comfort | Awaiting user | PENDING |
| DMA / FIFO / handler timing on Pico | Awaiting serial log | PENDING |
| Actual amplifier / speaker photo | Awaiting user if the test fails | PENDING |
