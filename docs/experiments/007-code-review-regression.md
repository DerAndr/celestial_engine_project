# Experiment 007 — Existing assembly regression after code review

Status: BLOCKED BY AUDIO FAILURE — SEE EXPERIMENT 009

The user reports distorted/uncomfortable audio at V10 and GAIN disconnected. The checklist below describes the earlier 0.7.1-P4 build. Current 0.7.2-AUDIO-DIAG boots muted without automatic chime/voice; first complete [experiment 009](009-speaker-distortion.md), then revise/resume this combined regression.

Date: 2026-10-10. Firmware: `0.7.1-P4`. This checks the existing recorded P0–P4/P6/P7 assembly before returning to P5; it does not authorize full-array integration.

## Goal

Verify that the bug fixes preserve the existing display, controls, seven-pixel ring, microphone and speaker milestones together.

## Parts

Existing Pico 2 W breadboard assembly with SH1106 OLED, PEC11R encoder, QUERY button, seven-pixel ring, INMP441, MAX98357A and its tested speaker; USB data cable and development PC. Use the already verified wiring and supporting components.

## Power off

Disconnect USB and any other supply before inspecting or changing wiring. This test requires no wiring changes. If the assembly has changed, stop and verify actual breakout markings and the changed connections before applying power. Keep staff USB power separate from wearable Pi 5 battery power.

## Wiring table

Reference only: compare with the recorded assembly, not breadboard row coordinates from old experiments. No 5 V connection goes to a Pico GPIO; every peripheral requires common ground.

| Module pin | Pico physical pin | GPIO / function | Voltage / note |
|---|---:|---|---|
| OLED VCC | 36 | 3V3 OUT | 3.3 V |
| OLED GND | 3 | GND | Existing common ground |
| OLED SDA / SCL | 4 / 5 | GP2 / GP3, I2C1 | 3.3 V logic |
| Encoder A / B | 9 / 10 | GP6 / GP7 | Internal pull-ups |
| Encoder common C | 8 | GND | Existing common ground |
| Encoder SW1 / SW2 | 11 / 13 | GP8 / GND | Active LOW |
| QUERY signal / return | 12 / existing GND | GP9 / GND | Normally open, internal pull-up |
| INMP441 VDD | 36 | 3V3 OUT | 3.3 V only |
| INMP441 GND / L/R | Existing GND / 13 | GND | Left-channel selection; preserve recorded ground wiring |
| INMP441 SCK / WS / SD | 14 / 15 / 16 | GP10 / GP11 / GP12, PIO0 SM0 | 3.3 V logic |
| LED data input path | 20 | GP15, PIO0 SM1 | Preserve previously tested seven-pixel path; shifter/full-array validation is separate |
| Seven-pixel ring 5V / GND | 40 / existing GND | VBUS / GND | Existing USB-powered core only; 18% firmware brightness cap |
| MAX98357A VIN / GND | 40 / 23 | VBUS / GND | 5 V amplifier supply |
| MAX98357A BCLK / LRC / DIN | 21 / 22 / 24 | GP16 / GP17 / GP18, PIO1 SM0 | 3.3 V logic |
| MAX98357A GAIN | — | Unconnected | Left unconnected (floating) for 9 dB default gain; no wire needed |
| MAX98357A SD_MODE | — | Unconnected | Default stereo average / shutdown managed over I2S BCLK |
| Speaker terminals | — | Amplifier + / − outputs | Both connect to speaker, neither to GPIO or GND |

The direct LED smoke test is historical evidence, not validation of every WS2812 input threshold. If rewiring the level shifter becomes necessary, verify the actual chip/board pinout first. Do not add the outer rings during this regression.

## Code/build changes

Earlier review changes: bounded OLED transfers/recovery, stable QUERY edges, IRQ encoder capture with a 4-transition threshold, reliable LED frames, capped brightness, mic metering snapshots, amplifier sleep timing and local mic-test UI. Its audio comfort/quality assumptions failed physical revalidation. Current embedded speech remains 8-bit source audio; the current diagnostic firmware changes PCM packing and the output PIO program. Use [experiment 009](009-speaker-distortion.md) for current code, commands and observations; the remaining checklist here is historical until revised.

