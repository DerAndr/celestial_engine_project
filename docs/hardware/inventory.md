# Hardware Inventory

Update status as parts arrive and are tested.

Status values: `ORDERED`, `RECEIVED`, `TESTED`, `INSTALLED`, `FAILED`.

| Component | Qty | Status | Notes |
|---|---:|---|---|
| Raspberry Pi Pico 2 W with headers | 1 | TESTED | RP2350; Wi-Fi/Bluetooth; P0 validated on COM9 |
| SH1106 1.3in 128x64 I2C OLED | 1 | TESTED | P1 validated on I2C1 (0x3C); clean display telemetry |
| Bourns PEC11R-4225F-S0024 encoder | 1 | TESTED | P2 validated; Gray quadrature + push switch on GP6/7/8 |
| Prototype momentary NO QUERY button | 1 | TESTED | P3 validated; Push-to-Talk active-LOW on GP9 |
| INMP441 I2S microphone | 1 | RECEIVED | Pin headers soldered; ready for P7 audio input |
| MAX98357A I2S amplifier | 3 | RECEIVED | Pin headers & screw terminal soldered; in test for P6 |
| 4 ohm / 3 W mini speaker | 2 | RECEIVED | Ready for connection to MAX98357A |
| WS2812B 7-LED ring | 1 | RECEIVED | 3 wires soldered (5V, GND, DIN); ready for P4/P5 |
| WS2812B 12-LED ring | 1 | ORDERED | OD 37 / ID 23 mm |
| WS2812B 16-LED ring | 1 | ORDERED | OD 45 / ID 32 mm |
| WS2812B 24-LED ring | 1 | ORDERED | OD 66 / ID 52 mm |
| SN74AHCT125N | 2 | RECEIVED | WS2812 3.3V->5.0V level shifting DIP-14 IC |
| ~470 ohm resistor | TBD | RECEIVED | WS2812 data series protection resistor |
| ~1000 uF >=10 V capacitor | TBD | RECEIVED | LED power bulk decoupling capacitance |
| Velleman VTBB3 breadboard | 1 | RECEIVED | 400 holes (30 rows, a-j, +/- rails) |
| 22 AWG red/black wire | 1 set | RECEIVED | Power distribution |
| 26 AWG multicolor wire | 1 set | RECEIVED | Signals |
| Raspberry Pi 5 | 1 | RECEIVED | Already owned; wearable brain |
| Raspberry Pi Active Cooler | 1 | RECEIVED | Already owned |
| Geekworm X1209 V1.0 | 1 | ORDERED | Pi 5 portable power only |
| Geekworm X12-A1 + X12-A3 | 1 | ORDERED | Pi 5 portable power only |
| Molicel INR18650-M35A 3500 mAh flat-top | 4 | ORDERED | Pi 5 portable power only |
