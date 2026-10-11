# 028 — Three-Ring Concentric LED Array Reversion & Firmware 1.0.15

Date: 2026-10-11. Target: Pico 2 W / RP2350. Main firmware: **1.0.15-THREE-RINGS**.

## Goal
Revert the Celestial Engine LED subsystem to the verified 3-concentric-ring array (43 LEDs: 7 Core + 12 Inner + 24 Outer) following physical removal of the unstable 4th 16-LED ring by the user, while preserving jitter-free PIO-paced DMA and the 18% USB-safe brightness cap.

## Parts
The existing assembled staff:
- Raspberry Pi Pico 2 W (RP2350)
- WS2812B 7-LED core ring (indices 0..6)
- WS2812B 12-LED inner ring (indices 7..18)
- WS2812B 24-LED outer ring (indices 19..42)
- SN74AHCT125N level shifter (GP15 -> Level Shifter -> LED DIN)
- SH1106 1.3in I2C OLED display (GP2/GP3)
- Bourns PEC11R rotary encoder (GP6/GP7/GP8)
- Momentary NO QUERY button (GP9)
- INMP441 I2S microphone (GP10/GP11/GP12)
- MAX98357A I2S amplifier & QUARKZMAN 4 ohm / 3 W speaker (GP16/GP17/GP18)

*Note: The 4th 16-LED ring previously at the tail of the chain has been physically removed.*

## Power off
Disconnect USB power before making any physical alterations to wiring. In this milestone, the user has already disconnected the 4th ring.

## Wiring table
| Module | Module Pin | Pico Physical Pin | GPIO / Hardware | Voltage / Note |
|---|---|---|---|---|
| WS2812B 7-LED Core | DIN | Pin 20 | GP15 (PIO0 SM1 via AHCT125) | 5.0 V data out of level shifter |
| WS2812B 7-LED Core | DOUT | Ring 1 DIN | — | Serial chain to 12-LED ring |
| WS2812B 12-LED Inner | DOUT | Ring 2 DIN | — | Serial chain to 24-LED ring |
| WS2812B 24-LED Outer | DOUT | — | — | Unconnected (tail of 43-LED chain) |
| All 3 Rings | +5V / GND | — | 5V / GND Rails | Common ground & bulk capacitor |

## Code and build changes
1. `firmware/CMakeLists.txt`: Set `CELESTIAL_FOUR_RINGS=0` compile definition for `celestial_firmware`.
2. `firmware/src/config.h`:
   - Updated `FIRMWARE_VERSION` to `"1.0.15-THREE-RINGS"`.
   - Set `LED_BASE_CHAIN_COUNT = 43` and `LED_RING_COUNT = 43`.
   - Set `LED_BRIGHTNESS_LIMIT = 18` (18% cap for USB power).
   - Set `LED_RESET_US = 300` (safe WS2812 reset latch).
   - Set `AUDIO_DEFAULT_VOLUME = 35` and `AUDIO_STARTUP_SOUNDS = 1`.
3. `firmware/src/leds.c`:
   - Enabled PIO-paced DMA unconditionally for 43-word frame packing and zero-jitter transmission.
   - Re-scaled `render_voice_reactive` across 3 concentric layers: 0..30% Core (7), 30..65% Inner (12), 65..100% Outer (24).
   - Distributed `render_stars` star3 index evenly (`+ 31`) across the 24-LED outer ring.
4. `firmware/src/main.c`:
   - Updated `MODE_CELESTIAL` OLED telemetry to `"RUNE: 3R-43L [OK]"` and `"43-LUMEN RUNE ARRAY"`.
   - Updated `MODE_LED_CORE` OLED card to `"RUNE: 3 CONCENTRIC RINGS"` and `"ARRAY: 43 LUMENS (18% CAP)"`.
   - Updated `MODE_SYSTEM` ticker to `"3-RINGS (43 LEDS)"`.
   - Updated boot banner to log `"THREE-RING DMA: frames:43 (7+12+24)"`.

## Commands — repository root
```powershell
# Run regression tests
powershell -File .\scripts\check-audio.ps1

# Compile firmware
cmake --build firmware/build --target celestial_firmware --parallel 4

# Flash Pico 2 W
& 'C:/Users/AndrD/.pico-sdk/picotool/2.3.1/picotool/picotool.exe' load -v -x .\firmware\build\celestial_firmware.uf2 -f
```

## Expected result
1. Pico 2 W boots cleanly, logs `THREE-RING DMA: frames:43 (7+12+24) | brightness_cap:18%`.
2. Plays boot chime and connects to Wi-Fi AP (`192.168.178.101`).
3. OLED dashboard displays `RUNE: 3R-43L [OK]`, `ARRAY: 43 LUMENS (18% CAP)`.
4. All 9 LED animations run smoothly with zero glitching or rogue white flashes across the 3 physical rings.
5. Holding QUERY streams microphone speech up to 60 seconds over UDP to PC Brain server.
