# Contributing to Celestial Engine Mk.V

Thank you for your interest in contributing to **Celestial Engine Mk.V**! We welcome improvements, bug fixes, celestial animations, and architectural optimizations.

---

## 🧭 Core Philosophy & Rules

1. **English-First Standard:**
   - English is the primary language across the entire codebase: symbol names, function signatures, UI strings, telemetry logs, commit messages, and documentation.
   - Secondary languages (such as German and Russian) are supported through dedicated localization structures and neural voice models, but English is always the authoritative default.

2. **One Observable Milestone at a Time:**
   - Do not implement multiple complex hardware changes in one unverified pass.
   - Verify every step on actual hardware or through the native test harness before moving to the next milestone.

3. **Hardware Safety is Non-Negotiable:**
   - **Never apply 5V logic** to any Pico 2 W GPIO pin. RP2350 I/O pads are 3.3V only.
   - **Never exceed the 18% LED brightness cap** in firmware. The 43-LED headpiece must remain electrically safe and thermally cool on USB power.
   - **Never mix staff power and Raspberry Pi 5 power.** Keep battery supplies and ground references clean.

---

## 🛠️ Development Workflow

### 1. Embedded Firmware (`firmware/`)
- Target: **Raspberry Pi Pico 2 W** (RP2350 dual ARM Cortex-M33).
- Build system: **CMake + Ninja** with official Raspberry Pi Pico SDK 2.3.1+.
- Code style: Modern C99/C11, modular drivers separated from application state logic.
- Keep the central state machine authoritative (`main.c`). Do not create rogue state loops in peripheral drivers.

To build the firmware:
```powershell
cmake --build firmware/build --target celestial_firmware --parallel 4
```

### 2. Android Companion (`android/`)
- Language: **Kotlin 2.0+**
- UI Framework: **Jetpack Compose + Material 3**
- Navigation: Edge-to-edge layout, responsive Canvas visualizers for 43-LED rings and Cosmos star maps.
- Network: Asynchronous UDP socket transport with zero main-thread blocking.

To build the Android app:
```bash
cd android
./gradlew assembleDebug
```

### 3. Server & Tools (`tools/`)
- Target: **Raspberry Pi 5 16GB** (Debian Bookworm aarch64) & Workstation Python 3.11+.
- Audio Pipeline: Whisper.cpp / Google STT -> Ollama / OpenRouter -> Piper C++ / Edge-TTS.
- Keep network protocol minimal, zero-copy where possible, and resilient to packet loss.

---

## 🧪 Testing Guidelines

Before opening a pull request, run all automated checks:

1. **Native C Probes & Firmware Harness:**
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\scripts\check-audio.ps1
   ```
2. **Python Protocol & Regression Suite:**
   ```bash
   python -m unittest discover -s tests -v
   ```
3. **Firmware Clean Compilation:**
   Ensure zero errors and zero warnings when building `celestial_firmware.uf2`.

---

## 📝 Commit Conventions

We follow the Conventional Commits specification:
- `feat:` New user-facing feature or celestial mode (e.g., `feat(firmware): add cosmos zoom transition`)
- `fix:` Bug fix or regression patch (e.g., `fix(oled): prevent model catalog clipping`)
- `docs:` Documentation updates, pinout corrections, or experiment logs
- `refactor:` Code cleanup without changing external behavior
- `test:` Adding or updating regression probes and test harnesses
- `chore:` Dependency updates, CI/CD configuration, or build scripts

---

## 📜 Code of Conduct

Be respectful, collaborative, and constructive. Keep the cosmic spirit alive — and above all: **DON'T PANIC.**
