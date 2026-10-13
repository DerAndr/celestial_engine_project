# CELESTIAL ENGINE Mk.V

Interactive AI staff with embedded physical controls, 3D concentric WS2812B LED animations, I2S voice input/output, and a wireless companion Raspberry Pi 5 "Wearable Brain" server.

---

## 🧭 System Status Dashboard

| Subsystem | Target Hardware | Current State | Active Baseline / Key Document |
|---|---|---|---|
| **Staff Embedded Core** | Raspberry Pi Pico 2 W (RP2350) | **OPERATIONAL** | 43 LEDs (7→12→24), SH1106 OLED, INMP441 Mic, MAX98357A Amp |
| **Staff Autonomy** | Standalone Offline Oracle | **OPERATIONAL** | Onboard voice bank, mic reactive vortex, retrofuturistic prophecies (EN/DE/RU) |
| **Staff Firmware** | C/C++ (Pico SDK 2.3.1) | **1.0.17 Ready** | Protocol v2, dual-AP auto-failover, standalone oracle, full-screen marquee |
| **AI Brain (Universal)**| Cloud VPS / Docker / Dev PC | **OPERATIONAL** | Host-agnostic Python 3.11, OpenRouter (DeepSeek/Grok), Edge-TTS |
| **Mobile Deployment** | Any Smartphone / 4G Hotspot | **ZERO-APP READY** | Standard Wi-Fi tethering; no APK or phone apps needed |
| **Wearable Brain (Pi 5)**| Raspberry Pi 5 (16 GB, Bookworm 64-bit) | **OPTIONAL / BASELINE** | MicroSD Lite baseline operational (`192.168.178.51`); awaiting Pimoroni PIM699 base |

---

## 📚 Quick Documentation Index

- **Project Vision & Hardware Architecture:** [PROJECT.md](PROJECT.md)
- **Agent Operational Rules & Safety:** [AGENTS.md](AGENTS.md)
- **Hardware Inventory & Parts Matrix:** [docs/hardware/inventory.md](docs/hardware/inventory.md)
- **Global GPIO Pinout:** [docs/wiring/pinout.md](docs/wiring/pinout.md)
- **Tasks & Milestone Backlog:** [docs/tasks.md](docs/tasks.md)
- **Wireless UDP Protocol v2:** [docs/protocol.md](docs/protocol.md)
- **Universal Host-Agnostic Brain Architecture:** [docs/architecture/ai_dual_brain_architecture.md](docs/architecture/ai_dual_brain_architecture.md)
- **Raspberry Pi 5 Server Migration Guide:** [docs/pi5-next-stage.md](docs/pi5-next-stage.md)
- **Interactive System Architecture Map:** [docs/interactive_map.html](docs/interactive_map.html)

### Recent Technical Milestones & Post-Mortems
- [Experiment 037 — RPi 5 NVMe Migration & Diagnostics](docs/experiments/037-rpi5-nvme-stand-alone-migration.md)
- [Technical Post-Mortem: RPi 5 + Lexar NM790 Power Delivery Resolution](docs/hardware/rpi5_nvme_migration_incident_report.md)
- [Experiment 036 — Voice Protocol v2 & Fullscreen Response Text](docs/experiments/036-response-review-and-fullscreen-text.md)

---

## 🛠️ Quick Start & Developer Commands

### 1. Build Pico 2 W Firmware (Windows / PowerShell)
```powershell
cmake --build firmware/build --target celestial_firmware --parallel 4
.\scripts\check-audio.ps1
```
*Outputs: `firmware/build/celestial_firmware.uf2`*

### 2. Run Brain Transport Tests
```powershell
python -m unittest discover -s tests -p "test_*.py" -v
```

### 3. Launch AI Brain Server (PC / Local Dev)
```powershell
python tools/ai_brain_server.py
```

### 4. Deploy Cloud AI Brain Server (Docker / Cloud VPS)
```bash
docker compose -f tools/docker-compose.yml up -d
```

### 5. Optional Deploy on Raspberry Pi 5 (SSH)
```bash
ssh andr@192.168.178.51
cd ~/celestial_engine_project
python3 -m venv .venv
source .venv/bin/activate
pip install -r tools/requirements-brain.txt
python tools/mock_brain_server.py
```
*(See [docs/pi5-next-stage.md](docs/pi5-next-stage.md) for full systemd service setup)*.

