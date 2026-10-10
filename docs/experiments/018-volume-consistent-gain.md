# 018 — Consistent startup/encoder gain and louder low volume

Date: 2026-10-10. Normal firmware **1.0.5-AUDIO-GAIN**, flashed and verified.

## Goal
Make the requested level increase more apparent and give boot and encoder the same volume behavior.

## Parts
Existing Pico 2 W, MAX98357A and one QUARKZMAN 4-ohm / 3-W speaker.

## Power off
No wiring changes. Disconnect USB and all other supplies before touching wiring or soldering.

## Wiring table

| Amplifier pin | Pico physical pin | GPIO/function | Voltage/note |
|---|---:|---|---|
| VIN | 40 | VBUS | Existing USB 5 V |
| GND | 23 | GND | Existing common ground |
| BCLK | 21 | GP16 | 3.3 V logic |
| LRC | 22 | GP17 | 3.3 V logic |
| DIN | 24 | GP18 | 3.3 V logic |
| GAIN | — | Unconnected | Existing board default |
| SD/MODE | Existing connection | Board dependent | Unchanged |
| OUT+/OUT− | — | Both speaker wires | One speaker; neither wire to GND |

## Evidence and code/build changes
User says the recent increments were imperceptible, while the encoder works. Earlier digital peaks did rise 94→113→135; each of the last two increments was only +1.58 dB. This does not establish a new amplifier fault. ERR-AUDIO-02/03 remain resolved by the user's solder repair.

Review found inconsistent gain: boot V10 used `(10/100)^2 = 0.0100`, but the volume setter used a 48 dB logarithmic curve, giving V10 approximately 0.00658. Even the first clockwise step to V11 could decrease gain relative to boot.

One shared `calculate_volume_gain()` now serves boot and all volume controls. Configurable `AUDIO_VOLUME_RANGE_DB = 40` maps V1..V100 to −40..0 dB; V0 stays exact mute. Startup V10 is 0.01520 (−36.36 dB), **+3.64 dB** from the old boot level; setter V10 is **+7.27 dB** from the old setter level. V100 retains gain 1 and `AUDIO_PCM16_PEAK = 14400`. Tone and voice both receive this final gain. Default V10, startup sounds, 16-bit I2S, DMA and GPIO assignments are preserved.

## Commands
From repository root:

```powershell
.\scripts\check-audio.ps1
cmake --build firmware/build --target celestial_firmware --parallel 4
& 'C:/Users/AndrD/.pico-sdk/picotool/2.3.1/picotool/picotool.exe' load -v -x .\firmware\build\celestial_firmware.uf2 --ser 09457D708B309FED -f
```

## Expected result / recorded verification
Existing native C audio check now covers equality of startup/setter gain and a rising first clockwise step. Both checks failed before the fix and pass afterward. Existing 17 driver/control probes pass, V0 is exact silence, V1 produces nonzero PCM, and measured V10/V5 sine RMS ratio 1.262 matches the configured curve. No Python tests added.

RP2350 build passes; picotool verifies flash and boots the normal application. Live COM9 status confirms `FW:1.0.5-AUDIO-GAIN`, `limit16:14400`, `Vol:10%`, startup `peak:205` (previously 135), `tx_stalls:0`, `restarts:0`, `late_refills:0`, `stall_pending:0`, OLED ONLINE. This verifies digital output increase; the user's loudness preference awaits ordinary listening. No extra diagnostic playback was triggered.

## If it fails
If a level adjustment is still insufficient, use the known working encoder during normal playback and report the desired direction. If grinding returns, stop playback with V0; inspect soldering with power disconnected before changing I2S.

## Documentation update
PROJECT, README, inventory, pinout and HTML record the current version, shared gain curve and live confirmation. [Experiment 017](017-audio-solder-repair-level.md) retains the previous ceiling-adjustment history.
