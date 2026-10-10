#!/usr/bin/env python3
"""Celestial Engine Mk.V — Mission Control Dashboard & One-Click Flasher.

Usage:
    python tools/dashboard.py
    (Opens http://localhost:5050 automatically in your browser)
"""

import http.server
import json
import os
import queue
import re
import shutil
import subprocess
import sys
import threading
import time
import urllib.parse
import webbrowser
from pathlib import Path

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    serial = None

ROOT_DIR = Path(__file__).resolve().parents[1]
BUILD_DIR = ROOT_DIR / "firmware/build"
UF2_FILE = BUILD_DIR / "celestial_firmware.uf2"
PORT = 5050

# Telemetry and state store
state_lock = threading.Lock()
telemetry = {
    "connected": False,
    "port": None,
    "bootsel_ready": False,
    "bootsel_drive": None,
    "state": "IDLE",
    "mode": "ORACLE",
    "volume": 10,
    "mic_level": 0,
    "oled": "UNKNOWN",
    "query": "UP",
    "last_tick": 0,
    "logs": []
}

log_subscribers = []
serial_ser = None
serial_lock = threading.Lock()


def find_bootsel_drive():
    """Detect mounted RP2350 or RPI-RP2 drive letter on Windows."""
    try:
        ps = "$v = Get-Volume | Where-Object { $_.DriveLetter -and $_.FileSystemLabel -in @('RP2350', 'RPI-RP2') }; if ($v) { $v[0].DriveLetter + ':' }"
        res = subprocess.run(["powershell", "-NoProfile", "-Command", ps], capture_output=True, text=True, timeout=2)
        drive = res.stdout.strip()
        if drive and len(drive) == 2 and drive[1] == ':':
            return drive + "\\"
    except Exception:
        pass
    # Fallback to checking drive letters
    for letter in "DEFGHIJKLMNOPQRSTUVWXYZ":
        vol = f"{letter}:\\"
        if os.path.exists(vol) and (os.path.exists(os.path.join(vol, "INFO_UF2.TXT"))):
            return vol
    return None


def find_pico_serial_port():
    """Find Raspberry Pi Pico USB CDC serial port."""
    if serial:
        for p in serial.tools.list_ports.comports():
            desc = (p.description or "").lower()
            mfg = (p.manufacturer or "").lower()
            if "pico" in desc or "usb serial" in desc or "raspberry" in mfg or "2e8a" in (p.hwid or "").lower():
                return p.device
            # Typical fallback on this system is COM9
            if p.device.upper() == "COM9":
                return p.device
    # Fallback to COM9 if present
    return "COM9"


def broadcast_log(line):
    clean_line = line.strip()
    if not clean_line:
        return
    with state_lock:
        telemetry["logs"].append(clean_line)
        if len(telemetry["logs"]) > 200:
            telemetry["logs"].pop(0)

        # Parse heartbeat telemetry
        # e.g.: [INFO] [HEARTBEAT] Tick #630 | OLED: ONLINE | State: IDLE | Mode: [ORACLE] | Vol: 10% | Mic: 0% | QRY: UP
        if "[HEARTBEAT]" in clean_line:
            m_tick = re.search(r"Tick #(\d+)", clean_line)
            m_oled = re.search(r"OLED:\s*(\w+)", clean_line)
            m_state = re.search(r"State:\s*(\w+)", clean_line)
            m_mode = re.search(r"Mode:\s*\[([^\]]+)\]", clean_line)
            m_vol = re.search(r"Vol:\s*(\d+)%", clean_line)
            m_mic = re.search(r"Mic:\s*(\d+)%", clean_line)
            m_qry = re.search(r"QRY:\s*(\w+)", clean_line)

            if m_tick: telemetry["last_tick"] = int(m_tick.group(1))
            if m_oled: telemetry["oled"] = m_oled.group(1)
            if m_state: telemetry["state"] = m_state.group(1)
            if m_mode: telemetry["mode"] = m_mode.group(1)
            if m_vol: telemetry["volume"] = int(m_vol.group(1))
            if m_mic: telemetry["mic_level"] = int(m_mic.group(1))
            if m_qry: telemetry["query"] = m_qry.group(1)

        elif "[VOLUME]" in clean_line:
            m_vol = re.search(r"(\d+)%", clean_line)
            if m_vol: telemetry["volume"] = int(m_vol.group(1))
        elif "[QUERY]" in clean_line:
            if "LISTENING" in clean_line:
                telemetry["state"] = "LISTENING"
            elif "IDLE" in clean_line:
                telemetry["state"] = "IDLE"

    for q in list(log_subscribers):
        try:
            q.put_nowait(clean_line)
        except Exception:
            pass


