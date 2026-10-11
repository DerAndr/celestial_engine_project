# CELESTIAL ENGINE Mk.V

Current user baseline: a stable three-ring Pico staff with live voice requests and generated spoken answers from the PC brain server. **The fourth 16-LED ring was discarded by the user** after continuing instability; active chain is 7→12→24 (43 LEDs, 18% cap).

Prepared update: **1.0.16-RESPONSE-FIX** + **brain protocol v2**. Fixes response-ring overwrite, duplicate START resets, late answers after cancellation/timeout, and replay of the old one-second preview. Response packets now have request IDs, ordering, ACK/retry and explicit completion. Server PCM no longer receives the microphone ×5 preamp. OLED response text uses the full screen with 56-pixel-high scrolling letters. New physical validation remains pending.

Start here: [PROJECT](PROJECT.md), [tasks](docs/tasks.md), [review](docs/reviews/2026-10-11-streaming-review.md), [experiment 036](docs/experiments/036-response-review-and-fullscreen-text.md), [protocol](docs/protocol.md), [Pi 5 next stage](docs/pi5-next-stage.md), [pinout](docs/wiring/pinout.md), [inventory](docs/hardware/inventory.md), [interactive map](docs/interactive_map.html). Agents must read [AGENTS](AGENTS.md).

From repository root on the existing Windows build environment:

```powershell
cmake --build firmware/build --target celestial_firmware --parallel 4
.\scripts\check-audio.ps1
python -m unittest discover -s tests -p test_brain_transport.py -v
python tools/ai_brain_server.py
```

Main UF2: **firmware/build/celestial_firmware.uf2**. Update firmware and restart the updated server together; v1 and v2 do not interoperate. The native C checks execute actual audio/control/display code. Three small Python checks cover the Python transport only, including a 14-second response with deliberately lost packet/ACK. The old four-ring diagnostic target is historical and is not the current setup.

One MAX98357A drives one QUARKZMAN 4 Ω / 3 W speaker; GAIN remains unconnected. Audio solder repair history: [017](docs/experiments/017-audio-solder-repair-level.md), [018](docs/experiments/018-volume-consistent-gain.md). Shared volume curve and GPIO stay unchanged; removing erroneous server preamplification may make speech sound quieter and cleaner at the same VOLUME.

Next: confirm the new pair on the PC, then run the same server independently on Pi 5 using existing Wi-Fi. The staff retains local controls, effects, music and phrases without the brain; QUERY requires a server. Hotspot, local AI and portable power follow separate milestones. Git contains source/docs; private Wi-Fi/API configuration and built UF2 are excluded.
