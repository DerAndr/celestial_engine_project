# Hardware Inventory

Update status as parts arrive and are tested.

Current main software: **1.0.16-RESPONSE-FIX**, standard celestial_firmware.uf2. Current profile uses 18% brightness cap, PIO-paced DMA on 43 LEDs, and 35% audio volume. User physically removed the 4th 16-LED ring and discarded due to instability/flickering. The 3 verified concentric rings (7 + 12 + 24 = 43 LEDs) form the active baseline.

Latest observation (2026-10-11): 4th ring removed by user. 3-ring array confirmed operational. Audio playback and streaming verified over Wi-Fi UDP.

User-reported amplifier marking: `MAX98357A I2S MONO AMP`, `VIN 2.5-5.V`, `DEFAULT GAIN 9DB`, `L+R / 2 OUT`. Module is marked for I2S with 9 dB default gain and stereo-average mono output; exact breakout manufacturer/photo remain unavailable. This is identification evidence, not a new audio pass.

Status values: `ORDERED`, `RECEIVED`, `TESTED`, `INSTALLED`, `FAILED`, `REMOVED`; the 16-LED ring is marked REMOVED.

| Component | Qty | Status | Notes |
|---|---:|---|---|
| Raspberry Pi Pico 2 W with headers | 1 | TESTED | RP2350; Wi-Fi/Bluetooth; P0 validated on COM9 |
| SH1106 1.3in 128x64 I2C OLED | 1 | TESTED | P1 validated on I2C1 (0x3C); clean display telemetry |
| Bourns PEC11R-4225F-S0024 encoder | 1 | TESTED | P2 validated on GP6/7/8. PEC11R quadrature decoding with smooth card navigation |
| Prototype momentary NO QUERY button | 1 | TESTED | P3 validated; Push-to-Talk active-LOW on GP9 with up to 60s stream |
| INMP441 I2S microphone | 1 | TESTED | P6 live metering/voice reaction; streaming to Brain server at 16 kHz |
| MAX98357A I2S amplifier | 3 | TESTED | One unit revalidated after user solder repair; GAIN unconnected. Other units not individually verified |
| QUARKZMAN 4 ohm / 3 W mini speaker, JST-PH2.0 | 2 | TESTED | One working speaker after solder repair; listing 44 mm. Second speaker not individually verified |
| WS2812B 7-LED ring | 1 | TESTED | Core ring (index 0..6); center amber ember + 6 inner orbiting sparks |
| WS2812B 12-LED ring | 1 | TESTED | Inner ring (index 7..18); OD 37 / ID 23 mm; working in 7→12→24 chain |
| WS2812B 16-LED ring | 1 | REMOVED BY USER | Physically removed from assembly by user due to instability/flashes. Firmly excluded from firmware 1.0.15 |
| WS2812B 24-LED ring | 1 | TESTED | Outer ring (index 19..42); OD 66 / ID 52 mm; outer horizon of 43-LED chain |
| SN74AHCT125N | 2 | RECEIVED | WS2812 3.3V->5.0V level shifting DIP-14 IC |
| ~470 ohm resistor | TBD | RECEIVED | WS2812 data series protection resistor |
| ~1000 uF >=10 V capacitor | TBD | RECEIVED | LED power bulk decoupling capacitance |
| Velleman VTBB3 breadboard | 1 | RECEIVED | 400 holes (30 rows, a-j, +/- rails) |
| 22 AWG red/black wire | 1 set | RECEIVED | Power distribution |
| 26 AWG multicolor wire | 1 set | RECEIVED | Signals |
| Raspberry Pi 5 | 1 | TESTED | 16 GB RAM; wearable brain; verified on MicroSD Lite 64-bit |
| Raspberry Pi Active Cooler | 1 | TESTED | Operational on Pi 5 |
| Lexar NM790 1 TB NVMe SSD | 1 | TESTED | M.2 2280 PCIe 4.0; 100% healthy (0 media errors, 453 MB/s read); awaiting PIM699 |
| Geekworm X1002 V1.1 NVMe HAT | 1 | FAILED / RETIRED | Power delivery brownout on high-current write bursts (D3cold lockup); retired |
| Geekworm P580-V2 metal case | 1 | RETIRED | Form factor tied to X1002; retired |
| Pimoroni PIM699 (NVMe Base) | 1 | PLANNED / ORDERED | High-capacity power delivery NVMe base; certified for Lexar NM790 |
| Geekworm X1209 V1.0 | 1 | ORDERED | Pi 5 portable power only |
| Geekworm X12-A1 + X12-A3 | 1 | ORDERED | Pi 5 portable power only |
| Molicel INR18650-M35A 3500 mAh flat-top | 4 | ORDERED | Pi 5 portable power only |

2026-10-11 checkpoint reconciles arrival/status with subsequent photos and user-confirmed operation: 12/24 rings are present and working, and the user repaired the 16-position ring with a soldered jumper around one failed LED. This records the observed repair and light response, without claiming isolated faulty output or verified one-to-one addressing. See [checkpoint](../checkpoints/2026-10-11.md) and [tasks](../tasks.md).
