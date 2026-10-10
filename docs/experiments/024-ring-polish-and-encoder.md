# 024 — Fourth-ring polish and encoder step correction

Date: 2026-10-11. Firmware: **1.0.9-RING-POLISH**. Status: **USER-TESTED / BACKGROUND AND WHITE-FLASH ISSUES REPORTED — follow-up 025**.

The user subsequently reports persistent blue/violet backgrounds in PULSAR/VOICE/VORTEX, individual white flashes in RAINBOW, and good COSMIC SNAKE. Removing the programmed intensity wave did not resolve the white flashes. [025](025-main-build-and-ring-backgrounds.md) removes those backgrounds and prepares a separate pure-RGB diagnostic. Old 1.0.9 is retained exactly as `firmware/build/celestial_ring_experiment_1.0.9.uf2`; historical build/flash instructions below describe the old target configuration, not the current main artifact. No physical confirmation of encoder cadence or complete white-flash resolution is claimed.

## Goal
Remove the fourth ring's rainbow brightness surge, place its voice response before the largest ring, and make one encoder cycle produce one menu step.

## Parts
Existing assembled Pico 2 W, four rings, microphone, encoder and USB data cable. No new parts.

## Power off
No wiring change is required. Keep all power disconnected before touching or changing wiring. Connect the assembled Pico by USB only when ready to flash.

## Wiring table — retained

| Module pin | Pico physical pin | GPIO/function | Voltage/note |
|---|---|---|---|
| First-ring DIN | Existing pin 20 data path | GP15 / PIO0 SM1 | 3.3 V GPIO through existing data interface |
| Fourth-ring DIN | — | 24-ring DO | Retain 7→12→24→spare wire order |
| Fourth-ring DO | — | Unconnected | Last output free |
| Fourth-ring +5 V / GND | Existing rails | LED supply / common ground | Existing removable supply leads |
| Encoder A / B / SW | 9 / 10 / 11 | GP6 / GP7 / GP8 | Existing 3.3 V pull-ups; no pin change |
| INMP441 SCK / WS / SD | 14 / 15 / 16 | GP10 / GP11 / GP12 | Existing microphone interface |

## Report and code review

After delivery of 1.0.8 the user reports working effects with three issues: occasional bright flashes on the fourth ring in RAINBOW, fourth-ring microphone response appearing after the first rather than near the largest ring, and excessively sensitive rotary navigation. The Pico is disconnected for this update. The agent did not flash or capture a serial banner for that user-run effects observation.

1. **Rainbow:** 1.0.8 explicitly modulates the fourth-ring HSV value from 120 to 255 with a traveling wave. This is a demonstrated programmed brightness modulation, but does not prove that every reported flash is software-generated. Replace it with constant value 180, retaining the per-pixel hue gradient. With the unchanged 4% cap, each fourth-ring rainbow channel is at most 7/255 on the wire; there is no moving intensity crest or white highlight in this mode. Electrical/address interference from the provisional repair remains unverified.
2. **Voice order:** 1.0.8 uses the raw 0..100% level directly for the fourth-ring fill, so it starts at low voice levels independently of the established ring stages. Separate serial wiring order from visual fill order: **7 → 12 → 16 → 24 LEDs**. In the experimental target, the small ring covers 0..30%, the 12-ring 25..50%, the fourth 50..75%, and the largest ring 75..100%. The fourth has a quiet violet background and a cyan-to-gold fill. Its reserve frames stay at the end of the serial stream; no wire movement is necessary.
3. **Encoder:** the old driver emits a step after two quadrature transitions, producing two software steps per complete cycle. The repository's confirmed model is PEC11R-4225F-S0024: Bourns specifies 24 detents and 24 pulses/revolution for this ordering combination, so one complete quadrature cycle is the appropriate default per detent. See the [official Bourns PEC11R datasheet](https://www.bourns.com/docs/Product-Datasheets/pec11R.pdf), ordering table and quadrature diagram. Centralize a four-transition threshold in config.h. IRQ and polling now share one decoder; polling samples GPIO and updates state with interrupts disabled. Previously it sampled before entering the critical section and could use a stale snapshot after an intervening IRQ. Invalid two-channel jumps discard a partial cycle; valid contact reversals cancel naturally. No time-based throttle or acceleration is added, so intentional fast rotation still counts full cycles.

