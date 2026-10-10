# Hardware Inventory

Update status as parts arrive and are tested.

Current software: `0.7.2-AUDIO-DIAG`. TESTED entries describe historical observations. The user reports distorted/uncomfortably loud audio at V10; GAIN wire removed. Follow [experiment 009](../experiments/009-speaker-distortion.md). No speaker/amplifier defect is proven; this is a failed subsystem regression.

Status values: `ORDERED`, `RECEIVED`, `TESTED`, `INSTALLED`, `FAILED`.

| Component | Qty | Status | Notes |
|---|---:|---|---|
| Raspberry Pi Pico 2 W with headers | 1 | TESTED | RP2350; Wi-Fi/Bluetooth; P0 validated on COM9 |
| SH1106 1.3in 128x64 I2C OLED | 1 | TESTED | P1 validated on I2C1 (0x3C); clean display telemetry |
| Bourns PEC11R-4225F-S0024 encoder | 1 | TESTED | P2 validated; Gray quadrature + push switch on GP6/7/8 |
| Prototype momentary NO QUERY button | 1 | TESTED | P3 validated; Push-to-Talk active-LOW on GP9 |
| INMP441 I2S microphone | 1 | TESTED | P6 live metering/voice reaction recorded as verified in PROJECT.md |
| MAX98357A I2S amplifier | 3 | RECEIVED | One historical P7 pass; current output distorted at V10, GAIN removed; debugging in 009. Other units not individually verified |
| 4 ohm / 3 W mini speaker | 2 | RECEIVED | One historical P7 pass; current distortion under investigation, damage not established. Second speaker not individually verified |
| WS2812B 7-LED ring | 1 | TESTED | P4 direct GP15 core-ring test recorded as passed; full array remains P5 |
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

2026-10-10 software review reconciled these entries with existing project/experiment records. It did not establish new arrivals or new physical passes. The 12/16/24-ring arrival status remains unconfirmed beyond the recorded ORDERED entries.
