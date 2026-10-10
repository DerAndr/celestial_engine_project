# 022 — Firmware-first fourth-ring diagnosis using PIO-paced DMA

Date: 2026-10-11. Firmware: **1.0.7-RING-DMA**. Status: **PASSED — user-confirmed four-ring light/data response at 4%; spare repair and address mapping remain provisional**.

## Goal
Check fourth-ring response to a complete 59-pixel frame on the current 7→12→24→spare wiring before any soldering change.

## Parts and confirmed observations
Existing Pico 2 W and four wired rings. No new part, soldering or jumper change. User confirms the previous experimental UF2 was flashed and both row-20→first DIN and 24-ring DO→spare DIN have continuity. The original three rings are stable; the spare responds to direct row-20 data but was dark after the 24-ring. User explicitly chooses firmware inspection/update before the proposed rewiring in 021. That rewiring was not performed.

The spare still has the provisional common-DI→faulty-DOUT jumper and the original faulty output attached, as recorded in 019. Firmware cannot isolate that output. Address consumption, electrical repair and valid 24-ring output remain unverified.

## Power off
Leave the current wiring intact. Disconnect USB and every supply before touching wiring or soldering. To repeat the visual boot sequence, power-cycle the already assembled device without moving signal wires. Do not inject another live data source into the connected 24-ring DO net.

## Wiring table — retained, no rewiring

| Module pad | Pico physical pin | GPIO/function | Voltage/note |
|---|---|---|---|
| First-ring DIN | Existing pin 20 / data path | GP15 / PIO0 SM1 | Existing controller connection; 3.3 V GPIO |
| 12-ring DIN | — | 7-ring DO | Existing serial connection |
| 24-ring DIN | — | 12-ring DO | Existing serial connection |
| Spare DIN | — | 24-ring DO | Existing soldered connection, continuity confirmed |
| Spare DO | — | Free | Last output unconnected |
| Spare +5 V / GND | Existing rails | LED supply / common ground | Same removable supply leads |

## Source review and focused changes
The previous build's arrays, renderers and transmit loop really contain 59 pixels, including nonzero tail data. No 43-pixel truncation was found in that build. GRB packing is left-aligned into 32-bit words and PIO autopull consumes 24 bits at 800 kbit/s. Existing PIO bit timings remain unchanged.

A software reliability risk exists in feeding the PIO FIFO word-by-word from CPU: a sufficiently long interrupt can drain it during a frame. The PIO then stalls with its data pin low, which can look like a reset/latch to LEDs. This mechanism is a plausible contributor, **not an established explanation of this particular dark ring**. The source audit and a host harness cannot prove the live waveform at 24-ring DO.

Experimental target changes only:

- Pack all 59 GRB words in a persistent frame buffer before starting DMA.
- Claim one unused DMA channel, use 32-bit transfers with incrementing read address, fixed PIO0 SM1 TX FIFO destination, its TX DREQ and high priority. Wait for DMA completion before reusing the buffer.
- Drain the FIFO, allow 35 µs for the remaining 30 µs pixel, then at least 300 µs of low reset time. The old experiment used 80 µs of reset time; longer reset is a conservative compatibility choice, not a proven defect in the old target.
- At each boot send uniform red, green and blue to every one of the 59 addresses, two seconds each. This makes the initial observation independent of selected animation, microphone level and the spare's unverified address mapping. Then resume the existing effects and uniform tail color.
- Keep 4% brightness cap, startup V0 and no automatic audio. GPIOs, PIO allocation, nominal frame count and ordinary firmware behavior are retained.

Boot logging identifies DMA frame count, reset time and each color phase. End-of-frame idle PIO stalls are expected; no misleading zero-underrun claim is logged.

## Commands — repository root

