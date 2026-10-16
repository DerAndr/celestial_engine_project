## Summary of Changes
Provide a clear, high-level summary of what was changed and why.

## Subsystems Modified
- [ ] `firmware/` (Pico 2 W / RP2350 C firmware)
- [ ] `android/` (Android Companion Kotlin / Jetpack Compose)
- [ ] `tools/` (Raspberry Pi 5 Python AI server & local engine)
- [ ] `docs/` (Architecture, experiment logs, pinout specifications)
- [ ] `.github/` (CI/CD workflows and repository automation)

## Hardware Safety Verification
Before submitting, certify compliance with hardware safety rules:
- [ ] **3.3V Logic Rule:** No 5V logic fed into any Pico 2 W GPIO pin.
- [ ] **Current Cap Rule:** No GPIO used as a high-current power supply.
- [ ] **LED Thermal/Power Cap:** Total WS2812B brightness strictly capped at <= 18% in firmware.
- [ ] **Common Ground:** All modules share a solid common ground plane.
- [ ] **Single State Machine:** All UI / audio / LED transitions routed through the central state machine.

## Testing & Validation
- [ ] Built `celestial_firmware.uf2` with zero compilation warnings/errors.
- [ ] Ran native test harness (`scripts/check-audio.ps1` or `python -m unittest discover tests`).
- [ ] Verified physical behavior on hardware or Android emulator.
