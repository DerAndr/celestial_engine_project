# CELESTIAL ENGINE Mk.V

Start here:

1. Read [PROJECT.md](PROJECT.md) for current implementation, hardware and prototype sequence.
2. Coding agents must read [AGENTS.md](AGENTS.md) before making changes.
3. The [inventory](docs/hardware/inventory.md) tracks confirmed hardware state.
4. The [master pinout](docs/wiring/pinout.md) is the wiring source of truth.
5. The [interactive map](docs/interactive_map.html) presents milestones, pin assignments, inventory and the planned state flow. Open [map.html](map.html) from the repository root.

**2026-10-11 checkpoint:** the user repaired the fourth ring with a jumper bypassing one failed LED; current physical chain is 7→12→24→16. Four-ring light response is confirmed, while white rainbow flashes and full four-ring effects remain unfinished. Next: finish an independently useful Pico staff, then build the Raspberry Pi brain separately and integrate them. [Prioritized tasks](docs/tasks.md) · [Saved session state](docs/checkpoints/2026-10-11.md). Tonight is documentation/commit/push only.

Archived three-ring audio firmware is **1.0.5-AUDIO-GAIN**. **ERR-AUDIO-02/03 RESOLVED by user solder repair**: sound and encoder volume work. Startup and encoder now share one 40 dB gain curve. Startup V10 rises by +3.64 dB from 1.0.4; V100 retains the 14400 digital ceiling. Live COM9 confirms startup peak 205 (previously 135), zero TX stalls and OLED ONLINE. See [experiment 018](docs/experiments/018-volume-consistent-gain.md). The working 16-bit I2S format, default V10 and startup sounds are preserved.

The user confirms one QUARKZMAN 4 ohm / 3 W speaker connected only to amplifier OUT+/OUT−; GAIN remains unconnected. Experiments 013–016 preserve the debugging history; no further isolated listening sequence is requested.

**P7 audio revalidated; audio blocker cleared.** P0–P4, P6 and P7 retain passes. All four rings respond; P5 effects and P8 standalone integration remain partial.

The [2026-10-10 code review](docs/reviews/2026-10-10-code-review.md) records software fixes and remaining limitations. The [regression experiment](docs/experiments/007-code-review-regression.md) contains the hardware test to run before further integration.

From the repository root:

```powershell
cmake --build firmware/build --target celestial_firmware --parallel 4
.\scripts\check-audio.ps1 -RingExperiment
.\scripts\check-audio.ps1 -RingDiagnostic
```

The checks compile actual production C with installed Visual Studio C Build Tools, without Python. `-RingExperiment` checks the current main four-ring profile; `-RingDiagnostic` checks the separate pure-RGB diagnostic. The default command still checks the legacy three-ring/audio profile. Firmware targets Pico 2 W / RP2350 with the official Pico SDK.

Current main binary: **firmware/build/celestial_firmware.uf2**, version **1.0.10-FOUR-RINGS**. The earlier 1.0.5 three-ring/audio UF2 is preserved exactly as **firmware/build/celestial_audio_1.0.5.uf2**.

2026-10-11: main and diagnostic builds pass, **not flashed while Pico is disconnected**. User reports good COSMIC SNAKE, blue/violet spare backgrounds in PULSAR/VOICE/VORTEX and individual white flashes in RAINBOW. Main sets inactive spare pixels to black in those three modes. White flashes remain unresolved: **firmware/build/celestial_ring_experiment.uf2** (1.0.10-RING-DIAGNOSTIC) holds pure red/green/blue on the spare, five seconds each in RAINBOW, to isolate the issue. Prior 1.0.9 is saved as **celestial_ring_experiment_1.0.9.uf2**, and the passed 1.0.7 hardware baseline remains. [Naming, effects, diagnostic observation and flashing](docs/experiments/025-main-build-and-ring-backgrounds.md).

Experiment logs: [P0 Pico](docs/experiments/000-pico-alive.md), [P1 OLED](docs/experiments/001-oled-sh1106.md), [P2 encoder](docs/experiments/002-encoder-pec11r.md), [P3 QUERY](docs/experiments/003-query-button.md), [P7 audio](docs/experiments/004-audio-max98357a.md), [P6 mic](docs/experiments/005-mic-inmp441.md), [P4 LEDs](docs/experiments/006-ws2812-first-ring.md).
