# 020 — Experimental firmware for the removable fourth ring

Date: 2026-10-11. Version **1.0.6-RING-EXPERIMENT**. Status: **FAILED — fourth ring dark in the serial chain; user confirms celestial_ring_experiment.uf2 is flashed**.

## Goal
Observe the repaired spare ring after the existing 24-LED ring while retaining the normal application controls.

## Parts / actual user observation
Existing Pico 2 W, 7/12/24 rings and the spare 16-position ring with one dark LED. User reports soldering the spare after the large ring and says all power is disconnected. Its power uses removable leads from the same existing +5 V rail, not a separate supply. No new hardware is required.

The common-DI→faulty DOUT jumper and old output attachment were confirmed in experiment 019; no later isolation has been reported. This is an explicitly requested temporary visual experiment, not certification of a permanent repair. Reduced brightness limits LED light output; it does not electrically isolate the old DOUT. Serial address consumption remains unverified.

## Power off
Keep USB and every other supply disconnected while changing wiring. For flashing, hold BOOTSEL while connecting USB so the older application does not run before the experimental UF2 is copied. Disconnect all power before removing the spare; disconnect its data as well as power.

## Wiring table

| Module pad | Pico physical pin | GPIO/function | Voltage/note |
|---|---|---|---|
| Existing first-ring DI | Existing pin 20 through existing data path | GP15 / PIO0 SM1 | Existing verified first-three-ring connection |
| Spare DI/DIN | — | DO of the 24-LED ring | Last ring; do not connect separately to A20 |
| Spare DO/DOUT | — | Unconnected | Last output stays free |
| Spare +5V/VCC | Existing +5 V rail | LED power | Removable lead from the same supply |
| Spare GND | Existing common GND rail | Ground | Removable lead; common ground |

Data order is **7→12→24→spare**. Removing the last ring leaves the original chain intact.

## Code/build changes
Separate CMake target `celestial_ring_experiment` uses the application sources with `CELESTIAL_RING_EXPERIMENT=1`; ordinary `celestial_firmware` remains its own target. Configuration sends **59 frames: 43 existing + 16 reserved tail frames**. Sixteen slots accommodate the currently unknown bypass addressing; this is not a claim that all sixteen positions are working or that there are 59 working LEDs.

The existing first-three-ring animations and OLED/encoder/microphone functionality remain. All sixteen appended slots receive the same slowly changing HSV color after each renderer, including modes that previously only wrote the first 43 pixels. Initial tail color is green. Uniform color avoids depending on a guessed fifteen-address mapping during this first observation. All LED brightness is capped at **4%**, including after a request for higher brightness. Startup audio is disabled and startup VOLUME is **0** in this target only; ordinary firmware retains its working audio configuration.

## Commands / flash
From repository root:

```powershell
cmake --build firmware/build --target celestial_ring_experiment --parallel 4
.\scripts\check-audio.ps1 -RingExperiment
```

Output: `firmware/build/celestial_ring_experiment.uf2`. With the board in BOOTSEL, copy this file to the mounted RP2350 disk. Alternatively, the following command loads and verifies that same artifact on the Pico previously identified in this session:

```powershell
& 'C:/Users/AndrD/.pico-sdk/picotool/2.3.1/picotool/picotool.exe' load -v -x .\firmware\build\celestial_ring_experiment.uf2 --ser 09457D708B309FED -f
```

Rollback: power off, detach the spare ring's three removable leads, then flash `firmware/build/celestial_firmware.uf2` by the same BOOTSEL procedure. Do not assume normal firmware will clear previously latched tail pixels while their power remains connected.

## Expected result
After flash, OLED and encoder work, startup is silent (V0), and the first three rings run their familiar animations at reduced brightness. The spare's working pixels initially appear green and slowly change color; the physically failed LED stays dark. Report simply whether the fourth ring responds and whether the first three remain stable. Stop here until that physical result is known.

Boot log should identify `1.0.6-RING-EXPERIMENT` and `EXPERIMENT: frames:59 (base:43 + tail:16) | brightness_cap:4%`. Log/frame counts do not certify the bypass's electrical behavior or address mapping.

## Recorded software verification
RP2350 / Pico 2 W SDK 2.3.1 build passes. Offline picotool inspection confirms UF2 family `rp2350-arm-s`, target RP2350 and board `pico2_w`. No device was powered, reset or flashed by the agent.

One native C check using actual firmware sources failed against the old 43-frame configuration, then passed after implementation: 59 frames, 4% brightness cap, a nonzero uniform tail in all eight animation modes and a changing transmitted tail color over time. The existing normal C audio/control check and its 17 probes also pass. No Python tests added or run.

The original normal UF2 was preserved byte-for-byte, SHA256 `A365B9BADC53AE5ED731E0C4A1FCC82698DF8A239A995F5A16778B8969DE442F`.

## If it fails
1. For erratic colors, resets or unusual heating, disconnect all power and detach the spare; retain the first-three-ring arrangement.
2. If the spare is completely dark, with power disconnected verify spare DI comes from **24-ring DO**, plus the removable supply/GND connections; confirm the flashed filename/version is the experimental one.
3. Keep 4% cap for this milestone. Resolve the hardware result before modifying effects, raising brightness or assigning an exact logical tail length.

## Actual observation and next checkpoint — 2026-10-11

User reports the first three rings remain stable, while the fourth lights only when signal is supplied live from breadboard row 20. It remains dark when connected after the large ring. This supports a functioning downstream light path under direct injection; it does not establish a good 24-ring DO, its connecting solder joints, or the firmware currently running on Pico. The test has **FAILED** at the serial-chain milestone.

Source review and experimental-target compile definitions confirm 59 transmitted frames, with nonzero appended color in all modes. Ordinary firmware still sends 43 frames. User subsequently confirms flashing celestial_ring_experiment.uf2, excluding the ordinary 43-frame firmware as the explanation. A read-only picotool query found the Pico running with USB serial, but could not inspect its flash without a reboot; no reboot or flash was performed.

With the experimental UF2 confirmed by the user, the next single physical checkpoint is continuity from the **24-ring DO PCB pad to the spare DIN PCB pad**, with USB and all supplies disconnected. Use the already familiar multimeter continuity setting, touching the actual pads rather than only the connecting wire. A missing beep points to the wire or its solder connections; a beep establishes continuity but does not prove that DO emits valid data. Report this result before choosing another change. Do not touch signal wires live or connect the direct controller source and 24-ring DO together.

## Documentation update / resolution
User clarifies the two continuity pairs: **row-20 signal→first-ring DIN** and **24-ring DO→fourth-ring DIN**, both beep. The intended connecting nets therefore have user-confirmed continuity; no DIN-to-DOUT short was reported. Continuity does not establish valid output data, solder-joint reliability under load, or the cause of the dark fourth ring. A photo is no longer required to resolve which pairs were probed.

Experimental UF2 flashing and the two signal-wire continuity checks are confirmed by the user. Fourth-ring serial operation, faulty output isolation and address mapping remain unresolved. The next checkpoint is [experiment 021](021-fourth-ring-upstream-data.md): temporarily feed the spare from the already functioning 12-ring DO, with 24-ring DO disconnected from the spare. No firmware change or new flash follows from this observation alone.

User subsequently declines resoldering and requests a firmware-first investigation. Experiment 021 is deferred, not performed. The current checkpoint is [022](022-ring-dma-firmware.md), with 1.0.7-RING-DMA built, flash-verified and running on the unchanged chain; physical fourth-ring observation pending. This supersedes the next-step recommendation above.
