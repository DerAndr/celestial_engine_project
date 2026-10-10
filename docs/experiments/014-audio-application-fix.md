# 014 — Working application audio fixes

Date: 2026-10-10. Firmware: **0.9.0-AUDIO-FIX**, built, flashed and flash-verified by the agent. ERR-AUDIO-02/03 remain OPEN until ordinary playback is observed. The user requested actual application fixes instead of further isolated listening tests.

## Goal
Fix demonstrated volume, PCM precision and DMA lifecycle defects in the normal application, preserving the hardware and controls.

## Parts
Existing Pico 2 W, MAX98357A and ONE QUARKZMAN nominal 4-ohm / 3-W speaker. User confirms both speaker wires go only to amplifier +/−. No extra parts.

## Power off
No wiring change is needed. Disconnect USB and other supplies before touching wires. GAIN stays unconnected; SD/MODE stays in its existing arrangement.

## Wiring table
| Module | Pico physical pin | GPIO/function | Voltage/note |
|---|---:|---|---|
| VIN | 40 | VBUS | Existing USB 5 V, never a GPIO supply |
| GND | 23 | GND | Common ground |
| BCLK | 21 | GP16, PIO1 SM0 | 3.3 V logic; 1.024 MHz during playback |
| LRC | 22 | GP17, PIO1 SM0 | 3.3 V logic; 16 kHz |
| DIN | 24 | GP18, PIO1 SM0 | Signed PCM24 in 32-bit slots |
| GAIN | — | Unconnected | Nominal 9 dB |
| SD/MODE | Existing connection | Board dependent | Unchanged |
| OUT+/OUT− | — | Both speaker wires | One speaker, neither wire to GND |

## Findings and code/build changes
- **DMA stop/wake defect:** old sleep stopped only BCLK, leaving DMA active/queued. Wake reset PIO before aborting DMA and reused old buffers. On RP2350, aborting chained DMA requires clearing EN on both channels first ([official SDK, RP2350-E5](https://www.raspberrypi.com/documentation/pico-sdk/hardware.html#dma_channel_abort)). New stop disables PIO/channel IRQs, clears both EN bits before abort, acknowledges pending IRQs, clears FIFO/buffers and holds all audio pins low. Wake enables DMA through non-triggering AL1_CTRL, starts only A to prime FIFO, then enables PIO on a reset frame boundary.
- **Mute defect:** V0 previously changed future synthesis only; already queued PCM remained. It now stops the stream and clears both buffers synchronously. Stop-all also does so. Sleeping IRQs cannot restart the stream.
- **Precision defect:** the previous 16-bit V1 sine rounded entirely to zero. Attenuated V10 voice produced only 49 distinct values in a 241-value ramp. New synthesis preserves fractional PCM16 units as signed PCM24, left aligned in standard 32-bit I2S slots; PIO, clock divider, FIFO words and DMA buffers change together. Negative samples are shifted only after unsigned conversion.
- **Inconsistent gain:** tones previously used frequency-dependent attenuation and a different volume curve from voice. One final master gain now applies to all sources, independent of pitch: V0 mute; V1..100 span −48..0 dB in equal steps. Per-source effects/envelopes remain.
- **Output cap/latency:** digital ceiling reduced 2400→800 equivalent PCM16 units (−9.54 dB). Buffers reduced from 64→16 ms each; queued audio at most 32 ms. These are digital properties, not a measured acoustic guarantee.
- **Startup:** normal UI/OLED/mic/43 LEDs remain; V0, no automatic chime/voice. No separate speaker-test script is required for ordinary use.

The separate probe target now has identity CELESTIAL-SPEAKER-PROBE-02 to distinguish its 32-bit format from the historical probe 01. It was not flashed or played in this application-fix pass.

## Commands
From repository root, already executed:
```powershell
cmake --build firmware/build --target celestial_firmware speaker_probe --parallel 4
.\scripts\check-audio.ps1
& 'C:/Users/AndrD/.pico-sdk/picotool/2.3.1/picotool/picotool.exe' load -v -x .\firmware\build\celestial_firmware.uf2 --ser 09457D708B309FED -f
```
The serial selector identifies the Pico observed in this session; use the actual device identity if hardware changes. Do not send serial B to a probe binary: B there means sound, not BOOTSEL. The command above uses picotool's USB reset interface.

## Expected result / observed status
Normal application is already running; no manual flash or diagnostic listening sequence is requested. V0 starts stopped; ordinary VOLUME changes should alter amplitude while keeping pitch.

Observed status-only A response on COM9:
```text
FW:0.9.0-AUDIO-FIX | limit16:800 | 16kHz / 32-bit slots / PCM24 | Vol:0% | peak24:0
DMA A/B busy:0/0 | restarts:0 | late_refills:0 | stall_pending:0
OLED: ONLINE | State: IDLE | Vol:0%
```
This confirms deployment and idle behavior, not the physical sound quality.

## If it fails
If ordinary playback still produces the loud grinding, stop playback at V0 or disconnect USB. Keep the incident OPEN; inspect actual I2S signals/module identity rather than claiming a further gain reduction proves repair. Obtain a clear board/wiring photo before any board-specific change. No escalating listening test is part of this step.

## Documentation update / verification
PROJECT, README, inventory, pinout and HTML point to this application build. Experiment 013 preserves the historical report: full silence; status from probe 01 showed range 0..16 and remaining 0. That observation did not prove normal playback repaired.

Before changes the native C check reproduced **V1 nonzero PCM FAIL** and **V0 clears queued PCM FAIL**. After changes both pass; voice ramp 241/241 values, tone 230 values, V10/V5 RMS ratio 1.322 matching the dB curve, DMA bounds/late-IRQ checks pass. Compiled PIO decode passes signed 32-bit channels and rejects the wrong-shift control. Existing 17 C driver/control probes pass. No Python tests added/run. Picotool reported flash verification OK; live version/status verified above. User acoustic confirmation pending.
