# Master Pinout — Celestial Engine Mk.V

**Status: GPIO allocation matches firmware; 3 concentric rings verified (43 LEDs: 7 Core + 12 Inner + 24 Outer). 4th 16-LED ring removed by user due to instability.**

Reconciled through the 2026-10-11 update against `config.h`, drivers and user hardware modifications. 

Current main: `1.0.16-RESPONSE-FIX` (`celestial_firmware.uf2`). Uses 16-bit stereo I2S, 512 kHz BCLK and 16 kHz LRC during playback. The 4th 16-LED ring was removed from the physical assembly. LED array reverted to the verified 3-ring chain (43 LEDs: 7 Core + 12 Inner + 24 Outer) with PIO-paced DMA and standard 18% brightness cap. No GPIO assignment changes.

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
| **WS2812 Data** | AHCT125 1A / verified direct core DIN | Pin 20 | GP15 (PIO0 SM1) | 3.3 V GPIO; actual shifter wiring remains unverified | THREE CONCENTRIC RINGS (43 LEDs) DMA PASS |
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
1. **Clock Domain Separation**: Microphone metering and speaker playback both run at 16 kHz with separate PIO state machines and GPIO clocks (GP10/11 vs GP16/17). Output carries signed PCM16 in 16-bit stereo slots: BCLK 512 kHz, LRCLK 16 kHz. Normal playback stops clocks after idle. Generated speech now returns through acknowledged UDP v2; new physical verification is pending.
2. **PIO allocation**: Mic is PIO0 SM0; LEDs are PIO0 SM1; speaker is PIO1 SM0. Pico SDK 2.3.1's CYW43 allocator searches from PIO2 and claims a free state machine. Do not move LEDs onto the documented former PIO2 SM0 assignment without checking wireless resource ownership.
3. **Level Shifting**: The SN74AHCT125N provides the planned reliable 3.3 V -> 5 V LED data path. The direct core-ring smoke test is recorded as passed; shifter/full-array validation is separate. Never connect 5 V directly to any Pico GPIO.
4. **Current configured array**: 43 LEDs in 7→12→24 chain, PIO0 SM1 DMA, 18% cap and 300 µs reset. The user discarded the fourth 16-LED ring after instability; it is excluded from main. No GPIO changes.
5. **Amplifier GAIN**: Left unconnected (default internal 9 dB gain). The normal application stops I2S clocks after idle; physical current, heating and acoustic behavior have not been measured. The isolated probe starts silent and stops clocks after one bounded transfer.
