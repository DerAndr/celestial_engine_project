---
name: pico-rp2350-development
description: >-
  Provides instructions to build, configure, flash, and debug C/C++ firmware for the Celestial Engine Mk.V
  staff on Raspberry Pi Pico 2 W (RP2350 / Dual ARM Cortex-M33). Use this skill when the user asks to
  compile firmware, edit main.c/leds.c/audio.c/net_client.c, configure CMake for RP2350, handle CYW43
  Wi-Fi UDP v2, flash UF2 binaries, or debug serial output over USB CDC.
---

# Raspberry Pi Pico 2 W (RP2350) Firmware Runbook — Celestial Engine Mk.V

## 1. Target Hardware & Dual-Core Architecture
- **MCU**: Raspberry Pi RP2350 (Dual ARM Cortex-M33 @ 150 MHz, 520 KB SRAM)
- **Board**: Raspberry Pi Pico 2 W (`PICO_BOARD=pico2_w`, `PICO_PLATFORM=rp2350-arm-s`)
- **Wireless**: Infineon `CYW43439` (802.11n Wi-Fi + BLE 5.2), onboard LED on `CYW43_WL_GPIO_LED_PIN`.
- **Dual-Core Split (`firmware/src/main.c`)**:
  - **Core 0 (Network, UI & State Machine)**:
    - Manages `CYW43439` Wi-Fi connection (Hotspot-First default -> Android Phone AP, plus `[HOST]` OLED menu toggle for `PHONE BRAIN`, `PI 5 BRAIN`, `AUTO MESH`).
    - Binary UDP v2 client/server on port `5005` (`firmware/src/net_client.c`, FNV-1a magic `0x811C9DC5`, CRC-16-CCITT).
    - `1.3-inch SH1106` 128×64 I2C1 OLED (`0x3C`, `GP2` SDA, `GP3` SCL @ 400 kHz) rendering 13 system modes and 20 FPS scrolling ticker.
    - Quadrature rotary encoder `Bourns PEC11R` (`GP6` CLK, `GP7` DT, `GP8` SW) and green side `QUERY` push-to-talk button (`GP9`, active-LOW).
  - **Core 1 (Deterministic Real-Time PIO + DMA)**:
    - **LED Engine (`firmware/src/leds.c`)**: Drives **43 addressable WS2812B LEDs** (`7 Core + 12 Inner + 24 Outer = 43`; the 4th 16-LED ring was permanently removed) on `GP15` via `SN74AHCT125N` 5V level shifter (`18%` brightness cap, **28 concentric patterns** including 2D polar kinematics spells and real-time phonetic VU coupling).
    - **Audio Engine (`firmware/src/audio.c`)**: Captures `16 kHz` 16-bit mono PCM from `INMP441` mic (`GP10/11/12`, `PIO0`) and plays `16 kHz` TTS audio from a lock-free **64 KB circular ring buffer** (`2.04s` capacity), **21 8-bit chiptunes**, and **14 cosmic/R2-D2/spell SFX** through `MAX98357A` I2S Class-D amp (`GP16/17/18`, `PIO1`).

## 2. Critical Anti-Regression Rules
1. **Centralized Pinout (`firmware/include/config.h`)**:
   - Never hard-code GPIO numbers or LED counts (`43`) in `.c` files; always reference macros from `firmware/include/config.h`.
2. **Physical Emergency Abort / Interrupt (`GP8` / `GP9`)**:
   - Pressing the rotary encoder button (`GP8`) or `QUERY` (`GP9`) while in `STATE_PROCESSING` or `STATE_SPEAKING` must immediately abort active audio DMA, flush the 64 KB playback ring buffer, and transition the state machine back to `STATE_IDLE`.
3. **Web Digital Twin Synchronization**:
   - Whenever adding or modifying LED patterns (`28`) in `firmware/src/leds.c` or chiptunes/SFX (`21` melodies + `14` SFX) in `firmware/src/audio.c`, keep `docs/simulator.js`, `docs/twin/state.js`, and `README.md` synchronized.

## 3. Build & Verification Commands
From repository root:
```powershell
# Compile firmware (.uf2)
cmake --build firmware/build --target celestial_firmware --parallel 4

# Run Python protocol & regression tests
python -m unittest discover -s tests -v
```

## 4. Flashing `celestial_firmware.uf2`
1. Disconnect Pico 2 W from USB/battery power.
2. Hold the white **BOOTSEL** button on the Pico 2 W while plugging USB into the PC.
3. Release BOOTSEL once the `RP2350` mass-storage drive mounts.
4. Copy `firmware/build/celestial_firmware.uf2` to the drive:
   ```powershell
   Copy-Item firmware\build\celestial_firmware.uf2 <PICO_DRIVE_LETTER>:\
   ```
