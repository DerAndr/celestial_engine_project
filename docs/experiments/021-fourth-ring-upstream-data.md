# 021 — Compare fourth-ring response to upstream data

Date: 2026-10-11. Status: **DEFERRED at user's request; not performed**.

User does not want to resolder and requests firmware review/update first. Do not perform this wiring change now. [Experiment 022](022-ring-dma-firmware.md) is the active checkpoint on the existing 7→12→24→spare chain.

## Goal
Determine whether the spare ring responds to the known-functioning 12-ring output while retaining the experimental firmware.

## Parts and confirmed observations
Existing Pico 2 W, 7/12/24 rings, spare ring and one signal jumper. No new part is needed. User confirms flashing celestial_ring_experiment.uf2; three original rings are stable, spare is dark after the 24-ring but responds to direct row-20 data. User confirms continuity for row-20→first DIN and 24-ring DO→spare DIN. A continuity beep does not certify an active, valid data output.

## Power off
Disconnect USB and every supply before changing data wiring. Disconnect the spare DIN from the 24-ring DO before connecting any alternate source. Never join two data outputs. Keep the existing spare power, common ground and provisional local jumper arrangement for this bounded comparison; its old faulty DOUT remains attached, so this test does not establish output isolation or a permanent repair.

## Wiring table

| Module pad | Pico physical pin | GPIO/function | Voltage/note |
|---|---|---|---|
| First-ring DIN | Existing pin 20 / existing data path | GP15 / PIO0 SM1 | Existing controller wiring; no new GPIO connection |
| Spare DIN | — | Temporarily from **12-ring DO** | Remove its connection to 24-ring DO first |
| 24-ring DIN | — | Existing 12-ring DO connection | Retain; same upstream output feeds two DIN inputs |
| 24-ring DO | — | Temporarily free | No connection to spare or alternate source |
| Spare DO | — | Free | Last branch output unconnected |
| Spare +5 V / GND | Existing rails | LED supply / common ground | Retain same supply; no power rewiring |

Temporary data tree: **7→12→24**, with a second branch **12→spare**. This is a diagnostic arrangement, not a new final architecture. Touch only the data lead to the spare. If this requires inaccessible soldering or the pads are not clearly marked, stop before applying power and request a close view of that connection.

## Code/build changes and commands
None. Keep **1.0.6-RING-EXPERIMENT**, 59 transmitted frames, 4% cap and silent V0 startup. No build/flash command is needed from repository root. After reconnection choose **LED EFFECTS→RAINBOW WAVE** using the encoder so the upstream frames also contain visible colors.

## Expected result
If the spare lights from 12-ring DO while remaining dark from 24-ring DO, the failure depends on the latter source/path or its remaining stream; inspect that path next rather than declaring the entire 24-ring dead. The spare may mirror part of the large ring because both receive the same upstream frames. Its one faulty position may remain dark. The first three should remain stable.

If the spare stays dark, the 24-ring output alone has not been established as the cause. Record the exact result before another change. Stop for the user's observation.

## If it fails
1. If the original rings become unstable or there is unusual heating, disconnect all power immediately.
2. With all power disconnected, remove the temporary branch and restore the previous arrangement or leave the spare's data and power disconnected.
3. Report whether the spare lights and whether the original rings stayed stable; do not raise brightness, flash another build or probe live contacts.

## Documentation update
Experiment 020 records the clarified continuity pass and retains the failed serial-chain milestone. Inventory and pinout distinguish the intended 7→12→24→spare chain from this proposed temporary branch. The upstream comparison has not been performed or passed.

Reference: [Worldsemi WS2812B datasheet](https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf), DIN/DOUT roles and 24-bit-per-pixel forwarding. Its pin diagram does not determine the orientation of the user's actual ring pads.
