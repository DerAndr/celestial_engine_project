# Experiment 000 — Pico Alive (P0)
 
Status: COMPLETED (PASSED)

Historical P0 record: `celestial_p0.uf2`, drive `D:` and banner `0.1.0-P0` refer to the original test. Current firmware is `0.7.1-P4`, built as `firmware/build/celestial_firmware.uf2`; detect the actual drive and serial port using [experiment 007](007-code-review-regression.md). Current code initializes CYW43 but does not scan, connect to Wi-Fi or contact a Pi service. This preserves the original pass without claiming a new wireless test.

## Goal
Validate Raspberry Pi Pico 2 W toolchain, build process, firmware flashing via BOOTSEL, USB CDC serial logs, CYW43 onboard LED control, and Wi-Fi scanning.

## Hardware Required
- Raspberry Pi Pico 2 W (with pre-soldered headers)
- Micro-USB (or USB-C depending on board variant) data cable (must support data, not charge-only)
- Host development PC running Windows

## Wiring
No external breadboard wiring or modules for this step. Pico connects directly to the host PC via USB.

## Tests & Milestones
- [x] Pico 2 W CMake project created (`firmware/`)
- [x] Centralized hardware config created (`firmware/src/config.h`)
- [x] Master pinout updated for RP2350 (`docs/wiring/pinout.md`)
- [x] Raspberry Pi Pico SDK / VS Code build toolchain confirmed
- [x] Firmware compiled to UF2 (`celestial_p0.uf2`)
- [x] Pico 2 W placed in BOOTSEL mode and flashed
- [x] USB CDC Serial output observed at 115200 baud (COM9)
- [x] Onboard LED blinking at 1 Hz
- [x] Wi-Fi architecture initialized and operating in threadsafe background mode

## Expected Behavior
1. Connecting Pico in BOOTSEL mode mounts a removable drive labeled `RP2350` (or `RPI-RP2`).
2. Dragging `celestial_p0.uf2` to the drive unmounts it immediately.
3. The onboard LED lights up solidly during initialization, then toggles ON/OFF every 1 second (1 Hz heartbeat).
4. Opening the virtual COM port (115200 baud) prints banner `Celestial Engine Mk.V - Firmware: 0.1.0-P0`, reports CYW43 initialization, scans and lists local Wi-Fi SSIDs, and prints regular heartbeat logs.

## Observed Behavior
1. Drive `RP2350` mounted as drive letter `D:\`.
2. Firmware `celestial_p0.uf2` flashed successfully; drive unmounted automatically and Pico rebooted.
3. USB CDC serial device enumerated on `COM9` at 115200 baud.
4. Active heartbeat logs sampled and verified:
   `[INFO] [HEARTBEAT] Tick #74 | LED: ON | State: P0_ALIVE`
   `[INFO] [HEARTBEAT] Tick #75 | LED: OFF | State: P0_ALIVE`
   `[INFO] [HEARTBEAT] Tick #76 | LED: ON | State: P0_ALIVE`
   `[INFO] [HEARTBEAT] Tick #77 | LED: OFF | State: P0_ALIVE`
5. Onboard LED confirmed toggling at 1 Hz (500 ms / 1 s cycle).

## Problems / Fixes
- **Build error `lwipopts.h: No such file or directory`**: Added [lwipopts.h](../../firmware/src/lwipopts.h) tailored for `pico_cyw43_arch_lwip_threadsafe_background` (`NO_SYS = 1`). Clean compilation succeeded producing `celestial_p0.uf2`.
- **Copy path confusion**: Absolute path to binary used to flash directly to `D:\`.
- **Serial CDC DTR signal**: When inspecting USB CDC on Windows, DTR (`DtrEnable = $true`) must be asserted for Pico USB stdio stream to be captured.

## Result
PASSED — Ready for P1 (SH1106 OLED Display).
