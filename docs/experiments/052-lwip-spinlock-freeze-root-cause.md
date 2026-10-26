# Experiment 052: Root-Cause Analysis and Fix for Periodic Pico 2 W Lockups

## Date
2026-10-26

## Objective
Identify and eliminate the root cause of the periodic total lockup / freeze on Raspberry Pi Pico 2 W where LEDs, OLED, and controls freeze after several minutes of runtime with the Android Companion active.

## Root Cause Analysis
1. **Fatal Deadlock in `stdio_usb` Spinlock from lwIP ISR Context:**
   - `pico_cyw43_arch_lwip_threadsafe_background` executes UDP receive callbacks (`net_client_udp_recv_callback`) inside an interrupt / alarm handler.
   - `net_client_udp_recv_callback` contained multiple blocking `printf()` calls (lines 287, 290, 315, 325, 372, 374, 392, 414, 489, 501).
   - In `main.c`, the main loop continuously calls `LOG_INFO("HEARTBEAT", ...)` every 1.0s and logs on encoder/button events.
   - On RP2350, `stdio_usb` uses a non-reentrant hardware spinlock. When Core 0 is inside `printf()` in the main thread and an incoming UDP packet triggers `net_client_udp_recv_callback`, the callback calls `printf()` and attempts to acquire the spinlock already held by Core 0 with interrupts disabled.
   - **Result:** Permanent recursive deadlock in `spin_lock_blocking`. The CPU halts forever; OLED, LEDs, audio, and USB CDC halt.

2. **CYW43 SPI Bus Race Condition (Missing Synchronization):**
   - In `main.c`, `cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state)` was called every 1.0s without `cyw43_arch_lwip_begin()`.
   - In `net_client.c`, `cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA)` and `cyw43_arch_wifi_connect_async` were called outside lwIP lock, causing SPI bus corruption during background network traffic.

3. **No Hardware Safety Net:**
   - The RP2350 hardware watchdog was not enabled, leaving the microcontroller permanently frozen upon deadlocks.

## Applied Fixes
1. **`firmware/src/net_client.c`:**
   - Removed all `printf()` calls from `net_client_udp_recv_callback`.
   - Deferred authorized companion command logging to the main thread in `main.c`.
   - Wrapped `cyw43_tcpip_link_status`, `netif`, and `cyw43_arch_wifi_connect_async` inside `cyw43_arch_lwip_begin()` and `cyw43_arch_lwip_end()`.
   - Moved UDP send failure `printf` outside of `cyw43_arch_lwip_end()`.

2. **`firmware/src/main.c`:**
   - Included `#include "hardware/watchdog.h"`.
   - Enabled 8-second hardware watchdog on boot with `watchdog_enable(8000, 1)`.
   - Added `watchdog_update()` in the main `while(true)` loop.
   - Added watchdog reset recovery detection: `if (watchdog_caused_reboot())`.
   - Wrapped all `cyw43_arch_gpio_put` calls in `cyw43_arch_lwip_begin()` and `cyw43_arch_lwip_end()`.

## Verification & Status
- Compiled clean: `celestial_firmware.uf2` (1,108,992 bytes).
- Ready for flashing via BOOTSEL.
