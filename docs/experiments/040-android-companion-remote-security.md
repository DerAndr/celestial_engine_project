# Experiment 040 — Secure Network Protocol for Celestial Companion (Android ⟷ Pico 2 W)

**Date**: 2026-10-14  
**Objectives**: 
1. Implement a native mobile companion app for Android 15/16/17 (Pixel 9 Pro XL) with an adaptive Edge-to-Edge interface.
2. Establish a secure bidirectional binary UDP protocol for remote staff control (modes, volume, animations, sound effects, time synchronization).
3. Ensure cryptographic command authentication via a local secret key without risk of leaking credentials to public repositories.

---

## 1. Security Architecture (Zero-Leak Protocol)

### 1.1. Authentication Model
Every remote command (`APP_COMMAND`, `APP_TIME_SYNC`) is authenticated with a 32-bit token:
$$\text{Token} = \text{FNV-1a}(\text{SecretPasskey})$$

- On the microcontroller side (RP2350 Pico 2 W), the secret is compiled from the local `wifi_credentials.h` header, which is strictly tracked in `.gitignore`.
- On the Android side, the passkey is entered by the user in app settings and stored in encrypted app storage (`SharedPreferences`).
- Only `wifi_credentials.h.example` is committed to git history.
- The microcontroller verifies the hash of every incoming command packet. Unauthenticated or compromised packets are dropped immediately and logged:
  `[SECURITY] Rejected unauthorized command from IP:PORT`

### 1.2. Network Transport & Discovery
- **Listening Socket**: Microcontroller binds explicitly to port 5005 (`udp_bind(udp_client_pcb, IP_ADDR_ANY, 5005)`).
- **Discovery Beacon**: Once per second, the staff sends a broadcast `HEARTBEAT` across the subnet (`255.255.255.255:5005`). The smartphone app automatically captures and updates the staff IP in real time.

---

## 2. Verification

1. **Firmware Compilation**:
   - `ninja -C firmware/build celestial_firmware` — successful (0 errors).
   - Firmware flashed to Pico 2 W via `picotool.exe load -f -x`.
2. **Android APK Build**:
   - GitHub Actions workflow `Build Android Companion APK` — successful.
   - Installed to Pixel 9 Pro XL via ADB — smooth launch.
3. **Physical Connectivity**:
   - Ping Pixel 9 Pro XL (`192.168.178.46`) ⟷ Pico 2 W (`192.168.178.101`): 0% packet loss.
   - Microcontroller serial log confirmed authorized command execution:
     - Mode switching (`'M'`)
     - Sound effect playback (`'C'`)
     - Epoch time synchronization (`'X'`)
   - Attempting commands with an invalid passkey token was cleanly rejected by the microcontroller.
4. **43-LED Concentric Headpiece Mirroring**:
   - Native Compose visualizer (`ConcentricLedRingsView`) mirrors the physical 43-LED WS2812B headpiece:
     - Index 0: Center gemstone
     - Indices 1..6: Inner ring (6 LEDs)
     - Indices 7..18: Middle ring (12 LEDs)
     - Indices 19..42: Outer ring (24 LEDs)
   - 33 Hz animation engine faithfully emulates all 18 firmware algorithms (`BREATHE` through `CLOCK_ORRERY`).
   - Bidirectionally synchronized with Pico 2 W via UDP telemetry (`activeLedAnim`).

---

## 3. Status
**PASSED** — secure staff remote control from smartphone is fully operational.
