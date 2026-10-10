# 023 — Four-ring cosmic effects

Date: 2026-10-11. Firmware: **1.0.8-RING-EFFECTS**. Status: **USER-TESTED / ISSUES REPORTED — follow-up 024**.

Follow-up: after UF2 delivery the user reports working effects, but intermittent fourth-ring rainbow flashes, an undesired voice-response order and overly sensitive encoder navigation. The agent's earlier flash attempt was unsuccessful; the user-run result below supersedes the earlier pending observation. [Experiment 024](024-ring-polish-and-encoder.md) builds 1.0.9 for these software corrections; physical confirmation remains pending.

## Goal
Give the working fourth ring per-pixel cosmic effects on the existing wiring and 4% brightness limit.

## Parts
The assembled Pico 2 W, four-ring chain and existing USB cable. No additional components.

## Power off
No wiring change is needed. Disconnect USB and all other supplies before touching connections or soldering. For flashing, connect the assembled Pico to the computer with its existing USB cable.

## Wiring table — unchanged

| Module pin | Pico physical pin | GPIO/function | Voltage/note |
|---|---|---|---|
| First-ring DIN | Existing pin 20 data path | GP15 / PIO0 SM1 | Pico GPIO is 3.3 V; retain existing data interface |
| 12-ring DIN | — | 7-ring DO | Existing serial connection |
| 24-ring DIN | — | 12-ring DO | Existing serial connection |
| Fourth-ring DIN | — | 24-ring DO | Working chain confirmed in 022 |
| Fourth-ring DO | — | Unconnected | Last ring |
| Fourth-ring +5 V / GND | Existing supply rails | LED supply / common GND | Removable leads from the same supply |

## Code/build changes

The user approved richer fourth-ring effects after confirming the visual/data pass of 1.0.7. Replace its uniform diagnostic tail with a renderer confined to the configured spare offset/count. Keep 59 transmitted slots, PIO-paced DMA, the 300 µs reset, 4% cap and initial red/green/blue sequence unchanged. Audio remains at silent startup V0; ordinary menu controls remain available.

| Selected mode | Fourth-ring behavior |
|---|---|
| RAINBOW WAVE (default) | Full hue gradient with a traveling brightness wave |
| GALAXY VORTEX | Counter-rotating cyan/violet comet with a white head and fading tail |
| PULSAR PULSE | A delayed outward double pulse with a violet background |
| VOICE REACTIVE | Quiet violet halo fills toward cyan/gold with microphone level |
| COSMIC BREATHE | Existing selected-color breathing across the complete array |
| COSMIC SNAKE | Existing moving snake across the complete array |
| SOLAR PLASMA | Existing warm random flame pattern across the complete array |
| STARRY NIGHT | Existing blue background and moving stars across the complete array |
| Chiptune / cosmic SFX playback | Opposed comets colored by the current note; blue halo during note rests |
| Easter egg | Existing fast rainbow across the complete array, without a tail override |

The first three rings retain their existing patterns. The fourth ring's physical mapping remains provisional because its faulty output is still attached to the common-DI bypass. Rendering 16 reserved slots does not establish 16 independent working pixels; the known damaged pixel can remain dark and some addresses may duplicate.

## Commands — repository root

```powershell
cmake --build firmware/build --target celestial_ring_experiment --parallel 4
.\scripts\check-audio.ps1 -RingExperiment
.\scripts\check-audio.ps1
Copy-Item -LiteralPath .\firmware\build\celestial_ring_experiment.uf2 -Destination .\firmware\build\celestial_ring_effects_1.0.8.uf2
& 'C:/Users/AndrD/.pico-sdk/picotool/2.3.1/picotool/picotool.exe' load -v -x .\firmware\build\celestial_ring_effects_1.0.8.uf2 --ser 09457D708B309FED -f
```

Build and native checks completed successfully. The final command was attempted, but **no accessible Pico with the session serial was found**. Windows also reported no Raspberry Pi USB device, no COM9 and no RP2350 BOOTSEL volume. The new firmware has **not** been flashed. Reconnect the Pico, then repeat the final command; alternatively copy the UF2 to its RP2350 drive in BOOTSEL mode.

New UF2 SHA256: `22823CEC524484CCE9DB763BA300F053DD3B74CC7AE7E2FB01798C6B740B7ACF`. Offline inspection confirms SDK 2.3.1, pico2_w, RP2350 ARM Secure, Release.

## Expected result

After flashing, boot shows red → green → blue for two seconds each. Then all four rings animate; the fourth should display several colors at once in the default RAINBOW WAVE and visibly move its brightness wave. Select VORTEX for its comet. VOICE REACTIVE uses microphone level; music/SFX comets use active notes, and hearing them requires raising VOLUME from 0. No brightness increase is part of this change.

Serial identifies `1.0.8-RING-EFFECTS` and logs `DMA boot RGB complete; four-ring effects active (per-pixel spare)`.

## Actual observation and verification

The preceding 1.0.7 four-ring response is physically confirmed in [022](022-ring-dma-firmware.md). The user subsequently reports working 1.0.8 effects with the three issues recorded above and disconnects the Pico for the next update. No agent serial capture or agent flash verification of that user-run observation is claimed.

The existing native C ring check first failed against 1.0.7's uniform tail, then passed against the new renderer. It checks different fourth-ring colors in the actual packed frame, temporal rainbow movement, microphone-level response at a fixed animation tick, note-frequency response, note rests and full-frame easter-egg rendering. Existing DMA, GRB packing, boot RGB, reset, cap and eight-mode checks remain. The normal C audio/control check also passes with 17 existing probes. No Python tests were added or run. Host substitutes cannot verify physical address mapping or judge the appearance of the effects.

Known-good rollback: `firmware/build/celestial_ring_dma_1.0.7.uf2`, SHA256 `236027A52E6B2FF9C7EC4FB02E3526207A001878D3F42879997AEF1DA4A32634`. Normal 1.0.5 UF2 remains unchanged, SHA256 `A365B9BADC53AE5ED731E0C4A1FCC82698DF8A239A995F5A16778B8969DE442F`.

## If it fails

1. If flashing cannot find the Pico, reconnect its USB data cable and check for its serial device or BOOTSEL volume.
2. If the fourth still shows uniform color after boot, confirm the serial banner says 1.0.8 and select RAINBOW WAVE. BREATHE is intentionally uniform.
3. If the fourth is dark in all three boot colors or the original rings become unstable, return to the preserved 1.0.7 UF2 before further changes. Disconnect power if anything heats unusually.

## Documentation update

PROJECT, README, inventory, pinout, interactive map and root map record the built effects firmware and pending flash/physical check. Experiment 022 remains the passed hardware baseline; no rewiring or additional power claim is made.