Encoder changes also apply when rebuilding the normal target. The preserved normal 1.0.5 UF2 remains unchanged; only the experimental target is built here. Existing DMA, 59 slots, 300 µs reset, 4% cap, RGB boot, silent V0 startup, audio gain and pin assignments remain.

## Commands — repository root

```powershell
.\scripts\check-audio.ps1 -RingExperiment
.\scripts\check-audio.ps1
cmake --build firmware/build --target celestial_ring_experiment --parallel 4
Copy-Item -LiteralPath .\firmware\build\celestial_ring_experiment.uf2 -Destination .\firmware\build\celestial_ring_polish_1.0.9.uf2
& 'C:/Users/AndrD/.pico-sdk/picotool/2.3.1/picotool/picotool.exe' load -v -x .\firmware\build\celestial_ring_polish_1.0.9.uf2 --ser 09457D708B309FED -f
```

The checks and build completed successfully. The final command has **not** been executed: Pico remains absent from the Windows USB device inventory, matching the user's disconnection. To flash manually, hold BOOTSEL while reconnecting the Pico by USB, release once its RP2350 drive appears, then copy **celestial_ring_polish_1.0.9.uf2** onto that drive.

UF2 SHA256: `CC47F8C1836153570AD6B0E7AB00D29B5077DBC8FA4C229589EACBCB34AC1C46`. Offline inspection: RP2350 ARM Secure, pico2_w, SDK 2.3.1, Release.

## Expected result

After the existing red→green→blue boot sequence, default RAINBOW has a moving color gradient on the fourth ring at steady, softer intensity. In VOICE REACTIVE the fourth should fill after the 12-ring and before the largest 24-ring. Low sounds should leave it as a quiet violet halo. One encoder detent should move one menu item; volume should change one point per detent. Actual encoder behavior needs confirmation on the received part.

Serial banner: **1.0.9-RING-POLISH**. The same four-ring DMA boot-complete message follows the color sequence.

## Verification and actual result

The existing native C checks were extended rather than adding another framework. Before the fix, the ring check failed all three targeted observations: fourth-ring rainbow peak above its softer bound, early voice activation, and a menu step emitted after half an encoder cycle. After the fix it passes. It sweeps more than a full brightness-wave period, verifies actual transmitted tail words, checks low/mid/high microphone levels, and requires zero steps for half-cycle/contact bounce plus exactly one for each full forward/reverse cycle. Existing GRB/DMA/reset/cap/boot and effect checks remain. The normal C audio/control check passes with 17 probes. No Python tests were added or run.

These checks validate software frames and logical encoder transitions, not physical light intensity, wiring noise or the damaged LED's unisolated output. The subsequent user observation confirms that white flashes persist and that the spare's nonzero backgrounds are visible; see the follow-up at the top. No hardware bug is claimed resolved.

Rollback artifacts are retained: passed hardware baseline `celestial_ring_dma_1.0.7.uf2` and user-tested effects `celestial_ring_effects_1.0.8.uf2`. Their hashes and the normal 1.0.5 hash still match the recorded values.

## If it fails

1. If the fourth still flashes in RAINBOW, report whether one pixel or the whole ring flashes and whether the flash is white or a brighter version of its current color. Do not raise brightness or resolder as part of this step.
2. If voice order differs, confirm VOICE REACTIVE is selected and music/SFX playback is stopped, because playback intentionally overrides that mode.
3. If one physical detent still skips items or needs two clicks, report the exact change per click; use that observation to revisit the actual encoder cadence. If existing rings become unstable or anything heats unusually, disconnect power and return to the passed 1.0.7 baseline.

## Documentation update

Record the user-tested 1.0.8 issues and pending 1.0.9 observation in PROJECT, README, inventory, pinout and both HTML entry points. Existing hardware addressing remains provisional and the 022 hardware pass remains preserved.