## Commands

From repository root, using the already configured local Pico toolchain:

```powershell
python -m unittest discover -s tests -v
cmake --build firmware/build --parallel 4
```

Expected software outcome: 27 passing tests and a successful build. Host tests require installed Visual Studio C Build Tools, Python and Node.js; the firmware build uses the Pico SDK and ARM compiler.

For flashing, disconnect USB, hold BOOTSEL while reconnecting, then release it. Detect the actual mounted volume rather than assuming a drive letter:

```powershell
$picoVolumes = @(Get-Volume | Where-Object { $_.DriveLetter -and $_.FileSystemLabel -in @('RP2350', 'RPI-RP2') })
if ($picoVolumes.Count -ne 1) { throw 'Expected exactly one Pico BOOTSEL volume; inspect mounted drives before copying.' }
Copy-Item -LiteralPath '.\firmware\build\celestial_firmware.uf2' -Destination ('{0}:\' -f $picoVolumes[0].DriveLetter)
Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Name
```

If the observed Pico serial port is the previously recorded `COM9`:

```powershell
.\scripts\monitor.ps1 -Port COM9
```

If it differs, use its observed DeviceID with the same `-Port` argument. Do not use a port belonging to another device.

## Expected result

1. Serial shows version `0.7.1-P4`; display initializes and heartbeat continues. The complete startup chime precedes the online announcement.
2. Rotate the encoder slowly and quickly in both directions during OLED refreshes. Navigation remains consistent with the previously verified direction and detent behavior.
3. After at least a second of silence, select a mode or menu item that produces a short blip. The first blip is audible, as are subsequent blips.
4. Hold QUERY: LISTENING shows a live mic meter and hold time. Speaking/clapping changes the meter. Release: return to IDLE, with a serial notice that retained capture/upload is not implemented.
5. Press the encoder switch while holding QUERY. It does not start hidden music. After release, normal switch navigation resumes.
6. Exercise the existing LED animations. All seven pixels update, rainbow changes are smooth, and there is no flicker or reset. Do not increase the brightness cap.
7. Repeat a few holds and navigation actions while audio and LEDs are active. No freezes, repeated boot banners or unexpected music are observed. A boot with QUERY held is detected after debounce; release restores normal navigation.

Optional OLED recovery check only if a known connector can be handled safely: power off, disconnect the OLED, power on and confirm serial/controls continue; power off to reconnect, then reboot and confirm display operation. Do not hot-wire power rails to test timeout handling.

## If it fails

1. Stop this milestone; do not proceed to P5.
2. Save the exact symptom and serial log, including the version banner and whether a reset occurred.
3. For absent serial, verify the observed port, USB data cable and BOOTSEL copy result. For silent audio, record the selected volume and whether startup speech worked.
4. Disconnect all power before checking the existing module labels, supply voltages and common grounds. Do not alter GPIO assignments to chase a symptom.
5. Report the failing numbered check and actual wiring changes since the last pass. Debug that failure before rerunning the remaining checks.

## Documentation update

Software verification: 27 host tests pass; Pico 2 W Release build succeeds. Physical checks above have not been performed during the code review.

Record the user's observations here before changing this status:

| Check | Observation | Result |
|---|---|---|
| Startup display / serial / audio | Pending | PENDING |
| Encoder slow / fast / direction | Pending | PENDING |
| First blip after idle | Pending | PENDING |
| QUERY / mic / release | Pending | PENDING |
| Encoder switch during LISTENING | Pending | PENDING |
| Seven-pixel animations | Pending | PENDING |
| Combined operation / held boot | Pending | PENDING |

Update `docs/wiring/pinout.md` only for verified wiring changes and `docs/hardware/inventory.md` only for confirmed arrivals or individually tested parts. Existing historical passes remain separate from this pending regression.
