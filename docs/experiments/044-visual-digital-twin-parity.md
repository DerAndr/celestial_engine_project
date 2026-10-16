# Experiment 044: Real-time Visual Digital Twin Parity (43-LED Rings + SH1106 OLED HUD)

## Objective
Establish complete, mathematically synchronized visual parity between the physical Celestial Staff hardware (43-LED WS2812B headpiece + SH1106 128x64 OLED display) and the Android Companion App.

## Hardware Configuration
- **Processor**: Raspberry Pi Pico 2 W (RP2350 ARM Cortex-M33 @ 150 MHz).
- **Headpiece LEDs**: 43 WS2812B RGB pixels on GP15 PIO DMA.
  - Index 0: Center Core Stone.
  - Indices 1..6: Inner Ring (Radius 1.0).
  - Indices 7..18: Middle Ring (Radius 2.0).
  - Indices 19..42: Outer Ring (Radius 3.2).
- **Display**: SH1106 128x64 monochrome OLED on I2C1 (GP2 SDA, GP3 SCL).
- **Microphone**: INMP441 MEMS microphone on PIO I2S (GP10, GP11, GP12).
- **Audio Output**: MAX98357A I2S 3W Class-D amplifier on GP16, GP17, GP18.
- **Network**: CYW43 Wi-Fi running UDP binary protocol on port 5005.

## Implemented Enhancements
1. **Pico 2 W Telemetry Protocol (12-byte payload)**:
   - Extended `net_client_send_telemetry` to transmit:
     `{ mode, state, volume, live_rendered_anim, clock_style, lang, host_type, pref_host, ai_model, mic_level, cosmos_zoom, is_audio_playing }`.
   - Transmits `live_rendered_anim` (tracking dynamic animations during voice recording, data reception, speaking, cosmos zoom, and chronos dials) instead of static saved menu index.
   - High-rate 100 ms periodic telemetry broadcast while in `STATE_LISTENING`, `STATE_PROCESSING`, or `STATE_SPEAKING`.
2. **ConcentricLedRingsView Mathematical Parity**:
   - Replicated exact logarithmic whirlpool spiral and wave mathematics from `firmware/src/leds.c`.
   - Incorporated all 5 neural model themes (`LLM_THEMES`: Deep Thought cyan, Eddie solar gold, Grok cyberpunk magenta, Marvin steel-blue, Local Pi 5 cyber emerald matrix & ruby core).
   - Voice-reactive vortex suction with pulsing red recording core and voice acceleration during `STATE_LISTENING`.
   - Concentric inward data ripples or quantum matrix beams during `STATE_PROCESSING`.
   - Outward radiating sonic shockwaves during `STATE_SPEAKING`.
3. **SH1106 OLED Display Digital Mirror (`StaffOledScreenView.kt`)**:
   - 2:1 aspect ratio matching 128x64 pixels.
   - Phosphor cyan scanline styling.
   - Live VU audio meter reacting to `micLevelPercent`, neural radar sweep during thinking, and speech equalizer spectrum.
   - Live Cosmos zoom (1x Milky Way, 1k Solar, 1m Terra) and Chronos astronomical time.
4. **Android Dashboard Integration**:
   - Unified "STAFF DIGITAL TWIN" card on the Dashboard and Led Screen displaying the 43-LED rings and OLED HUD side-by-side in real time.

## Verification
- Firmware built cleanly with Ninja and flashed via `picotool load -f celestial_firmware.uf2 -x`.
- Android companion APK built cleanly in CI workflow and installed via `adb install build_artifacts/app-debug.apk` on Pixel 9 Pro XL.
- Telemetry payload unpacked safely with backwards-compatibility checks.