def serial_reader_thread():
    global serial_ser
    while True:
        drive = find_bootsel_drive()
        with state_lock:
            telemetry["bootsel_ready"] = bool(drive)
            telemetry["bootsel_drive"] = drive

        port = find_pico_serial_port()
        if not serial:
            time.sleep(2)
            continue

        try:
            with serial_lock:
                if not serial_ser or not serial_ser.is_open:
                    serial_ser = serial.Serial(port, 115200, timeout=1)
                    with state_lock:
                        telemetry["connected"] = True
                        telemetry["port"] = port
                    broadcast_log(f"[SYS] Connected to Pico on {port}")

            while True:
                line = None
                with serial_lock:
                    if serial_ser and serial_ser.is_open:
                        line = serial_ser.readline().decode('utf-8', errors='replace')
                if line:
                    broadcast_log(line)
                else:
                    time.sleep(0.02)
                    drive = find_bootsel_drive()
                    with state_lock:
                        telemetry["bootsel_ready"] = bool(drive)
                        telemetry["bootsel_drive"] = drive
        except Exception:
            with state_lock:
                telemetry["connected"] = False
            with serial_lock:
                if serial_ser:
                    try: serial_ser.close()
                    except Exception: pass
                    serial_ser = None
            time.sleep(1)


def send_serial_cmd(cmd_text):
    global serial_ser
    with serial_lock:
        if serial_ser and serial_ser.is_open:
            serial_ser.write(cmd_text.encode('utf-8'))
            serial_ser.flush()
            return True
    return False


def build_firmware():
    broadcast_log("[BUILD] Compiling firmware via Ninja...")
    res = subprocess.run(["ninja", "-C", str(BUILD_DIR)], cwd=str(ROOT_DIR), capture_output=True, text=True)
    for line in res.stdout.splitlines():
        if line.strip():
            broadcast_log(f"[BUILD] {line.strip()}")
    if res.returncode != 0:
        for line in res.stderr.splitlines():
            if line.strip():
                broadcast_log(f"[ERROR] {line.strip()}")
        return False, res.stderr or "Build failed"
    return True, "Build succeeded"


def flash_firmware():
    ok, msg = build_firmware()
    if not ok:
        return False, msg

    drive = find_bootsel_drive()
    if not drive:
        # Try soft reset into BOOTSEL via serial 'B' command
        broadcast_log("[FLASH] Attempting soft reset into BOOTSEL mode...")
        send_serial_cmd("B\n")
        # Wait up to 5 seconds for drive to appear
        for _ in range(25):
            time.sleep(0.2)
            drive = find_bootsel_drive()
            if drive:
                break

    if not drive:
        return False, "Pico BOOTSEL drive not detected! Please hold BOOTSEL while plugging USB, or click 'Reboot to BOOTSEL'."

    broadcast_log(f"[FLASH] Copying celestial_firmware.uf2 to {drive}...")
    dest = Path(drive) / "celestial_firmware.uf2"
    try:
        shutil.copyfile(str(UF2_FILE), str(dest))
    except OSError as e:
        # Pico reboots and dismounts instantly upon receiving the UF2, which is normal on Windows
        pass
    broadcast_log(f"[FLASH] Firmware flashed successfully to {drive}!")
    return True, f"Flashed to {drive}"