```powershell
cmake --build firmware/build --target celestial_ring_experiment --parallel 4
.\scripts\check-audio.ps1 -RingExperiment
Copy-Item -LiteralPath .\firmware\build\celestial_ring_experiment.uf2 -Destination .\firmware\build\celestial_ring_dma_1.0.7.uf2
& 'C:/Users/AndrD/.pico-sdk/picotool/2.3.1/picotool/picotool.exe' load -v -x .\firmware\build\celestial_ring_dma_1.0.7.uf2 --ser 09457D708B309FED -f
```

The last command was executed successfully on the session Pico, with flash verification **OK** and application reboot. No user BOOTSEL/copy step is required for the already updated board. UF2 SHA256: `236027A52E6B2FF9C7EC4FB02E3526207A001878D3F42879997AEF1DA4A32634`.

## Expected observation
On startup the working pixels of all four rings should show red→green→blue together, with each color held for two seconds. The known failed pixel may stay dark. Afterwards the first three resume familiar animations; the spare should show slowly changing uniform color. This uniform tail is intentional diagnostic behavior, not a missing-animation regression.

## User-confirmed physical result

User reports colors at startup on **all four rings, including the spare**. Afterwards the original three continue their rainbow animation, while all pixels of the fourth change color together. This matches the intended diagnostic tail behavior and confirms a functioning downstream light/data path on the existing 7→12→24→spare wiring with 1.0.7. No resoldering or upstream branch was required.

The visual/data milestone is **PASSED**. The combined DMA/reset update restored the observed response; this result does not separately identify DMA feeding versus reset timing as the old cause. It also does not establish the exact number of serial addresses or electrical isolation of the still-attached faulty DOUT. Preserve this 4% configuration as the working experimental baseline; richer fourth-ring animations are a separate change.

## Verification and actual runtime observation
One existing native C check was extended. It failed against the old CPU-fed implementation, then passed with the new driver: 59-word DMA transfer, correct destination and DREQ, 32-bit GRB packing, stable complete frame buffer, drain/reset wait, uniform boot RGB at every address, 4% cap and tail rendering in all eight modes. Hardware substitutes do not measure real electrical waveforms or emulate all bus/interrupt scheduling. No Python tests added or run.

The Pico 2 W / RP2350 SDK 2.3.1 Release build passes, as does the existing normal audio/control check with 17 probes. Offline UF2 inspection confirms rp2350-arm-s / pico2_w, and the ELF contains 1.0.7-RING-DMA.

Passive COM9 capture after flashing records `DMA boot RGB complete; normal effects with uniform tail resumed`, OLED ONLINE, IDLE, V0 and uninterrupted heartbeat ticks 5–16. The early banner/color-start lines were missed while opening the serial port; no claim that those lines were captured. No error line appeared in the captured interval. Live log is `firmware/build/ring-dma-live.log`. LEDs were not observed by the agent.

Prior experimental UF2 is preserved as `firmware/build/celestial_ring_experiment_1.0.6.uf2`. The normal 1.0.5 UF2 remains byte-for-byte unchanged, SHA256 `A365B9BADC53AE5ED731E0C4A1FCC82698DF8A239A995F5A16778B8969DE442F`.

## If it fails
1. If the spare remains dark, report whether it also stayed dark during the initial uniform RGB sequence; do not rewire or increase brightness yet.
2. If the existing rings become unstable or something heats unusually, disconnect all power and stop this milestone.
3. Use the exact physical observation to choose the next investigation. The previous DO-source comparison remains deferred; no claim that the 24-ring output is faulty has been established.

## Documentation update
PROJECT, README, inventory, pinout and HTML record the user-confirmed four-ring visual/data pass and the intentionally uniform fourth-ring animation. The spare's local repair and exact address mapping remain provisional; experiment 021 stays deferred and unperformed.

References: [official Raspberry Pi WS2812 PIO example](https://github.com/raspberrypi/pico-examples/blob/master/pio/ws2812/ws2812.pio) for low side-set during a stalled output instruction and FIFO configuration; [Worldsemi WS2812B datasheet](https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf) for GRB forwarding and reset behavior. Neither source certifies the user's particular ring revision or its repair.
