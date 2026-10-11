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
    src/mic.c
    src/leds.c
    src/voice_data.c
    src/net_client.c
)
target_compile_definitions(celestial_firmware PRIVATE CELESTIAL_FOUR_RINGS=0)

pico_generate_pio_header(celestial_firmware ${CMAKE_CURRENT_LIST_DIR}/src/audio_i2s.pio)
pico_generate_pio_header(celestial_firmware ${CMAKE_CURRENT_LIST_DIR}/src/mic_i2s.pio)
pico_generate_pio_header(celestial_firmware ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

target_include_directories(celestial_firmware PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_compile_options(celestial_firmware PRIVATE -Wall -Wextra)

# Link standard libraries, CYW43 Wi-Fi architecture, I2C, PIO, DMA, and clocks
target_link_libraries(celestial_firmware
    pico_stdlib
    pico_rand
    pico_cyw43_arch_lwip_threadsafe_background
    hardware_gpio
    hardware_i2c
    hardware_pio
    hardware_dma
    hardware_clocks
    pico_bootrom
    m
)

# Enable USB CDC serial output, disable hardware UART for stdio
pico_enable_stdio_usb(celestial_firmware 1)
pico_enable_stdio_uart(celestial_firmware 0)

# Generate UF2, HEX, and BIN outputs
pico_add_extra_outputs(celestial_firmware)

# Main follows the user's three-ring wiring; the discarded fourth ring is absent.
# Retain the historical diagnostic target separately; it is not the active setup.
get_target_property(CELESTIAL_APP_SOURCES celestial_firmware SOURCES)
add_executable(celestial_ring_experiment ${CELESTIAL_APP_SOURCES})
target_compile_definitions(celestial_ring_experiment PRIVATE CELESTIAL_FOUR_RINGS=1 CELESTIAL_RING_EXPERIMENT=1)
foreach(pio_source audio_i2s mic_i2s ws2812)
    pico_generate_pio_header(celestial_ring_experiment ${CMAKE_CURRENT_LIST_DIR}/src/${pio_source}.pio)
endforeach()
target_include_directories(celestial_ring_experiment PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_compile_options(celestial_ring_experiment PRIVATE -Wall -Wextra)
target_link_libraries(celestial_ring_experiment
    pico_stdlib pico_rand
    pico_cyw43_arch_lwip_threadsafe_background
    hardware_gpio hardware_i2c hardware_pio hardware_dma hardware_clocks
    pico_bootrom m
)
pico_enable_stdio_usb(celestial_ring_experiment 1)
pico_enable_stdio_uart(celestial_ring_experiment 0)
pico_add_extra_outputs(celestial_ring_experiment)

# Isolate the failed audio milestone; the normal application target stays separate.
add_executable(speaker_probe src/speaker_probe.c)
pico_generate_pio_header(speaker_probe ${CMAKE_CURRENT_LIST_DIR}/src/audio_i2s.pio)
target_include_directories(speaker_probe PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_compile_options(speaker_probe PRIVATE -Wall -Wextra)
target_link_libraries(speaker_probe pico_stdlib hardware_pio hardware_dma hardware_clocks m)
pico_enable_stdio_usb(speaker_probe 1)
pico_enable_stdio_uart(speaker_probe 0)
pico_add_extra_outputs(speaker_probe)
