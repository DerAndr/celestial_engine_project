---
name: pico-rp2350-development
description: >-
  Build, configure, flash, and debug firmware for the Raspberry Pi Pico 2 W (RP2350 / ARM Cortex-M33).
  Use when compiling C/C++ firmware, configuring CMake for RP2350 and Pico 2 W, handling CYW43 Wi-Fi/Bluetooth,
  flashing UF2 binaries, or debugging serial output over USB CDC.
---

# Raspberry Pi Pico 2 W (RP2350) Development Runbook

## Target Specifications
- **MCU**: Raspberry Pi RP2350 (Dual ARM Cortex-M33 @ 150MHz)
- **Board**: Raspberry Pi Pico 2 W
- **Wireless**: Infineon CYW43439 (Wi-Fi 4 802.11n + Bluetooth 5.2)
- **SDK**: Raspberry Pi Pico SDK 2.x+ (`PICO_BOARD=pico2_w`, `PICO_PLATFORM=rp2350-arm-s`)
- **Onboard LED**: Connected via CYW43 chip (`CYW43_WL_GPIO_LED_PIN`), NOT direct RP2350 GPIO.

## CMake Configuration Standard (`CMakeLists.txt`)
```cmake
cmake_minimum_required(VERSION 3.13)

# Must define board and platform before sdk_init
set(PICO_BOARD pico2_w)
set(PICO_PLATFORM rp2350-arm-s)

include(pico_sdk_import.cmake)

project(celestial_engine C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(celestial_firmware
    src/main.c
)

# Pull in Pico SDK libraries
target_link_libraries(celestial_firmware
    pico_stdlib
    pico_cyw43_arch_none # or pico_cyw43_arch_lwip_poll / threadsafe_background for Wi-Fi
    hardware_gpio
    hardware_i2c
    hardware_pio
    hardware_dma
)

# Enable USB CDC stdio, disable UART stdio if using USB
pico_enable_stdio_usb(celestial_firmware 1)
pico_enable_stdio_uart(celestial_firmware 0)

# Create .uf2, .hex, .bin
pico_add_extra_outputs(celestial_firmware)
```

## Build Process
From `firmware/` directory:
```powershell
cmake -B build -G "Ninja" -DPICO_BOARD=pico2_w -DPICO_PLATFORM=rp2350-arm-s
cmake --build build
```

## Flashing UF2
1. Disconnect Pico 2 W from USB.
2. Press and hold the white **BOOTSEL** button on the Pico.
3. Plug the USB cable into the PC while holding BOOTSEL.
4. Release BOOTSEL once the mass storage drive `RP2350` (or `RPI-RP2`) mounts in Windows.
5. Copy `celestial_firmware.uf2` to the drive.
6. The Pico will automatically reboot and run the firmware.

## USB Serial Monitoring
On Windows, the Pico appears as a virtual COM port (e.g. `COM3`):
```powershell
# Check assigned COM port
Get-CimInstance Win32_SerialPort | Select-Object DeviceID, Description
```
Monitor via terminal (e.g. PuTTY, minicom, or VS Code Serial Monitor) at 115200 baud.
