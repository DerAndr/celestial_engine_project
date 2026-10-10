# Master Pinout — Celestial Engine Mk.V

**Status: GPIO allocation matches firmware; P5 full-array wiring remains unverified.**

Reconciled on 2026-10-10 against `config.h`, the drivers and recorded milestones in `PROJECT.md`. This is a documentation correction, not a new physical test.

Firmware: `0.7.2-AUDIO-DIAG`. RECORDED PASS refers to historical milestones. Current audio revalidation failed; use [experiment 009](../experiments/009-speaker-distortion.md) before combined regression/P5. Physical pin numbers and actual module markings take precedence over historical breadboard coordinates.

This global GPIO allocation satisfies all peripherals for the Celestial Engine staff:
- OLED (I2C)
- Rotary encoder (A/B/SW)
- QUERY push button
- I2S microphone (INMP441, separate PIO RX clock domain)
- I2S amplifier (MAX98357A, separate PIO TX clock domain)
- WS2812 LED array (via SN74AHCT125 level shifter)
- UART / debug console

## Pin Allocation Table

| Function | Module Pin | Pico Physical Pin | GPIO / Hardware | Voltage / Notes | Status |
|---|---|---:|---|---|---|
| **UART0 TX** (Debug) | Serial TX | Pin 1 | GP0 (UART0 TX) | 3.3 V logic (optional hardware console) | RESERVED |
| **UART0 RX** (Debug) | Serial RX | Pin 2 | GP1 (UART0 RX) | 3.3 V logic (optional hardware console) | RESERVED |
| **GND** | GND | Pin 3 | GND | Common Ground | SYSTEM |
| **OLED I2C SDA** | SDA | Pin 4 | GP2 (I2C1 SDA) | 3.3 V logic | RECORDED PASS (P1) |
| **OLED I2C SCL** | SCL | Pin 5 | GP3 (I2C1 SCL) | 3.3 V logic | RECORDED PASS (P1) |
| **GND** | GND | Pin 8 | GND | Common Ground | SYSTEM |
| **Encoder A** | Pin A | Pin 9 | GP6 (GPIO IN) | 3.3 V with internal pull-up | RECORDED PASS (P2) |
| **Encoder B** | Pin B | Pin 10 | GP7 (GPIO IN) | 3.3 V with internal pull-up | RECORDED PASS (P2) |
| **Encoder SW** | Pin SW | Pin 11 | GP8 (GPIO IN) | 3.3 V with internal pull-up | RECORDED PASS (P2) |
| **QUERY Button** | NO Contact | Pin 12 | GP9 (GPIO IN) | 3.3 V with internal pull-up | RECORDED PASS (P3) |
| **GND** | GND | Pin 13 | GND | Common Ground | SYSTEM |
| **INMP441 SCK** (BCLK) | SCK | Pin 14 | GP10 (PIO0 SM0) | 3.3 V logic (I2S RX bit clock) | RECORDED PASS (P6) |
| **INMP441 WS** (LRCK) | WS | Pin 15 | GP11 (PIO0 SM0) | 3.3 V logic (I2S RX word select) | RECORDED PASS (P6) |
| **INMP441 SD** (Data) | SD | Pin 16 | GP12 (PIO0 SM0) | 3.3 V logic (I2S data into Pico) | RECORDED PASS (P6) |
| **GND** | GND | Pin 18 | GND | Common Ground | SYSTEM |
| **WS2812 Data** | AHCT125 1A / verified direct core DIN | Pin 20 | GP15 (PIO0 SM1) | 3.3 V GPIO; level-shift before full-array integration | CORE TESTED (P4); P5 PENDING |
| **MAX98357A BCLK** | BCLK | Pin 21 | GP16 (PIO1 SM0) | 3.3 V logic (I2S TX bit clock) | RECORDED PASS (P7) |
| **MAX98357A LRC** | LRC | Pin 22 | GP17 (PIO1 SM0) | 3.3 V logic (I2S TX word select) | RECORDED PASS (P7) |
| **GND** | GND | Pin 23 | GND | Common Ground | SYSTEM |
| **MAX98357A DIN** | DIN | Pin 24 | GP18 (PIO1 SM0) | 3.3 V logic (I2S data into amp) | RECORDED PASS (P7) |
| **MAX98357A GAIN** | GAIN | — | Unconnected (Floating) | Left floating for 9 dB default hardware gain (no wire needed) | HARDWARE DEFAULT |
| **Spare GPIO** | — | Pin 25 | GP19 | Available expansion | FREE |
| **Spare GPIO** | — | Pin 26 | GP20 | Available expansion | FREE |
| **Spare GPIO** | — | Pin 27 | GP21 | Available expansion | FREE |
| **GND** | GND | Pin 28 | GND | Common Ground | SYSTEM |
| **Spare GPIO** | — | Pin 29 | GP22 | Available expansion | FREE |
| **Battery / ADC Sense** | Voltage Divider | Pin 31 | GP26 (ADC0) | Max 3.3 V analog input | FREE |
| **3V3 OUT** | VCC (Logic) | Pin 36 | 3V3(OUT) | 3.3 V supply to sensors & OLED | SYSTEM |
| **GND** | GND | Pin 38 | GND | Common Ground | SYSTEM |
| **VBUS (5V USB)** | 5V Rail | Pin 40 | VBUS | 5.0 V USB supply (Amp / Level Shifter) | SYSTEM |

## RP2350 Audio Architecture Notes
1. **Clock Domain Separation**: Microphone metering and speaker playback both run at 16 kHz with separate PIO state machines and GPIO clocks (GP10/11 vs GP16/17). Output now carries signed PCM24 in 32-bit stereo slots: BCLK 1.024 MHz, LRCLK 16 kHz. Diagnostic mode keeps silent frames running. Future TTS transport is not implemented.
2. **PIO allocation**: Mic is PIO0 SM0; LEDs are PIO0 SM1; speaker is PIO1 SM0. Pico SDK 2.3.1's CYW43 allocator searches from PIO2 and claims a free state machine. Do not move LEDs onto the documented former PIO2 SM0 assignment without checking wireless resource ownership.
3. **Level Shifting**: The SN74AHCT125N provides the planned reliable 3.3 V -> 5 V LED data path. The direct core-ring smoke test is recorded as passed; shifter/full-array validation is separate. Never connect 5 V directly to any Pico GPIO.
4. **Current configured array**: The active array is configured for 43 pixels across 3 concentric rings (7 + 12 + 24), with the defective 16-LED ring removed from the daisy chain. Maximum brightness is enforced at 18% in firmware (`LED_BRIGHTNESS_LIMIT = 18`). Using the 60 mA/LED model, worst-case full white at 18% is ~464 mA, and typical animated colors draw 140–190 mA, well within safe USB limits.
5. **Amplifier GAIN**: Left unconnected (default internal 9 dB gain). Idle sleep mode after 80 ms drops BCLK to shut down the amplifier during silence and prevent heating.
