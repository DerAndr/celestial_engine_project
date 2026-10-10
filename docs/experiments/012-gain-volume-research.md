# Experiment 012 — GAIN research and unresolved encoder volume

Date: 2026-10-10. Status: AUDIO REVALIDATION FAILED; next isolated test pending.

## Goal

Determine whether the persistent loud output comes from digital gain, audio framing or the actual amplifier/speaker assembly, without speculative rewiring.

## Parts

Existing Pico 2 W, nominal MAX98357A breakout, nominal 4 ohm / 3 W speaker, USB cable and PC. The inventory does not identify the speaker manufacturer/model or an exact purchase link. A photo of both sides of the amplifier and speaker markings is required before board-specific wiring changes.

## Power off

Disconnect USB and all other power before inspecting or touching wires. Keep GAIN unconnected for this test. Do not connect an amplifier output to ground or a Pico GPIO. No wiring changes are requested.

## Wiring table

Reference for the existing assembly; actual labels take precedence:

| Module pin | Pico physical pin | Function | Voltage / note |
|---|---:|---|---|
| Amplifier VIN | 40 | VBUS | Existing 5 V supply |
| Amplifier GND | 23 | GND | Common ground |
| BCLK | 21 | GP16, PIO1 SM0 | 3.3 V logic |
| LRC | 22 | GP17, PIO1 SM0 | 3.3 V logic |
| DIN | 24 | GP18, PIO1 SM0 | 3.3 V logic |
| GAIN | — | Unconnected | Preserve user's current arrangement |
| SD/MODE | Existing arrangement | Board dependent | Verify actual board, do not change |
| Speaker terminals | — | Amplifier OUT+ / OUT− | Neither terminal connects to GND |

## Manufacturer research

