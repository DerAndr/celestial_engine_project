# 025 — Main firmware naming, black backgrounds and white-flash isolation

Date: 2026-10-11. Main firmware: **1.0.10-FOUR-RINGS**. Diagnostic: **1.0.10-RING-DIAGNOSTIC**. Status: **BUILT / NOT FLASHED — white-flash cause unresolved; physical observation pending**.

## Goal
Restore the standard main UF2 name, make inactive fourth-ring pixels black in three effects, and isolate the remaining white flashes without rewiring.

## Parts
The existing assembled Pico 2 W, four rings, controls and USB data cable. No new parts.

## Power off
No wiring change is part of this milestone. Disconnect USB and every supply before touching connections or soldering. Connect the assembled Pico to the computer only when ready to flash.

## Wiring table — unchanged

| Module pad | Pico physical pin | GPIO/function | Voltage/note |
|---|---|---|---|
| First-ring DIN | Existing pin 20 data path | GP15 / PIO0 SM1 | Pico GPIO is 3.3 V through existing interface |
| Fourth-ring DIN | — | 24-ring DO | Existing 7→12→24→spare serial wiring |
| Fourth-ring DO | — | Free | Last output unconnected |
| Fourth-ring +5 V / GND | Existing supply rails | LED supply / common ground | Same removable leads |
| Encoder A / B / SW | 9 / 10 / 11 | GP6 / GP7 / GP8 | Existing connections and 3.3 V pull-ups |

## User observation and source review

Following 1.0.9 delivery, the user reports a persistent blue/violet fourth-ring background in PULSAR PULSE, VOICE REACTIVE and GALAXY VORTEX. COSMIC SNAKE is good. RAINBOW WAVE still has occasional **individual pixels becoming white**, rather than a uniform increase in the intended hue. No agent flash/serial capture of this user-run observation is claimed.

The background has a direct software cause: the spare renderer assigns nonzero HSV values to inactive pixels (40 in pulsar/vortex and 35 in voice), leaving a visible channel after the 4% scaling. Change those values to **zero**. The pulsar now goes entirely black between its waves; the voice ring is black below its fill range and on unfilled pixels; the vortex shows only its head/tail against black. COSMIC SNAKE is unchanged, as are the first three rings' effects, voice order 7→12→16→24, and four-transition encoder threshold.

The previous constant-intensity rainbow change did **not** resolve the reported white flashes. Review of the primary rainbow frame shows a fixed HSV value 180, no white highlight, at least one zero channel in each packed tail word, and no channel above 7/255 at the 4% cap. The host check also verifies this over successive frames. This rules out an intentional white flash in that renderer; it does not prove the actual waveform, physical pixel addressing or electrical cause. The spare still has the provisional common-DIN→faulty-DOUT jumper with the old faulty output attached. The exact source of the white pixels remains unresolved.

## Code/build changes and artifact names

Separate the four-ring hardware profile (`CELESTIAL_FOUR_RINGS`) from the experimental behavior flag (`CELESTIAL_RING_EXPERIMENT`). Both CMake application targets now select the current 59-slot / DMA / 4% / 300 µs / silent V0 profile. The standard main output is again **celestial_firmware.uf2**, which also matches the existing dashboard build/flash path; no dashboard Python change is needed.

| Artifact | Purpose |
|---|---|
| `firmware/build/celestial_firmware.uf2` | Current main four-ring application, version 1.0.10-FOUR-RINGS |
| `firmware/build/celestial_ring_experiment.uf2` | Current diagnostic, version 1.0.10-RING-DIAGNOSTIC |
| `firmware/build/celestial_ring_experiment_1.0.9.uf2` | Exact saved copy of the previously delivered 1.0.9 experiment |
| `firmware/build/celestial_audio_1.0.5.uf2` | Exact preserved old three-ring/audio main UF2 before overwriting its standard name |
| `firmware/build/celestial_ring_dma_1.0.7.uf2` | Passed four-ring hardware baseline |

The experimental build changes **only RAINBOW output on the spare**: after the existing six-second all-ring RGB boot, hold pure red, green and blue on the spare for five seconds each, repeating. Use the same value 180 and 4% cap as the main rainbow. The first three rings keep their rainbow. Other selected effects, music overrides and the easter egg retain the main behavior. Its banner and startup log explicitly identify the diagnostic. Main rainbow colors and PIO timings are unchanged; do not treat filename promotion as a hardware pass.

