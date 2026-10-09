# Master Pinout — Celestial Engine Mk.V

**Status: DRAFT ALLOCATION — Ready for milestone validation**

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
| **OLED I2C SDA** | SDA | Pin 4 | GP2 (I2C1 SDA) | 3.3 V logic | PLANNED (P1) |
| **OLED I2C SCL** | SCL | Pin 5 | GP3 (I2C1 SCL) | 3.3 V logic | PLANNED (P1) |
| **GND** | GND | Pin 8 | GND | Common Ground | SYSTEM |
| **Encoder A** | Pin A | Pin 9 | GP6 (GPIO IN) | 3.3 V with internal pull-up | PLANNED (P2) |
| **Encoder B** | Pin B | Pin 10 | GP7 (GPIO IN) | 3.3 V with internal pull-up | PLANNED (P2) |
| **Encoder SW** | Pin SW | Pin 11 | GP8 (GPIO IN) | 3.3 V with internal pull-up | PLANNED (P2) |
| **QUERY Button** | NO Contact | Pin 12 | GP9 (GPIO IN) | 3.3 V with internal pull-up | PLANNED (P3) |
| **GND** | GND | Pin 13 | GND | Common Ground | SYSTEM |
| **INMP441 SCK** (BCLK) | SCK | Pin 14 | GP10 (PIO0 SM0) | 3.3 V logic (I2S RX bit clock) | PLANNED (P6) |
| **INMP441 WS** (LRCK) | WS | Pin 15 | GP11 (PIO0 SM0) | 3.3 V logic (I2S RX word select) | PLANNED (P6) |
| **INMP441 SD** (Data) | SD | Pin 16 | GP12 (PIO0 SM0) | 3.3 V logic (I2S data into Pico) | PLANNED (P6) |
| **GND** | GND | Pin 18 | GND | Common Ground | SYSTEM |
| **WS2812 Data** | AHCT125 1A | Pin 20 | GP15 (PIO2 SM0) | 3.3 V GPIO -> AHCT125 -> 470R -> DIN | PLANNED (P4) |
| **MAX98357A BCLK** | BCLK | Pin 21 | GP16 (PIO1 SM0) | 3.3 V logic (I2S TX bit clock) | PLANNED (P7) |
| **MAX98357A LRC** | LRC | Pin 22 | GP17 (PIO1 SM0) | 3.3 V logic (I2S TX word select) | PLANNED (P7) |
| **GND** | GND | Pin 23 | GND | Common Ground | SYSTEM |
| **MAX98357A DIN** | DIN | Pin 24 | GP18 (PIO1 SM0) | 3.3 V logic (I2S data into amp) | PLANNED (P7) |
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
1. **Clock Domain Separation**: INMP441 (recording speech at 16 kHz mono) and MAX98357A (playing TTS speech at 22.05/24/44.1 kHz) use separate PIO state machines and independent GPIO clock lines (GP10/11 vs GP16/17).
2. **Level Shifting**: The WS2812 requires 5 V DIN logic. The SN74AHCT125N translates 3.3 V from GP15 to 5 V. Never connect 5 V directly to any Pico GPIO.
