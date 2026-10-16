---
name: Bug Report
about: Report a software defect, firmware hang, or regression
title: "[BUG] "
labels: ["bug"]
assignees: ""
---

### System Node Affected
- [ ] Pico 2 W Staff Firmware (RP2350)
- [ ] Wearable AI Brain (Raspberry Pi 5)
- [ ] Android Companion App (Jetpack Compose)
- [ ] Network Transport / Protocol v2 (UDP 5005)

### Description
A clear and concise description of what the bug is.

### Reproduction Steps
1. Power on subsystem: `...`
2. Rotate encoder to mode: `...`
3. Trigger action: `...`
4. Observe failure.

### Expected Behavior
A clear and concise description of what you expected to happen.

### Observable Telemetry & Logs
```text
(Paste serial terminal logs, journalctl -u celestial-brain.service, or logcat output here)
```

### Hardware Environment
- **Staff Core:** Raspberry Pi Pico 2 W (RP2350 ARM Cortex-M33)
- **Wearable SBC:** Raspberry Pi 5 16GB (NVMe Pimoroni Base)
- **Android Device:** (e.g., Pixel 9 Pro XL, Android 15)
- **Firmware Version:** (e.g., v1.0.19)

### Voltage & Safety Checks
- [ ] Logic levels verified to be 3.3V (no 5V on GPIO)
- [ ] WS2812B LED array capped at <= 18% power limit
