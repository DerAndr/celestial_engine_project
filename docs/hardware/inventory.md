# Hardware Inventory

Update status as parts arrive and are tested.

Current main software: **1.0.10-FOUR-RINGS**, standard celestial_firmware.uf2; current diagnostic: **1.0.10-RING-DIAGNOSTIC**, celestial_ring_experiment.uf2. Both built/checked, not flashed while Pico is disconnected. Current profile uses 4% LEDs and silent V0 startup. Old three-ring/audio 1.0.5 UF2 is archived as celestial_audio_1.0.5.uf2. Audio solder repair and working gain remain established; no failed speaker/amplifier IC is proven. See [025](../experiments/025-main-build-and-ring-backgrounds.md).

Latest observation (2026-10-10): after fixing bad soldering, audio works and is very quiet; user requests only a small increase. GAIN remains unconnected. QUARKZMAN pack of two nominal 4 ohm / 3 W speakers, JST-PH2.0, listing 44 mm; sensitivity/frequency-response data not established. ONE speaker is connected exclusively to amplifier +/−, with neither wire to GND.

User-reported amplifier marking: `MAX98357A I2S MONO AMP`, `VIN 2.5-5.V`, `DEFAULT GAIN 9DB`, `L+R / 2 OUT`. Module is marked for I2S with 9 dB default gain and stereo-average mono output; exact breakout manufacturer/photo remain unavailable. This is identification evidence, not a new audio pass.

Status values: `ORDERED`, `RECEIVED`, `TESTED`, `INSTALLED`, `FAILED`; the repaired ring has an explicit qualified status for user repair and confirmed data response.

| Component | Qty | Status | Notes |
|---|---:|---|---|
| Raspberry Pi Pico 2 W with headers | 1 | TESTED | RP2350; Wi-Fi/Bluetooth; P0 validated on COM9 |
| SH1106 1.3in 128x64 I2C OLED | 1 | TESTED | P1 validated on I2C1 (0x3C); clean display telemetry |
| Bourns PEC11R-4225F-S0024 encoder | 1 | TESTED | P2 validated on GP6/7/8. User reports sensitive navigation with 1.0.8; 1.0.9 uses four transitions per step and atomic IRQ/poll decoding. Build/native checks pass; physical detent behavior pending (024). |
| Prototype momentary NO QUERY button | 1 | TESTED | P3 validated; Push-to-Talk active-LOW on GP9 |
| INMP441 I2S microphone | 1 | TESTED | P6 live metering/voice reaction recorded as verified in PROJECT.md |
| MAX98357A I2S amplifier | 3 | TESTED | One unit revalidated after user solder repair; GAIN unconnected. Other units not individually verified |
| QUARKZMAN 4 ohm / 3 W mini speaker, JST-PH2.0 | 2 | TESTED | One working speaker after solder repair; listing 44 mm. Second speaker not individually verified |
| WS2812B 7-LED ring | 1 | TESTED | P4 direct GP15 core-ring test recorded as passed; full array remains P5 |
| WS2812B 12-LED ring | 1 | TESTED | OD 37 / ID 23 mm; received/connected and working in user-confirmed 7→12→24→16 chain |
| WS2812B 16-LED ring | 1 | REPAIRED BY USER / DATA RESPONSE CONFIRMED | User soldered a jumper around one failed LED: common ring DIN→failed LED DOUT; LED not removed, old output still attached (019). One position stays dark; other positions work in the last ring after the 24-ring. 1.0.7 four-ring light response confirmed at 4% (022); snake good. White rainbow flashes and exact addressing/isolation remain unresolved. 1.0.10 backgrounds corrected in code and pure-RGB diagnostic prepared, physical result pending (025). Full four-ring light/music rewrite is priority 2 in tasks.md. |
| WS2812B 24-LED ring | 1 | TESTED | OD 66 / ID 52 mm; received/connected and working before the repaired 16-position ring |
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

2026-10-11 checkpoint reconciles arrival/status with subsequent photos and user-confirmed operation: 12/24 rings are present and working, and the user repaired the 16-position ring with a soldered jumper around one failed LED. This records the observed repair and light response, without claiming isolated faulty output or verified one-to-one addressing. See [checkpoint](../checkpoints/2026-10-11.md) and [tasks](../tasks.md).
