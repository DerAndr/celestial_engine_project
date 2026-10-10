# 017 — Solder repair confirmed; small audio level increase

Date: 2026-10-10. Firmware **1.0.2-AUDIO-LEVEL**.

Build PASS; picotool flash verification OK; live COM9 confirms FW:1.0.2-AUDIO-LEVEL, limit16:10000, Vol:10%, peak:94, tx_stalls:0, DMA busy:0/0, OLED ONLINE. No new manual diagnostic sequence requested.

## Goal
Record the confirmed hardware repair and raise the working audio level slightly.

## Parts
Existing Pico 2 W, MAX98357A and one QUARKZMAN 4-ohm / 3-W speaker. No extra parts.

## Power off
No wiring change. Disconnect USB and all other supplies before touching wires or soldering.

## Wiring table
| Amplifier pin | Pico physical pin | GPIO/function | Voltage/note |
|---|---:|---|---|
| VIN | 40 | VBUS | Existing USB 5 V |
| GND | 23 | GND | Common ground; exact repaired joint not identified |
| BCLK | 21 | GP16 | 3.3 V logic |
| LRC | 22 | GP17 | 3.3 V logic |
| DIN | 24 | GP18 | 3.3 V logic |
| GAIN | — | Unconnected | Existing nominal 9 dB |
| SD/MODE | Existing connection | Board dependent | Unchanged |
| OUT+/OUT− | — | Both speaker wires | One speaker; neither wire to GND |

## Observation / incident status
User: "Я починил — это была ... пайка"; sound now works but is too quiet. **ERR-AUDIO-02 and ERR-AUDIO-03 RESOLVED by user hardware repair.** Poor soldering is the confirmed cause; the exact faulty joint was not identified to the agent. Earlier speculative framing/full-power root-cause claims are not established by software checks.

## Code/build changes
Single level setting: `AUDIO_PCM16_PEAK` **8000 → 10000**, amplitude ×1.25, **+1.94 dB**. This raises tone and voice equally at a fixed VOLUME. Existing 16-bit I2S, volume curve, default V10, startup sounds, GPIOs and 43 LEDs are preserved. This is a digital level change, not a certified acoustic SPL or power value.

## Commands
From repository root:
```powershell
cmake --build firmware/build --target celestial_firmware --parallel 4
& 'C:/Users/AndrD/.pico-sdk/picotool/2.3.1/picotool/picotool.exe' load -v -x .\firmware\build\celestial_firmware.uf2 --ser 09457D708B309FED -f
```
Device serial identifies the Pico in this session. No isolated listening-test script is required.

## Expected result
Same working sound and controls, with a small level increase at the same VOLUME. Live serial A should identify `1.0.2-AUDIO-LEVEL`, `limit16:10000`.

## If it fails
If loud grinding returns, use V0 or disconnect USB; inspect repaired solder joints before changing I2S framing. Touch wiring only with power disconnected.

## Documentation update
PROJECT, README, inventory, pinout and HTML close the solder-related incident. Further integration may resume; the new level preference remains subject to ordinary user listening.

## Second requested increase — 1.0.3-AUDIO-LEVEL
User requested another small overall increase. `AUDIO_PCM16_PEAK` 10000→12000, amplitude ×1.20, **+1.58 dB** from 1.0.2 (+3.52 dB total from the initial 8000 ceiling). Default V10, volume curve, I2S and wiring remain unchanged. Same root build/flash commands apply; this is the normal application binary, not a diagnostic probe.

Build PASS; picotool flash verification OK. Live COM9 confirms `FW:1.0.3-AUDIO-LEVEL`, `limit16:12000`. Current docs and HTML updated; four existing HTML scenarios pass. No added tests or separate manual listening sequence.

## Third requested increase — 1.0.4-AUDIO-LEVEL
User requested the same increase again. `AUDIO_PCM16_PEAK` 12000→14400, amplitude ×1.20, **+1.58 dB** from 1.0.3 (+5.11 dB total from 8000). Existing V10, volume curve, I2S and wiring are preserved.

Build PASS; picotool flash verification OK. Live COM9 confirms `FW:1.0.4-AUDIO-LEVEL`, `limit16:14400`. Current docs and HTML updated; four existing HTML scenarios pass. No added tests or separate manual listening sequence.

## Follow-up — 1.0.5-AUDIO-GAIN
User reports the small increments were imperceptible and confirms the encoder works. [Experiment 018](018-volume-consistent-gain.md) records the discovered boot/setter gain mismatch, its fix, and a louder low-volume curve with the same 14400 maximum ceiling.