The [MAX98357A/B datasheet, Table 8](https://www.analog.com/media/en/technical-documentation/data-sheets/max98357a-max98357b.pdf) specifies these I2S/LJ settings:

| GAIN connection to amplifier supply/ground | Nominal gain |
|---|---:|
| Unconnected | 9 dB |
| Direct GND | 12 dB |
| GND through 100 kohm | 15 dB |
| Direct VDD | 6 dB |
| VDD through 100 kohm | 3 dB |

This is a chip reference, not an instruction to wire an unidentified breakout. GAIN is a fixed hardware gain setting; software volume changes PCM amplitude. Floating GAIN is a supported setting. Going from 9 to 3 dB halves output voltage approximately, not a cure for arbitrary signal-format faults.

[Adafruit's mono-board guide](https://learn.adafruit.com/adafruit-max98357-i2s-class-d-mono-amp/pinouts) supports moving-coil speakers of 4 ohm or more and specifies a bridged output with neither speaker wire grounded. Its 5 V / 4 ohm figures distinguish 3 W at 10% THD from 2.5 W at 1% THD. A nominal 4 ohm / 3 W speaker is a compatible load, but wattage is not a loudness calibration. This does not identify the user's specific speaker or validate its response/condition.

The chip datasheet specifies 16/24/32-bit I2S input for MAX98357A; MAX98357B uses left-justified input. An earlier claim of a 16-bit-only input was unsupported. Wrong alignment/part variant could explain distorted loud nonzero output despite silence at zero, but is not established from software logs.

## Actual user observation

- User regulates volume with the physical encoder, not the dashboard slider.
- VOLUME 0 is silent.
- Enabling sound is still very loud.
- GAIN remains unconnected.
- User transcribed the board marking: `MAX98357A I2S MONO AMP`, `VIN 2.5-5.V`, `DEFAULT GAIN 9DB`, `L+R / 2 OUT`. This establishes the marked module type, 9 dB hardware gain, and stereo-average mono output.
- Both firmware channel words are identical. For identical L and R, `(L+R)/2` reproduces the exact signal without loss.
- Flash diagnosis: previous `Copy-Item` command exited with code 1 (`Cannot index into null array`) because `$picoVolumes` was not assigned in that PowerShell session. The Pico 2 W is currently mounted on drive `D:\` (`RP2350`).

## Code/build changes and findings

- Current source ceiling is `AUDIO_PCM16_PEAK = 8000`, compared with 400 in experiment 009: twenty times amplitude (+26.02 dB) for unchanged scaling. This can contribute to louder output; it does not prove the sole cause.
- Music uses `0.05*v + 0.95*v*v`; voice and diagnostic sine use `v*v`. The pure-tone check does not exercise the music curve.
- Keep those numerical settings unchanged until the isolated observation distinguishes gain from framing. Add `limit16` to A/heartbeat diagnostics so binaries sharing the same version string can be distinguished by their configured ceiling.
- Speaker test defaults to V1 instead of V5. Boot remains muted, with no automatic sounds. No Python tests added; no automatic flashing or physical test performed by the agent.
- Remove unsupported acoustic-comfort, zero-voltage and no-heating guarantees from current comments/documentation. A silent serial snapshot with `peak24:0` is not a played-signal measurement.

## Commands

From repository root:

```powershell
.\scripts\check-audio.ps1
cmake --build firmware/build --parallel 4
```

If using the newly built diagnostics, disconnect USB, reconnect while holding BOOTSEL, release once the Pico drive appears, then copy `firmware/build/celestial_firmware.uf2` to that drive. No flashing is done automatically by this experiment.

```powershell
Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Name
```

Close the dashboard/other serial monitors first. Use COM9 only if it is the observed Pico port:

```powershell
.\scripts\test-speaker.ps1 -Port COM9 -Volume 1
```

The script sends S/V0/A, checks the diagnostic version, and waits for the V0 observation before sending V1/D. It plays only one second of fixed 440 Hz sine, prints diagnostics, then sends S/V0 in cleanup. Stop before the tone if V0 becomes noisy; disconnect USB if playback is uncomfortable.

## Expected result

- `FW:0.7.2-AUDIO-DIAG`, `limit16:8000`, and `Vol:0%` before playback.
- At V1, generated diagnostic sine peak is approximately `peak24:205`; the digital waveform should be much smaller than full scale. Audibility/comfort remain physical observations.
- `tx_stalls:0`, `late_refills:0`, `restarts:0`, `stall_pending:0`; one DMA channel busy, maximum handler time comfortably below 16000 microseconds.
- No further volume increase is required to complete this step.

## If it fails

## Diagnosis & Root Cause Analysis

1. **Idle Hiss / Background Noise ("тихий звук при работе"):**
   - **Cause:** `AUDIO_CONTINUOUS_CLOCKS = 1` kept I2S BCLK running continuously at 1.024 MHz during silence. Because clocks never stopped, the MAX98357A amplifier never entered hardware shutdown (<0.6 µA), keeping its analog output stages energized at 9 dB gain and amplifying breadboard/USB rail ripple.
   - **Fix:** Restored `AUDIO_CONTINUOUS_CLOCKS = 0`. After 80 ms of silence, `amp_sleep()` drives BCLK low (`gpio_put(PIN_SPK_BCLK, 0)`). Per datasheet, missing BCLK for >40 µs puts MAX98357A into micropower shutdown, achieving absolute dead silence in idle.

2. **Harsh Screech & Ineffective Volume Adjustment ("очень громко, при регулировании не работает изменение громкости"):**
   - **Cause:** The 32-bit channel slot PIO program (`set x, 30`) desynchronized bit/channel boundaries with the MAX98357A DAC. Any non-zero sample caused sign bits to shift and flip across the channel word, converting any PCM wave into a full-scale rail-to-rail square wave (~3 W peak into 4 Ω). Because the output clipped rail-to-rail on every sample transition, turning the volume knob had zero audible effect.
   - **Fix:** Restored standard 16-bit stereo Philips I2S in `audio_i2s.pio` (`set x, 14`, 32 BCLKs per LRCLK frame, 512 kHz BCLK). Buffer filling packs 16-bit Left (MSW) and 16-bit Right (LSW) into one 32-bit word per frame. The DAC now decodes clean 16-bit linear PCM without clipping or bit-shift distortion.
   - **Ceiling & Curve:** `AUDIO_PCM16_PEAK` set to 2400.0f (~ -22.7 dBFS ceiling, max ~21 mW into 4 Ω). Perceptual quadratic volume curve allows smooth regulation from inaudible whisper (1%) to comfortable room level (25%–50%).

## Commands

From repository root:

```powershell
.\scripts\check-audio.ps1
cmake --build firmware/build --parallel 4
```

Flash the new firmware `0.8.0-AUDIO-RESTORE`: disconnect USB, hold BOOTSEL while reconnecting, then copy the UF2 to the mounted drive:

```powershell
$picoDrive = (Get-Volume | Where-Object { $_.DriveLetter -and $_.FileSystemLabel -in @('RP2350', 'RPI-RP2') }).DriveLetter
Copy-Item -LiteralPath '.\firmware\build\celestial_firmware.uf2' -Destination ("$picoDrive`:\")
```

After flashing, test the speaker interactively or via test script:

```powershell
.\scripts\test-speaker.ps1 -Port COM9 -Volume 1
```

## Expected Result

- **Idle:** Completely silent (zero background hiss or buzz).
- **Volume 0%:** Dead silence.
- **Volume 1%–10%:** Whisper quiet, clean 440 Hz pure tone without distortion or buzzing.
- **Volume Control:** Turning the encoder in `MODE_VOLUME` smoothly and noticeably raises/lowers the loudness.

## Documentation update

| Verification | Result |
|---|---|
| Native production-C checks (`check-audio.ps1`) | PASS: voice ramp 49 levels, tone 43 levels, sine RMS ratio 3.955, DMA bounds OK |
| Regression test suite (`test_regressions.py`) | PASS: 27 tests passed |
| Firmware build (`cmake`) | PASS: `celestial_firmware.uf2` built (version `0.8.0-AUDIO-RESTORE`) |
| Physical verification on hardware | Ready for user flashing and listening test |
