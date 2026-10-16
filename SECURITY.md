# Security & Safety Policy

## Supported Versions

| Version | Status |
|:---|:---|
| v1.0.x (Current) | :white_check_mark: Actively Supported |
| < v1.0.0 | :x: End of Life |

---

## Architecture & Threat Model

Celestial Engine Mk.V consists of physical hardware (embedded microcontroller, wearable single-board computer, addressable LEDs, audio transducers) communicating over a local wireless mesh:

1. **Network Transport Security:**
   - Communications between the staff (Pico 2 W) and server (Raspberry Pi 5 / Android Companion) use a compact binary UDP protocol over port 5005.
   - All packets include an FNV-1a hash integrity checksum (`0xCE1E571A` magic marker) and sequence-based replay protection.
   - The system is designed for isolated local area networks and mobile Wi-Fi hotspots; ports should never be exposed to the public internet without an encrypted tunnel (WireGuard / Tailscale).

2. **Electrical & Physical Safety Guardrails:**
   - **GPIO Voltage Clamp:** Raspberry Pi Pico 2 W (RP2350) GPIO pins are strictly 3.3V tolerant. Feeding 5V logic destroys the I/O pads. All pull-ups and external signals must be verified with a multimeter before connection.
   - **Power Rail Separation:** Staff power (USB 5V / 2A) and Raspberry Pi 5 Wearable power (USB-PD 5V / 5A) must never be cross-connected.
   - **WS2812B LED Power Cap:** The 43-LED concentric array can draw over 2.5 A at 100% white. Firmware enforces an unbreakable **18% global brightness cap** (`0x2E` maximum channel intensity) to prevent thermal overload, voltage sag, and wire overheating.

3. **AI Speech Safety Protocols:**
   - **Default PG-13 Family Safe Mode:** By default, all AI engines (both offline Ollama Gemma/Qwen and cloud models) operate under strict family-friendly system prompts forbidding obscenities, vulgarity, profanity, and harmful content across English, German, and Russian.
   - **Adult Mode (18+):** Can only be activated by physical user voice command (*"Enable adult mode"*), explicitly confirmed by the voice synthesizer, and automatically resets on system restart.

---

## Reporting a Vulnerability

If you discover a security flaw, network vulnerability, or safety hazard in Celestial Engine Mk.V:
1. Do not open a public issue.
2. Email the maintainer directly at **Andrey_Holz@outlook.com** with the subject line `[SECURITY] Celestial Engine Vulnerability`.
3. Provide reproduction steps, affected subsystems, and suggested remediation.
4. Security reports will be acknowledged within 48 hours and resolved with a patch release.
