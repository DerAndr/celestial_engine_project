# Experiment 002 — Bourns PEC11R Rotary Encoder (P2)

Status: COMPLETED (PASSED)

## Goal
Connect the Bourns PEC11R-4225F-S0024 rotary encoder (Channel A, Channel B, Push Switch) to RP2350 (GP6, GP7, GP8), implement quadrature decoding and debouncing, and navigate the 5 Celestial Engine modes on the OLED display.

## Hardware Required
- Raspberry Pi Pico 2 W (on breadboard c1–h20)
- SH1106 1.3" OLED (on breadboard rows 26–29)
- Bourns PEC11R-4225F-S0024 rotary encoder (bare 5-pin component, flying leads)
- 5x Dupont Female-to-Male jumper wires
- Micro-USB cable

## Bourns PEC11R Pin Layout
- **3-pin side (Encoder signals)**:
  - **Pin A** (left): Quadrature Channel A -> GP6 (Pico Pin 9)
  - **Pin C** (center): Common Ground -> GND (Pico Pin 8)
  - **Pin B** (right): Quadrature Channel B -> GP7 (Pico Pin 10)
- **2-pin side (Push switch)**:
  - **Pin SW1**: Push switch contact -> GP8 (Pico Pin 11)
  - **Pin SW2**: Push switch ground -> GND (Pico Pin 13)

## Wiring Table (Confirmed Working)

| Encoder Terminal (на энкодере) | Физический вывод энкодера | Куда на Pico (колонка `b`) | RP2350 GPIO / Функция | Назначение |
| :--- | :--- | :--- | :--- | :--- |
| **Сигнал A** | Сторона 3 ножек: **левая ножка** | **Pin 9** (Ряд 9 слева `b9`) | GPIO 6 (`PIN_ENCODER_A`) | Квадратурный сигнал A (pull-up вкл) |
| **Общий C (GND)** | Сторона 3 ножек: **средняя ножка** | **Pin 8** (Ряд 8 слева `b8`) | Ground (Земля) | Общая земля сигналов A и B |
| **Сигнал B** | Сторона 3 ножек: **правая ножка** | **Pin 10** (Ряд 10 слева `b10`) | GPIO 7 (`PIN_ENCODER_B`) | Квадратурный сигнал B (pull-up вкл) |
| **Кнопка SW1** | Сторона 2 ножек: **ножка 1** | **Pin 11** (Ряд 11 слева `b11`) | GPIO 8 (`PIN_ENCODER_SW`) | Кнопка вала (pull-up вкл, замыкает на GND) |
| **Кнопка SW2** | Сторона 2 ножек: **ножка 2** | **Pin 13** (Ряд 13 слева `b13`) | Ground (Земля) | Земля кнопки вала |

## Tests & Milestones
- [x] Implement quadrature decoder and mode navigation in firmware (`firmware/src/encoder.c`)
- [x] Build firmware `celestial_firmware.uf2` v0.3.0-P2
- [x] Connect PEC11R off-board via 5 Dupont wires directly to Pico pins 8, 9, 10, 11, 13
- [x] Rotating encoder successfully detected (CW / CCW detents updating position and modes)
- [x] Rotating 6 -> 7 activates full-screen '!!! 67 !!!' Easter egg
- [x] Pressing encoder push switch confirms mode on OLED and logs to serial (SW pin verified)

## Result
PASSED — Bourns PEC11R fully operational. Quadrature rotation with Gray decoding, dynamic detent thresholds, debounced push switch, and 5-mode menu navigation with full-screen Easter egg confirmed working by user.
