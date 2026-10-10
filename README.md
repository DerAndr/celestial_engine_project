# CELESTIAL ENGINE Mk.V

Start here:

1. Read [PROJECT.md](PROJECT.md) for current implementation, hardware and prototype sequence.
2. Coding agents must read [AGENTS.md](AGENTS.md) before making changes.
3. The [inventory](docs/hardware/inventory.md) tracks confirmed hardware state.
4. The [master pinout](docs/wiring/pinout.md) is the wiring source of truth.
5. The [interactive map](docs/interactive_map.html) presents milestones, pin assignments, inventory and the planned state flow. Open [map.html](map.html) from the repository root.

Current firmware is **0.7.2-AUDIO-DIAG**. The user reports distorted, overly loud audio at V10 with GAIN disconnected. The diagnostic build starts muted at V0, with no startup sounds; PCM precision and fixed-frequency volume preview are corrected. See the [audio review](docs/reviews/2026-10-10-audio-review.md) and [speaker test](docs/experiments/009-speaker-distortion.md).

**P7 audio revalidation is failing; P5 is blocked.** P0–P4, P6 and P7 retain historical passes; P8 integration remains partial. Resolve the audio fault before further hardware integration.

The [2026-10-10 code review](docs/reviews/2026-10-10-code-review.md) records software fixes and remaining limitations. The [regression experiment](docs/experiments/007-code-review-regression.md) contains the hardware test to run before further integration.

From the repository root:

```powershell
.\scripts\check-audio.ps1
cmake --build firmware/build --parallel 4
```

The audio check compiles production C with installed Visual Studio C Build Tools and runs through PowerShell, without Python. Firmware targets Pico 2 W / RP2350 with the official Pico SDK. Existing Python asset tools and older regression wrappers remain separate from this targeted audio check.

Latest software verification: **native C audio check passes**, Release build succeeds. The earlier 27-test result is historical. UF2 output is `firmware/build/celestial_firmware.uf2`. Current build/flash/serial instructions and V0/V5 observations are in experiment 009; software verification does not establish a physical audio pass.

Experiment logs: [P0 Pico](docs/experiments/000-pico-alive.md), [P1 OLED](docs/experiments/001-oled-sh1106.md), [P2 encoder](docs/experiments/002-encoder-pec11r.md), [P3 QUERY](docs/experiments/003-query-button.md), [P7 audio](docs/experiments/004-audio-max98357a.md), [P6 mic](docs/experiments/005-mic-inmp441.md), [P4 LEDs](docs/experiments/006-ws2812-first-ring.md).
