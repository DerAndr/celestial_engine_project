# Master Pinout — Celestial Engine Mk.V

**Status: GPIO allocation matches firmware; all four rings respond. Repaired-ring address mapping and white flashes remain unresolved.**

Reconciled through the 2026-10-11 checkpoint against `config.h`, the drivers and user observations in `PROJECT.md`. This is a documentation correction, not a new physical test.

Current main: `1.0.10-FOUR-RINGS`, built but awaiting physical validation; earlier working audio `1.0.5-AUDIO-GAIN` is archived. Both use 16-bit stereo I2S, 512 kHz BCLK and 16 kHz LRC during playback. User solder repair resolved ERR-AUDIO-02/03; see [experiment 017](../experiments/017-audio-solder-repair-level.md). All GPIO/wiring assignments are preserved. Physical pin numbers and actual module markings take precedence over historical breadboard coordinates.

User solder repair resolved the audio fault. [Experiment 018](../experiments/018-volume-consistent-gain.md) fixes boot/control gain consistency and raises startup V10 by +3.64 dB with the same maximum. GAIN remains unconnected. ONE QUARKZMAN 4-ohm / 3-W speaker is between OUT+/OUT−, neither wire to GND. No GPIO/wiring assignment changes.

Spare 16-LED ring: existing **7→12→24→spare** serial wiring is retained. Spare DIN from 24-ring DO, spare DO free, removable +5 V/GND from the same rails. The common-DI→faulty DOUT bypass still leaves the old faulty output attached ([019](../experiments/019-ring-single-pixel-repair.md)); address mapping/output isolation remain unresolved. [022](../experiments/022-ring-dma-firmware.md) is the passed 1.0.7 baseline. [025](../experiments/025-main-build-and-ring-backgrounds.md): main 1.0.10-FOUR-RINGS uses the standard celestial_firmware.uf2 name; separate celestial_ring_experiment.uf2 holds pure RGB on the spare in RAINBOW. Both retain PIO0 SM1, dynamic DMA, 59 reserved slots, 300 µs reset and 4% cap. Main removes the spare backgrounds in PULSAR/VOICE/VORTEX; visual voice order remains 7→12→16→24. No GPIO or physical ring order changes. White flashes remain unresolved and new physical observation is pending.

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
| **WS2812 Data** | AHCT125 1A / verified direct core DIN | Pin 20 | GP15 (PIO0 SM1) | 3.3 V GPIO; actual shifter wiring remains unverified | FOUR-RING RESPONSE CONFIRMED; EFFECTS PARTIAL |
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
1. **Clock Domain Separation**: Microphone metering and speaker playback both run at 16 kHz with separate PIO state machines and GPIO clocks (GP10/11 vs GP16/17). Output carries signed PCM16 in 16-bit stereo slots: BCLK 512 kHz, LRCLK 16 kHz. Normal playback stops clocks after idle. Future TTS transport is not implemented.
2. **PIO allocation**: Mic is PIO0 SM0; LEDs are PIO0 SM1; speaker is PIO1 SM0. Pico SDK 2.3.1's CYW43 allocator searches from PIO2 and claims a free state machine. Do not move LEDs onto the documented former PIO2 SM0 assignment without checking wireless resource ownership.
3. **Level Shifting**: The SN74AHCT125N provides the planned reliable 3.3 V -> 5 V LED data path. The direct core-ring smoke test is recorded as passed; shifter/full-array validation is separate. Never connect 5 V directly to any Pico GPIO.
4. **Current configured array**: Physical chain is 7→12→24→repaired 16-position ring, with 59 reserved serial slots and a 4% brightness cap. The user bypassed one failed LED with a common-DIN→failed-DOUT jumper; its original output remains attached and one position stays dark. All four rings respond, but exact repaired-ring addressing and individual white flashes remain unresolved. Final current and power validation is pending. Full four-ring lighting is a priority in [tasks](../tasks.md).
5. **Amplifier GAIN**: Left unconnected (default internal 9 dB gain). The normal application stops I2S clocks after idle; physical current, heating and acoustic behavior have not been measured. The isolated probe starts silent and stops clocks after one bounded transfer.