## Commands — repository root

```powershell
cmake --build firmware/build --target celestial_firmware celestial_ring_experiment --parallel 4
.\scripts\check-audio.ps1 -RingExperiment
.\scripts\check-audio.ps1 -RingDiagnostic
.\scripts\check-audio.ps1
```

`-RingExperiment` remains a compatible command name and now checks the main four-ring profile. `-RingDiagnostic` checks the experimental RGB behavior. The default check retains the legacy three-ring/audio profile for regression coverage; it is not the current CMake main profile.

For the **white-flash observation**, flash the diagnostic, not the main application:

```powershell
& 'C:/Users/AndrD/.pico-sdk/picotool/2.3.1/picotool/picotool.exe' load -v -x .\firmware\build\celestial_ring_experiment.uf2 --ser 09457D708B309FED -f
```

For ordinary operation after the observation, use the same command with **celestial_firmware.uf2**. If using BOOTSEL manually, hold BOOTSEL while connecting USB, release when the RP2350 drive appears and copy the chosen UF2 onto it. Do not change signal wiring during this test.

## Expected observation — one milestone

With the diagnostic and RAINBOW selected, no music/SFX or easter egg active: after boot the fourth ring should hold a uniform **red → green → blue**, five seconds each. Watch for 30 seconds and report whether an individual pixel still turns white during a held color. A white pixel during these pure-color frames cannot be an intentional rainbow hue; it points to corruption beyond the requested pure-color calculation, whose precise cause still needs investigation. If only the main gradient has white flashes, the next comparison should isolate data-pattern dependence without changing several electrical settings at once.

In both new files, PULSAR should go fully dark between waves, unfilled VOICE pixels should be dark, and VORTEX pixels outside its head/tail should be dark. Snake behavior should remain as reported good.

## Verification and actual result

The existing native C check failed on all three inactive-background cases before the change and passes after zeroing them. It checks actual packed DMA words, dark pulsar/voice phases and a vortex with both active and black pixels. Existing DMA, GRB, reset, cap, boot, note/microphone and encoder checks remain. Main rainbow checks cover the channel bound and absence of all-three-channel white; diagnostic checks cover pure uniform color and timed changes. Both four-ring variants and the legacy C audio/control check pass. No Python tests were added or run. Host checks cannot establish physical data integrity or reproduce the damaged output circuitry.

Both official Pico SDK 2.3.1 Release builds pass for pico2_w / RP2350 ARM Secure. Offline inspection confirms both program identities; their ELFs contain the expected main and diagnostic version strings. The Pico was absent from Windows USB inventory during preparation, so **neither new firmware was flashed by the agent**. Physical result: **pending**.

| File | SHA256 |
|---|---|
| celestial_firmware.uf2 | `A00C99B837EF6919448DD5FD29D98621D1DAEF29EB195A8179C18474C50FD70D` |
| celestial_ring_experiment.uf2 | `B3086D07E9F5B989E6B8CACEDDC0FF8454CFD436B0A7A947A86F91A15DB0F5A1` |
| celestial_ring_experiment_1.0.9.uf2 | `CC47F8C1836153570AD6B0E7AB00D29B5077DBC8FA4C229589EACBCB34AC1C46` |
| celestial_audio_1.0.5.uf2 | `A365B9BADC53AE5ED731E0C4A1FCC82698DF8A239A995F5A16778B8969DE442F` |

Versioned copies of the new main and diagnostic also exist as `celestial_four_rings_1.0.10.uf2` and `celestial_ring_diagnostic_1.0.10.uf2`. The preserved 1.0.7 hash still matches the earlier recorded baseline.

## If it fails

1. Confirm the diagnostic banner is **1.0.10-RING-DIAGNOSTIC** and RAINBOW is selected, with music/SFX stopped. Intentional white heads in other effects are not part of this observation.
2. Report whether white pixels occur during a held red/green/blue color, or only while colors change. Keep brightness and wiring unchanged so the comparison remains meaningful.
3. If any ring becomes unstable or anything heats unusually, disconnect all power. The passed 1.0.7 rollback remains available; electrical isolation of the faulty output is not claimed repaired by firmware.

## Documentation update

PROJECT, README, inventory, pinout and HTML distinguish the main four-ring output, current diagnostic and archived firmware. Experiment 024 records the user's continued white flashes and visible backgrounds; it is not marked physically fixed. Stop here for the user's diagnostic observation before any rewiring or new timing experiment.
