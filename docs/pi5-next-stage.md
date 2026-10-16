# Next Stage — Server on Raspberry Pi 5

First, confirm the revised pair Pico 1.0.16 + server v2 on the development PC per [036](experiments/036-response-review-and-fullscreen-text.md). The new hardware result is not yet verified. Then migrate **the exact same working server**, retaining its cloud STT/LLM/TTS. Standalone Hotspot and fully local models are separate future stages: avoid changing network, transport, and AI all at once.

The Pi 5 is powered independently from the staff; use the existing Active Cooler. For the initial migration, connect both Pico and Pi to the same home Wi-Fi router. No physical data cable between them is required. Staff GPIO and power remain unchanged.

On Raspberry Pi OS from the repository root:

```bash
sudo apt update
sudo apt install python3-venv ffmpeg flac
python3 -m venv .venv
.venv/bin/python -m pip install -r tools/requirements-brain.txt
hostname -I
.venv/bin/python tools/mock_brain_server.py
```

First test the mock server, then stop it with `Ctrl+C` and start `.venv/bin/python tools/ai_brain_server.py`. Only one server can bind to port 5005 at a time. Copy the existing private server configuration separately; it is excluded from git. Do not publish API keys. The cloud AI requires internet access; standalone local effects and music on Pico do not.

In the local `firmware/src/wifi_credentials.h`, update only `BRAIN_SERVER_IP` to the Pi's IP address, keeping the existing SSID/password. Build `cmake --build firmware/build --target celestial_firmware --parallel 4` on the development workstation and flash the UF2. In SYSTEM mode, the server should indicate READY. Holding QUERY → recording, releasing → PROCESSING → fullscreen text and full response playback → IDLE. Long responses and subsequent queries must not repeat stale audio.

After validation: register the server as a systemd service, configure server discovery/addressing, then discuss mobile hotspot and on-device STT/TTS in isolation. Do not begin Pi battery integration or new soldering purely for software migration.
