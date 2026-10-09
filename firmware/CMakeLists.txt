# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

# Default to Pico 2 W (RP2350)
set(PICO_BOARD pico2_w CACHE STRING "Target board")
set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Target platform")

include(pico_sdk_import.cmake)

project(celestial_engine C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(celestial_firmware
    src/main.c
    src/sh1106.c
    src/encoder.c
    src/query_button.c
    src/audio.c
)

pico_generate_pio_header(celestial_firmware ${CMAKE_CURRENT_LIST_DIR}/src/audio_i2s.pio)

target_include_directories(celestial_firmware PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

# Link standard libraries, CYW43 Wi-Fi architecture, I2C, PIO, DMA, and clocks
target_link_libraries(celestial_firmware
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    hardware_gpio
    hardware_i2c
    hardware_pio
    hardware_dma
    hardware_clocks
    m
)

# Enable USB CDC serial output, disable hardware UART for stdio
pico_enable_stdio_usb(celestial_firmware 1)
pico_enable_stdio_uart(celestial_firmware 0)

# Generate UF2, HEX, and BIN outputs
pico_add_extra_outputs(celestial_firmware)