# Embedded HTML dashboard
HTML_PAGE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>CELESTIAL ENGINE Mk.V — Mission Control</title>
<style>
:root {
  --bg: #0b0f19;
  --panel: #131b2e;
  --panel-border: #1e2942;
  --accent: #38bdf8;
  --accent-glow: rgba(56, 189, 248, 0.25);
  --success: #10b981;
  --warning: #f59e0b;
  --danger: #ef4444;
  --text: #f1f5f9;
  --text-dim: #94a3b8;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body {
  background: var(--bg);
  color: var(--text);
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
  min-height: 100vh;
  padding: 24px;
}
.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 24px;
  padding-bottom: 16px;
  border-bottom: 1px solid var(--panel-border);
}
.title-group h1 {
  font-size: 24px;
  font-weight: 700;
  letter-spacing: 0.5px;
  background: linear-gradient(135deg, #38bdf8, #818cf8);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
}
.title-group p {
  font-size: 13px;
  color: var(--text-dim);
  margin-top: 4px;
}
.status-pill {
  display: inline-flex;
  align-items: center;
  gap: 8px;
  padding: 6px 14px;
  border-radius: 9999px;
  font-size: 13px;
  font-weight: 600;
  background: var(--panel);
  border: 1px solid var(--panel-border);
}
.dot {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  background: #64748b;
}
.dot.online { background: var(--success); box-shadow: 0 0 10px var(--success); }
.dot.bootsel { background: #a855f7; box-shadow: 0 0 10px #a855f7; }

.grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  gap: 16px;
  margin-bottom: 24px;
}
.card {
  background: var(--panel);
  border: 1px solid var(--panel-border);
  border-radius: 12px;
  padding: 16px;
  position: relative;
  overflow: hidden;
}
.card h3 {
  font-size: 12px;
  text-transform: uppercase;
  color: var(--text-dim);
  letter-spacing: 0.5px;
  margin-bottom: 8px;
}
.card .val {
  font-size: 26px;
  font-weight: 700;
  color: var(--text);
}
.card .sub {
  font-size: 12px;
  color: var(--text-dim);
  margin-top: 4px;
}

.controls-section {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 16px;
  margin-bottom: 24px;
}
.btn-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(140px, 1fr));
  gap: 10px;
}
button {
  cursor: pointer;
  background: #1e2942;
  color: var(--text);
  border: 1px solid #334155;
  border-radius: 8px;
  padding: 12px 16px;
  font-size: 14px;
  font-weight: 600;
  transition: all 0.15s ease;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
}
button:hover {
  background: #334155;
  border-color: var(--accent);
  box-shadow: 0 0 12px var(--accent-glow);
}
button.primary {
  background: #0284c7;
  border-color: #38bdf8;
  color: #fff;
}
button.primary:hover {
  background: #0369a1;
  box-shadow: 0 0 16px rgba(56, 189, 248, 0.4);
}
button.danger {
  background: #991b1b;
  border-color: #ef4444;
}
button.danger:hover { background: #b91c1c; }

.vol-slider-box {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-top: 12px;
}
input[type="range"] {
  flex: 1;
  accent-color: var(--accent);
}

.console-box {
  background: #050811;
  border: 1px solid var(--panel-border);
  border-radius: 12px;
  padding: 16px;
}
.console-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 12px;
}
.console-header h3 {
  font-size: 13px;
  text-transform: uppercase;
  color: var(--text-dim);
  letter-spacing: 0.5px;
}
#log-terminal {
  height: 280px;
  overflow-y: auto;
  font-family: 'Cascadia Code', 'Consolas', 'Courier New', monospace;
  font-size: 12px;
  line-height: 1.5;
  background: #020409;
  padding: 12px;
  border-radius: 8px;
  border: 1px solid #111827;
}
.log-line { margin-bottom: 2px; white-space: pre-wrap; word-break: break-all; }
.log-INFO { color: #38bdf8; }
.log-HEARTBEAT { color: #64748b; }
.log-WARN { color: var(--warning); }
.log-ERROR { color: var(--danger); font-weight: 600; }
.log-BUILD { color: #a855f7; }
.log-FLASH { color: #10b981; font-weight: 600; }
</style>
</head>
<body>

<div class="header">
  <div class="title-group">
    <h1>CELESTIAL ENGINE Mk.V</h1>
    <p>Target: Raspberry Pi Pico 2 W (RP2350) | Firmware 0.7.1-P4</p>
  </div>
  <div class="status-pill">
    <div id="status-dot" class="dot"></div>
    <span id="status-text">CONNECTING...</span>
  </div>
</div>

<div class="grid">
  <div class="card">
    <h3>Staff State</h3>
    <div id="val-state" class="val">IDLE</div>
    <div id="sub-state" class="sub">Query Button: UP</div>
  </div>
  <div class="card">
    <h3>OLED & Menu Mode</h3>
    <div id="val-mode" class="val">ORACLE</div>
    <div id="sub-oled" class="sub">Display: ONLINE</div>
  </div>
  <div class="card">
    <h3>Master Volume</h3>
    <div id="val-vol" class="val">10%</div>
    <div class="vol-slider-box">
      <input type="range" id="vol-slider" min="0" max="100" value="10" oninput="onVolSlider(this.value)" onchange="sendVol(this.value)">
      <button style="padding: 4px 8px; font-size: 12px;" onclick="sendVol(document.getElementById('vol-slider').value)">Set</button>
    </div>
    <div style="display: flex; gap: 4px; margin-top: 8px; flex-wrap: wrap;">
      <button style="padding: 3px 6px; font-size: 11px;" onclick="setPresetVol(0)">0% Mute</button>
      <button style="padding: 3px 6px; font-size: 11px; background: #0369a1;" onclick="setPresetVol(1)">1% Whisper</button>
      <button style="padding: 3px 6px; font-size: 11px;" onclick="setPresetVol(3)">3% Quiet</button>
      <button style="padding: 3px 6px; font-size: 11px;" onclick="setPresetVol(8)">8% Soft</button>
      <button style="padding: 3px 6px; font-size: 11px;" onclick="setPresetVol(20)">20% Room</button>
    </div>
  </div>
  <div class="card">
    <h3>Microphone Peak</h3>
    <div id="val-mic" class="val">0%</div>
    <div class="sub">INMP441 Live Meter</div>
  </div>
</div>

<div class="controls-section">
  <div class="card">
    <h3>Firmware Actions</h3>
    <div class="btn-grid" style="margin-top: 12px;">
      <button class="primary" onclick="flashFirmware()">⚡ Build & Flash UF2</button>
      <button onclick="rebootBootsel()">🔄 Reboot to BOOTSEL</button>
    </div>
    <div id="flash-status" style="margin-top: 12px; font-size: 12px; color: var(--accent);"></div>
  </div>
  <div class="card">
    <h3>Audio & Visual Testing</h3>
    <div style="margin-top: 8px;">
      <label style="font-size: 11px; color: var(--text-dim); text-transform: uppercase;">8-Bit World Jukebox:</label>
      <div style="display: flex; gap: 6px; margin-top: 4px; margin-bottom: 8px;">
        <select id="track-select" style="flex: 1; background: #0f172a; color: #f8fafc; border: 1px solid #334155; border-radius: 6px; padding: 6px; font-size: 12px;">
          <option value="0">Super Mario Bros</option>
          <option value="1">Tetris Korobeiniki</option>
          <option value="2">Bella Ciao (Italian Folk)</option>
          <option value="3" selected>Ode to Joy (Beethoven Classical)</option>
          <option value="4">Du, du liegst mir im Herzen (German Folk)</option>
          <option value="5">Mo Li Hua / Jasmine (Chinese Folk)</option>
          <option value="6">Greensleeves (Celtic/English)</option>
          <option value="7">Sakura Sakura (Japanese Folk)</option>
          <option value="8">La Cucaracha (Mexican Folk)</option>
          <option value="9">Doom E1M1 Riff</option>
          <option value="10">Zelda Overworld Theme</option>
          <option value="11">Imperial March (Star Wars)</option>
          <option value="12">Mega Man 2 Dr. Wily Stage</option>
        </select>
        <button style="padding: 6px 12px; font-size: 12px;" onclick="playSelectedTrack()">▶ Play</button>
      </div>

      <label style="font-size: 11px; color: var(--text-dim); text-transform: uppercase;">Cosmic Sci-Fi SFX Suite:</label>
      <div style="display: flex; gap: 6px; margin-top: 4px; margin-bottom: 12px;">
        <select id="sfx-select" style="flex: 1; background: #0f172a; color: #f8fafc; border: 1px solid #334155; border-radius: 6px; padding: 6px; font-size: 12px;">
          <option value="0">Warp Drive</option>
          <option value="1">Pulsar Beacon</option>
          <option value="2">Alien Scanner</option>
          <option value="3">Laser Blaster</option>
          <option value="4">Tractor Beam</option>
          <option value="5">Stargate Portal</option>
          <option value="6">Supernova</option>
        </select>
        <button style="padding: 6px 12px; font-size: 12px;" onclick="playSelectedSfx()">🚀 FX</button>
      </div>

      <div class="btn-grid">
        <button class="danger" onclick="sendCmd('S')">⏹️ Stop Audio</button>
        <button onclick="sendCmd('T')">🔔 Play Chime</button>
        <button onclick="sendCmd('D')">🔊 440Hz Sine</button>
        <button onclick="sendCmd('L')">🌈 Cycle LEDs</button>
      </div>
    </div>
  </div>
</div>

<div class="console-box">
  <div class="console-header">
    <h3>Live Serial Telemetry Stream</h3>
    <div>
      <button style="padding: 4px 10px; font-size: 11px;" onclick="clearLogs()">Clear</button>
    </div>
  </div>
  <div id="log-terminal"></div>
</div>

<script>
let term = document.getElementById('log-terminal');

function appendLog(line) {
  let div = document.createElement('div');
  div.className = 'log-line';
  if (line.includes('[ERROR]')) div.className += ' log-ERROR';
  else if (line.includes('[WARN]')) div.className += ' log-WARN';
  else if (line.includes('[HEARTBEAT]')) div.className += ' log-HEARTBEAT';
  else if (line.includes('[BUILD]')) div.className += ' log-BUILD';
  else if (line.includes('[FLASH]')) div.className += ' log-FLASH';
  else div.className += ' log-INFO';
  div.textContent = line;
  term.appendChild(div);
  term.scrollTop = term.scrollHeight;
}

function clearLogs() {
  term.innerHTML = '';
}

function pollStatus() {
  fetch('/api/status')
    .then(r => r.json())
    .then(d => {
      let dot = document.getElementById('status-dot');
      let text = document.getElementById('status-text');
      if (d.bootsel_ready) {
        dot.className = 'dot bootsel';
        text.textContent = 'BOOTSEL READY (' + d.bootsel_drive + ')';
      } else if (d.connected) {
        dot.className = 'dot online';
        text.textContent = 'CONNECTED (' + d.port + ')';
      } else {
        dot.className = 'dot';
        text.textContent = 'DISCONNECTED';
      }
      document.getElementById('val-state').textContent = d.state;
      document.getElementById('sub-state').textContent = 'Query Button: ' + d.query;
      document.getElementById('val-mode').textContent = d.mode;
      document.getElementById('sub-oled').textContent = 'Display: ' + d.oled;
      document.getElementById('val-vol').textContent = d.volume + '%';
      document.getElementById('val-mic').textContent = d.mic_level + '%';
    })
    .catch(console.error);
}

let volDebounce = null;
function onVolSlider(val) {
  document.getElementById('val-vol').textContent = val + '%';
  if (volDebounce) clearTimeout(volDebounce);
  volDebounce = setTimeout(() => {
    sendVol(val);
  }, 120);
}

function setPresetVol(val) {
  document.getElementById('vol-slider').value = val;
  document.getElementById('val-vol').textContent = val + '%';
  sendVol(val);
}

function sendVol(val) {
  fetch('/api/cmd', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({cmd: 'V' + val + '\n'})
  });
}

function playSelectedTrack() {
  let idx = document.getElementById('track-select').value;
  sendCmd('M' + idx);
}

function playSelectedSfx() {
  let idx = document.getElementById('sfx-select').value;
  sendCmd('C' + idx);
}

function sendCmd(char) {
  fetch('/api/cmd', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify({cmd: char + '\n'})
  });
}

function rebootBootsel() {
  sendCmd('B');
  let fs = document.getElementById('flash-status');
  fs.textContent = 'Sent BOOTSEL reboot signal. Waiting for drive...';
}

function flashFirmware() {
  let fs = document.getElementById('flash-status');
  fs.textContent = 'Building & Flashing UF2... Please wait...';
  fetch('/api/flash', {method: 'POST'})
    .then(r => r.json())
    .then(d => {
      fs.textContent = d.success ? ('✅ ' + d.msg) : ('❌ ' + d.msg);
    })
    .catch(err => {
      fs.textContent = '❌ Error: ' + err;
    });
}

// SSE live log stream
function initStream() {
  let es = new EventSource('/api/stream');
  es.onmessage = e => {
    appendLog(e.data);
  };
  es.onerror = () => {
    es.close();
    setTimeout(initStream, 2000);
  };
}

initStream();
setInterval(pollStatus, 800);
pollStatus();
</script>
</body>
</html>
"""


class DashboardHandler(http.server.BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass  # Quiet HTTP server logging

    def do_GET(self):
        url = urllib.parse.urlparse(self.path)
        if url.path == "/" or url.path == "/index.html":
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(HTML_PAGE.encode("utf-8"))

        elif url.path == "/api/status":
            with state_lock:
                data = json.dumps(telemetry)
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(data.encode("utf-8"))

        elif url.path == "/api/stream":
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "keep-alive")
            self.end_headers()

            q = queue.Queue()
            log_subscribers.append(q)

            # Send historical logs first
            with state_lock:
                for line in telemetry["logs"][-30:]:
                    self.wfile.write(f"data: {line}\n\n".encode("utf-8"))
            self.wfile.flush()

            try:
                while True:
                    try:
                        line = q.get(timeout=20)
                        self.wfile.write(f"data: {line}\n\n".encode("utf-8"))
                        self.wfile.flush()
                    except queue.Empty:
                        self.wfile.write(b": ping\n\n")
                        self.wfile.flush()
            except Exception:
                pass
            finally:
                if q in log_subscribers:
                    log_subscribers.remove(q)
        else:
            self.send_error(404)

    def do_POST(self):
        url = urllib.parse.urlparse(self.path)
        length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(length).decode("utf-8") if length > 0 else ""

        if url.path == "/api/flash":
            success, msg = flash_firmware()
            resp = json.dumps({"success": success, "msg": msg})
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(resp.encode("utf-8"))

        elif url.path == "/api/cmd":
            try:
                data = json.loads(body)
                cmd = data.get("cmd", "")
                ok = send_serial_cmd(cmd)
                resp = json.dumps({"success": ok})
            except Exception as e:
                resp = json.dumps({"success": False, "error": str(e)})
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            self.wfile.write(resp.encode("utf-8"))
        else:
            self.send_error(404)


def main():
    print("=" * 60)
    print(" CELESTIAL ENGINE Mk.V — MISSION CONTROL DASHBOARD")
    print("=" * 60)
    print(f" Starting local web dashboard on http://localhost:{PORT} ...")

    # Start background serial listener thread
    t = threading.Thread(target=serial_reader_thread, daemon=True)
    t.start()

    server = http.server.ThreadingHTTPServer(("0.0.0.0", PORT), DashboardHandler)
    print(f" Mission Control is live at: http://localhost:{PORT}")
    print(" Press Ctrl+C to stop.")

    # Auto-open browser
    try:
        webbrowser.open(f"http://localhost:{PORT}")
    except Exception:
        pass

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping Mission Control...")
        server.server_close()


if __name__ == "__main__":
    main()
