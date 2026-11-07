#!/usr/bin/env python3
"""
Celestial Engine Mk.V — Raspberry Pi 5 Interactive Web Test Console (English Only)
Provides a browser-based laboratory at http://<pi5-ip>:8080/ to test and benchmark
everything running locally on the Raspberry Pi 5:
- STT: Moonshine ONNX (sherpa-onnx) vs Whisper.cpp
- LLM: Ollama Local Models (gemma4:e2b, qwen2.5:0.5b, qwen2.5:1.5b, llama3.2:1b, deepseek-r1:1.5b, gemma2:2b)
- 6 Oracle Personas (Machine God, Wise Archmage, Marvin Android, Cyberpunk Netrunner, Jack Pumpkin King, Custom)
- Arcane [SPELL: ...] Hardware Choreography parser & fast matcher
- TTS: Kokoro-82M Neural ONNX (multi-voice) vs Piper ONNX, with in-browser audio playback
"""

import base64
import http.server
import io
import json
import os
import shutil
import socket
import ssl
import struct
import subprocess
import sys
import tempfile
import threading
import time
import wave
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import local_ai_engine
import ai_brain_server

WEB_PORT = int(os.environ.get("BRAIN_WEB_PORT", "8080"))
HTTPS_PORT = int(os.environ.get("BRAIN_HTTPS_PORT", "8443"))
TLS_DIR = os.path.join(local_ai_engine.LOCAL_AI_DIR, "tls")
TLS_CERT = os.path.join(TLS_DIR, "pi5_cert.pem")
TLS_KEY = os.path.join(TLS_DIR, "pi5_key.pem")


def ensure_self_signed_cert():
    """Generates a self-signed TLS certificate for HTTPS (:8443) so browser getUserMedia() works over LAN IP."""
    if os.path.exists(TLS_CERT) and os.path.exists(TLS_KEY):
        return True
    try:
        os.makedirs(TLS_DIR, exist_ok=True)
        ips = ai_brain_server.discover_local_ips()
        san_entries = ["DNS:localhost", "DNS:rpi-15", "DNS:rpi-15.local", "IP:127.0.0.1"]
        for ip in ips:
            if ip and not ip.startswith("127."):
                san_entries.append(f"IP:{ip}")
        san_str = ",".join(dict.fromkeys(san_entries))
        cmd = [
            "openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
            "-keyout", TLS_KEY,
            "-out", TLS_CERT,
            "-days", "3650",
            "-subj", "/CN=Celestial-Pi5-Edge-Lab",
            "-addext", f"subjectAltName={san_str}"
        ]
        subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10)
        return os.path.exists(TLS_CERT) and os.path.exists(TLS_KEY)
    except Exception as e:
        print(f"  [WEB TLS WARN] Could not generate self-signed cert: {e}")
        return False


def get_pi5_system_telemetry():
    """Collects live Raspberry Pi 5 CPU temperature, RAM, NVMe disk, and throttling state."""
    cpu_temp_c = None
    throttled = "0x0"
    try:
        if os.path.exists("/sys/class/thermal/thermal_zone0/temp"):
            with open("/sys/class/thermal/thermal_zone0/temp", "r") as f:
                cpu_temp_c = round(int(f.read().strip()) / 1000.0, 1)
    except Exception:
        pass

    try:
        res = subprocess.run(["vcgencmd", "get_throttled"], capture_output=True, text=True, timeout=1.5)
        if res.returncode == 0 and "=" in res.stdout:
            throttled = res.stdout.strip().split("=")[-1]
    except Exception:
        pass

    mem_total_gb = 16.0
    mem_used_gb = 0.0
    try:
        if os.path.exists("/proc/meminfo"):
            info = {}
            with open("/proc/meminfo", "r") as f:
                for line in f:
                    parts = line.split(":")
                    if len(parts) == 2:
                        k = parts[0].strip()
                        v = int(parts[1].strip().split()[0])
                        info[k] = v
            total_kb = info.get("MemTotal", 16 * 1024 * 1024)
            avail_kb = info.get("MemAvailable", total_kb)
            mem_total_gb = round(total_kb / (1024 * 1024), 1)
            mem_used_gb = round((total_kb - avail_kb) / (1024 * 1024), 2)
    except Exception:
        pass

    disk_total_gb = 0.0
    disk_free_gb = 0.0
    try:
        usage = shutil.disk_usage("/")
        disk_total_gb = round(usage.total / (1024 ** 3), 1)
        disk_free_gb = round(usage.free / (1024 ** 3), 1)
    except Exception:
        pass

    cfg = ai_brain_server.load_config()
    capabilities = local_ai_engine.get_engine_capabilities()

    return {
        "hostname": socket.gethostname(),
        "ips": ai_brain_server.discover_local_ips(),
        "udp_port": ai_brain_server.PORT,
        "web_port": WEB_PORT,
        "cpu_temp_c": cpu_temp_c,
        "throttled": throttled,
        "mem_used_gb": mem_used_gb,
        "mem_total_gb": mem_total_gb,
        "disk_free_gb": disk_free_gb,
        "disk_total_gb": disk_total_gb,
        "active_model": cfg.get("local_model", "gemma4:e2b"),
        "active_persona": cfg.get("persona_id", "machine_god"),
        "stt_engine": cfg.get("stt_engine", "auto"),
        "tts_engine": cfg.get("tts_engine", "auto"),
        "kokoro_voice": cfg.get("kokoro_voice", "auto"),
        "capabilities": capabilities,
        "personas": [
            {"id": k, "title": v["title_en"], "prompt": v["en"]}
            for k, v in local_ai_engine.ORACLE_PERSONAS.items()
        ],
        "models": local_ai_engine.AVAILABLE_LOCAL_MODELS,
        "kokoro_voices": local_ai_engine.KOKORO_VOICES_LIST
    }


def pcm16_to_wav_bytes(pcm_bytes, sample_rate=16000):
    buf = io.BytesIO()
    with wave.open(buf, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(pcm_bytes)
    return buf.getvalue()


HTML_CONSOLE = """<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>CELESTIAL ENGINE Mk.V — Raspberry Pi 5 Edge AI Test Lab</title>
<style>
  :root {
    --bg: #070b14;
    --panel: #0f172a;
    --panel-alt: #131e36;
    --border: #1e293b;
    --accent: #38bdf8;
    --accent-2: #a855f7;
    --emerald: #10b981;
    --amber: #f59e0b;
    --rose: #f43f5e;
    --text: #f1f5f9;
    --muted: #94a3b8;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    padding: 0;
    background: radial-gradient(circle at top right, #161f38 0%, var(--bg) 65%);
    color: var(--text);
    font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
    min-height: 100vh;
  }
  header {
    display: flex;
    flex-wrap: wrap;
    justify-content: space-between;
    align-items: center;
    gap: 12px;
    padding: 16px 24px;
    background: rgba(15, 23, 42, 0.9);
    border-bottom: 1px solid var(--border);
    backdrop-filter: blur(10px);
    position: sticky;
    top: 0;
    z-index: 20;
  }
  .brand {
    display: flex;
    align-items: center;
    gap: 12px;
  }
  .brand h1 {
    margin: 0;
    font-size: 1.15rem;
    letter-spacing: 0.06em;
    text-transform: uppercase;
    color: var(--accent);
  }
  .brand span {
    font-size: 0.8rem;
    color: var(--muted);
  }
  .telemetry-strip {
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
  }
  .pill {
    background: var(--panel-alt);
    border: 1px solid var(--border);
    padding: 6px 11px;
    border-radius: 999px;
    font-size: 0.78rem;
    color: var(--muted);
    display: inline-flex;
    align-items: center;
    gap: 6px;
  }
  .pill b { color: var(--text); font-family: 'JetBrains Mono', monospace; }
  .pill.ok { border-color: rgba(16, 185, 129, 0.4); color: #6ee7b7; }
  .container {
    max-width: 1280px;
    margin: 0 auto;
    padding: 20px 24px 48px;
    display: grid;
    grid-template-columns: 380px 1fr;
    gap: 20px;
  }
  @media (max-width: 960px) {
    .container { grid-template-columns: 1fr; padding: 14px; }
  }
  .card {
    background: rgba(15, 23, 42, 0.88);
    border: 1px solid var(--border);
    border-radius: 14px;
    padding: 18px;
    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.35);
  }
  .card h2 {
    margin: 0 0 12px;
    font-size: 0.92rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    color: var(--accent);
    display: flex;
    justify-content: space-between;
    align-items: center;
  }
  .field-group { margin-bottom: 14px; }
  .field-group label {
    display: block;
    font-size: 0.76rem;
    text-transform: uppercase;
    letter-spacing: 0.06em;
    color: var(--muted);
    margin-bottom: 6px;
  }
  select, input[type="text"], textarea {
    width: 100%;
    background: #090e1a;
    color: var(--text);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 9px 11px;
    font-size: 0.88rem;
    font-family: inherit;
  }
  select:focus, input:focus, textarea:focus {
    outline: none;
    border-color: var(--accent);
  }
  .persona-grid {
    display: grid;
    grid-template-columns: repeat(2, 1fr);
    gap: 8px;
  }
  .persona-btn {
    background: #090e1a;
    border: 1px solid var(--border);
    color: var(--text);
    border-radius: 9px;
    padding: 9px 10px;
    text-align: left;
    cursor: pointer;
    transition: all 0.15s ease;
    font-size: 0.82rem;
    font-weight: 600;
  }
  .persona-btn:hover { border-color: var(--accent); }
  .persona-btn.active {
    background: rgba(56, 189, 248, 0.14);
    border-color: var(--accent);
    color: #fff;
    box-shadow: 0 0 14px rgba(56, 189, 248, 0.2);
  }
  .btn-row {
    display: flex;
    flex-wrap: wrap;
    gap: 10px;
    margin-top: 10px;
  }
  button.primary, button.secondary, button.spell-chip {
    cursor: pointer;
    border-radius: 9px;
    font-weight: 600;
    font-size: 0.86rem;
    padding: 10px 16px;
    border: 1px solid transparent;
    transition: all 0.15s ease;
    display: inline-flex;
    align-items: center;
    gap: 7px;
  }
  button.primary {
    background: linear-gradient(135deg, #0284c7, #2563eb);
    color: #fff;
    border-color: #38bdf8;
  }
  button.primary:hover { filter: brightness(1.12); }
  button.primary.recording {
    background: linear-gradient(135deg, #e11d48, #be123c);
    border-color: #fda4af;
    animation: pulse 1.1s infinite;
  }
  button.secondary {
    background: var(--panel-alt);
    color: var(--text);
    border-color: var(--border);
  }
  button.secondary:hover { border-color: var(--accent); }
  .chip-row {
    display: flex;
    flex-wrap: wrap;
    gap: 7px;
    margin-top: 8px;
  }
  button.spell-chip {
    padding: 6px 11px;
    font-size: 0.77rem;
    background: rgba(168, 85, 247, 0.12);
    border-color: rgba(168, 85, 247, 0.38);
    color: #e9d5ff;
  }
  button.spell-chip:hover {
    background: rgba(168, 85, 247, 0.25);
    border-color: #c084fc;
  }
  .metrics-grid {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 10px;
    margin: 14px 0;
  }
  @media (max-width: 680px) {
    .metrics-grid { grid-template-columns: repeat(2, 1fr); }
  }
  .metric-box {
    background: #090e1a;
    border: 1px solid var(--border);
    border-radius: 10px;
    padding: 10px 12px;
  }
  .metric-box .label {
    font-size: 0.7rem;
    text-transform: uppercase;
    color: var(--muted);
  }
  .metric-box .val {
    font-size: 1.05rem;
    font-weight: 700;
    color: var(--accent);
    font-family: 'JetBrains Mono', monospace;
    margin-top: 3px;
  }
  .metric-box .sub {
    font-size: 0.72rem;
    color: var(--muted);
    margin-top: 2px;
  }
  .oracle-output {
    background: #090e1a;
    border: 1px solid var(--border);
    border-radius: 12px;
    padding: 16px;
    margin-top: 12px;
  }
  .spell-banner {
    display: none;
    background: linear-gradient(90deg, rgba(168, 85, 247, 0.2), rgba(56, 189, 248, 0.15));
    border: 1px solid #a855f7;
    border-radius: 9px;
    padding: 10px 13px;
    margin-bottom: 12px;
    font-size: 0.84rem;
    color: #f3e8ff;
  }
  .response-text {
    font-size: 1.12rem;
    line-height: 1.5;
    color: #fff;
    font-weight: 500;
  }
  .transcript-line {
    font-size: 0.84rem;
    color: var(--muted);
    margin-bottom: 8px;
  }
  audio {
    width: 100%;
    margin-top: 12px;
    height: 38px;
  }
  table.bench-table {
    width: 100%;
    border-collapse: collapse;
    margin-top: 12px;
    font-size: 0.83rem;
  }
  table.bench-table th, table.bench-table td {
    border-bottom: 1px solid var(--border);
    padding: 8px 10px;
    text-align: left;
  }
  table.bench-table th { color: var(--muted); font-weight: 600; text-transform: uppercase; font-size: 0.72rem; }
  .badge-sota {
    background: rgba(16, 185, 129, 0.16);
    color: #6ee7b7;
    border: 1px solid rgba(16, 185, 129, 0.4);
    border-radius: 6px;
    padding: 2px 7px;
    font-size: 0.7rem;
    font-weight: 700;
  }
  @keyframes pulse {
    0%, 100% { transform: scale(1); }
    50% { transform: scale(1.03); }
  }
</style>
</head>
<body>
<header>
  <div class="brand">
    <div>
      <h1>🔮 Celestial Engine Mk.V — Pi 5 Edge AI Lab</h1>
      <span>100% Offline Raspberry Pi 5 NVMe Node • English SOTA Pipeline (Moonshine STT + Ollama + Kokoro-82M TTS)</span>
    </div>
  </div>
  <div class="telemetry-strip" id="telemetryStrip">
    <span class="pill ok">● Pi 5 Online</span>
    <span class="pill">Host: <b id="stHost">rpi-15</b></span>
    <span class="pill">CPU: <b id="stTemp">--°C</b></span>
    <span class="pill">RAM: <b id="stRam">-- GB</b></span>
    <span class="pill">NVMe Free: <b id="stDisk">-- GB</b></span>
    <span class="pill">Power: <b id="stPower">0x0</b></span>
  </div>
</header>

<div class="container">
  <!-- LEFT COLUMN: CONFIGURATION & ENGINE SELECTORS -->
  <div style="display:flex; flex-direction:column; gap:18px;">
    <div class="card">
      <h2>1. Oracle Persona (6 Presets)</h2>
      <div class="persona-grid" id="personaGrid">
        <button class="persona-btn active" data-id="machine_god">🤖 Machine God</button>
        <button class="persona-btn" data-id="archmage">🧙‍♂️ Wise Archmage</button>
        <button class="persona-btn" data-id="marvin">🛸 Marvin Android</button>
        <button class="persona-btn" data-id="cyberpunk">🕶️ Cyberpunk AI</button>
        <button class="persona-btn" data-id="pumpkin_lord">🎃 Jack Pumpkin</button>
        <button class="persona-btn" data-id="custom">✏️ Custom Role</button>
      </div>
      <div class="field-group" id="customPromptBox" style="display:none; margin-top:10px;">
        <label>Custom Persona System Prompt (English)</label>
        <textarea id="customPromptInput" rows="2" placeholder="You are an ancient cosmic navigation computer..."></textarea>
      </div>
    </div>

    <div class="card">
      <h2>2. SOTA Offline AI Pipeline</h2>
      <div class="field-group">
        <label>STT Engine (Speech-to-Text)</label>
        <select id="sttSelect">
          <option value="auto">⚡ Auto SOTA (Moonshine ONNX ➔ Whisper.cpp)</option>
          <option value="moonshine">🚀 Moonshine Tiny ONNX (Variable-Length RoPE, Ultra-Fast)</option>
          <option value="whisper">🎙️ Whisper.cpp (ggml-tiny.bin Baseline)</option>
        </select>
      </div>
      <div class="field-group">
        <label>Local LLM Core (Ollama on Pi 5 16GB RAM)</label>
        <select id="modelSelect">
          <option value="gemma4:e2b">🧠 gemma4:e2b — Google Gemma 4 E2B (Default)</option>
          <option value="qwen2.5:0.5b">⚡ qwen2.5:0.5b — Qwen 2.5 0.5B Ultra-Fast (~0.35s)</option>
          <option value="qwen2.5:1.5b">🌐 qwen2.5:1.5b — Qwen 2.5 1.5B Balanced</option>
          <option value="llama3.2:1b">🦙 llama3.2:1b — Meta Llama 3.2 1B Instruct</option>
          <option value="deepseek-r1:1.5b">🔬 deepseek-r1:1.5b — DeepSeek R1 1.5B Distill</option>
          <option value="gemma2:2b">💎 gemma2:2b — Google Gemma 2 2B</option>
        </select>
      </div>
      <div class="field-group">
        <label>TTS Engine (Speech Synthesis)</label>
        <select id="ttsSelect">
          <option value="auto">⚡ Auto SOTA (Kokoro-82M ONNX ➔ Piper ONNX)</option>
          <option value="kokoro">🗣️ Kokoro-82M INT8 ONNX (Studio Natural Voice)</option>
          <option value="piper">🤖 Piper C++ ONNX (en_US-lessac-medium)</option>
        </select>
      </div>
      <div class="field-group">
        <label>Kokoro-82M Voice Profile</label>
        <select id="voiceSelect">
          <option value="auto">🎭 Auto-Match Persona Voice</option>
          <option value="bm_george">🇬🇧 bm_george (British Male — Archmage / Marvin)</option>
          <option value="am_adam">🇺🇸 am_adam (American Male — Machine God)</option>
          <option value="am_michael">🇺🇸 am_michael (Deep Male — Jack Pumpkin)</option>
          <option value="af_heart">🇺🇸 af_heart (Warm Female — Studio Default)</option>
          <option value="bf_emma">🇬🇧 bf_emma (British Female — Oracle)</option>
          <option value="af_bella">🇺🇸 af_bella (Expressive Female — Cyberpunk)</option>
        </select>
      </div>
      <div class="btn-row">
        <button class="secondary" id="saveConfigBtn" style="width:100%; justify-content:center;">💾 Save as Default Pi 5 Brain Config</button>
      </div>
    </div>

    <!-- LIVE 43-LED CONCENTRIC RING & SH1106 OLED DIGITAL TWIN -->
    <div class="card">
      <h2>
        <span>Staff Hardware Preview (43 LEDs & OLED)</span>
        <span class="badge-sota" id="twinStateBadge">IDLE</span>
      </h2>
      <div style="display:flex; flex-direction:column; align-items:center; gap:10px;">
        <canvas id="ledRingCanvas" width="210" height="210" style="background:#050811; border-radius:50%; border:1px solid var(--border); box-shadow:0 0 24px rgba(56,189,248,0.12);"></canvas>
        <canvas id="oledCanvas" width="256" height="104" style="width:100%; max-width:280px; background:#020409; border:2px solid #1e293b; border-radius:8px; image-rendering:pixelated;"></canvas>
      </div>
      <div class="field-group" style="margin-top:10px; margin-bottom:0;">
        <label>Preview LED Animation / Sound FX</label>
        <div style="display:flex; gap:6px;">
          <select id="ledPreviewSelect" style="flex:1; font-size:0.78rem; padding:6px 8px;">
            <option value="0">#0 Cosmic Breathing (Idle)</option>
            <option value="1">#1 Warp Core Vortex</option>
            <option value="4">#4 Rainbow Prism</option>
            <option value="6">#6 Electric Storm</option>
            <option value="7">#7 Supernova Burst</option>
            <option value="10">#10 Solar Flare</option>
            <option value="15">#15 Aegis Shield</option>
            <option value="23">#23 Sailor Moon Prism</option>
            <option value="24">#24 Hogwarts House Runes</option>
            <option value="25">#25 Pokéball Capture Ring</option>
            <option value="26">#26 eBay Chaos Auction</option>
            <option value="28">#28 Ouroboros Spiral Snake</option>
            <option value="29">#29 Radial Shockwave</option>
            <option value="30">#30 Colliding Waves</option>
            <option value="31">#31 Archimedes Spiral</option>
            <option value="32">#32 Sacred Polygon Merkaba</option>
          </select>
          <button class="secondary" id="testSfxBtn" style="padding:6px 10px; font-size:0.76rem;">🔊 SFX</button>
        </div>
      </div>
    </div>
  </div>

  <!-- RIGHT COLUMN: INTERACTIVE VOICE, TEXT, SPELLS & BENCHMARK -->
  <div style="display:flex; flex-direction:column; gap:18px;">
    <div class="card">
      <h2>
        <span>3. Interactive Oracle & Arcane Grimoire Test</span>
        <span class="badge-sota" id="engineStatusBadge">Checking Engines...</span>
      </h2>

      <!-- Voice Recording & Sample WAV -->
      <div class="field-group">
        <label>A. Voice Input (Browser Microphone, Audio File, or On-Board Sample WAV)</label>
        <div id="httpsNotice" style="display:none; background:rgba(245, 158, 11, 0.14); border:1px solid rgba(245, 158, 11, 0.5); border-radius:9px; padding:10px 13px; margin-bottom:10px; font-size:0.82rem; color:#fde68a;">
          ⚠️ <b>Browser Security Note:</b> Live microphone access over LAN IP requires <b>HTTPS</b> or <b>http://localhost:8080/</b>.
          <a id="httpsSwitchLink" href="#" style="color:#38bdf8; font-weight:700; margin-left:6px; text-decoration:underline;">🔒 Switch to HTTPS (:8443) Now ➔</a>
        </div>
        <div class="btn-row">
          <button class="primary" id="micRecordBtn">🎙️ Record Microphone (Click to Start/Stop)</button>
          <button class="secondary" id="uploadAudioBtn">📁 Upload / Capture Audio</button>
          <input type="file" id="audioUploadInput" accept="audio/*" capture style="display:none;">
          <button class="secondary" id="sampleWavBtn">▶️ Test On-Device Sample WAV (query.wav)</button>
          <button class="secondary" id="abortBtn" style="border-color:rgba(244,63,94,0.5); color:#fda4af;">🛑 Stop / Abort</button>
        </div>
      </div>

      <!-- Text Input -->
      <div class="field-group" style="margin-top:14px;">
        <label>B. Direct English Query or Spell Incantation</label>
        <div style="display:flex; gap:10px;">
          <input type="text" id="promptInput" value="What is the probability of our mission succeeding tonight?" placeholder="Ask the Oracle or cast an arcane spell in English...">
          <button class="primary" id="askTextBtn" style="white-space:nowrap;">🔮 Ask Oracle</button>
        </div>
      </div>

      <!-- Quick Spell & Question Chips -->
      <div class="field-group">
        <label>Quick Arcane Spells ([SPELL: ...] Choreography) & Sample Queries</label>
        <div class="chip-row">
          <button class="spell-chip" data-q="Cast Fireball!">🔥 Cast Fireball</button>
          <button class="spell-chip" data-q="Cast Chain Lightning!">⚡ Chain Lightning</button>
          <button class="spell-chip" data-q="Activate Aegis Shield!">🛡️ Aegis Shield</button>
          <button class="spell-chip" data-q="Summon the Ouroboros Serpent!">🐍 Ouroboros</button>
          <button class="spell-chip" data-q="Cast Quantum Quack!">🦆 Quantum Quack</button>
          <button class="spell-chip" data-q="Cast Lumos Maxima!">🪄 Lumos Maxima</button>
          <button class="spell-chip" data-q="Cast Expecto Patronum!">🦅 Expecto Patronum</button>
          <button class="spell-chip" data-q="Start eBay Chaos Auction!">🔨 eBay Bid</button>
          <button class="spell-chip" data-q="Moon Prism Power Make Up!">🌙 Sailor Moon</button>
          <button class="spell-chip" data-q="Give me a short cosmic prophecy about Mars.">🪐 Mars Prophecy</button>
          <button class="spell-chip" data-q="Why is the answer to life, the universe, and everything forty-two?">🌌 Why 42?</button>
        </div>
      </div>

      <!-- Stage Timing Telemetry -->
      <div class="metrics-grid">
        <div class="metric-box">
          <div class="label">1. STT Latency</div>
          <div class="val" id="mStt">-- ms</div>
          <div class="sub" id="mSttSub">Engine: --</div>
        </div>
        <div class="metric-box">
          <div class="label">2. LLM Inference</div>
          <div class="val" id="mLlm">-- ms</div>
          <div class="sub" id="mLlmSub">-- tok/s</div>
        </div>
        <div class="metric-box">
          <div class="label">3. TTS Synthesis</div>
          <div class="val" id="mTts">-- ms</div>
          <div class="sub" id="mTtsSub">RTF: --</div>
        </div>
        <div class="metric-box">
          <div class="label">Total Pipeline</div>
          <div class="val" id="mTotal">-- s</div>
          <div class="sub" id="mAudioDur">Audio: -- s</div>
        </div>
      </div>

      <!-- Oracle Output & Audio Player -->
      <div class="oracle-output">
        <div class="transcript-line" id="transcriptLine">Input Query: <i>Ready for voice or text input...</i></div>
        <div class="spell-banner" id="spellBanner"></div>
        <div class="response-text" id="responseText">Select any Persona, Model, or Spell chip above to test the Raspberry Pi 5 offline brain.</div>
        <audio id="audioPlayer" controls></audio>
      </div>
    </div>

    <!-- Side-by-Side SOTA vs Baseline Benchmark Card -->
    <div class="card">
      <h2>
        <span>4. Raspberry Pi 5 SOTA vs Legacy Benchmark</span>
        <button class="secondary" id="runBenchBtn" style="padding:6px 12px; font-size:0.78rem;">⚡ Run Live Pi 5 Benchmark</button>
      </h2>
      <div style="font-size:0.82rem; color:var(--muted);">
        Compares <b>Moonshine ONNX</b> vs <b>Whisper.cpp</b> (STT) and <b>Kokoro-82M ONNX</b> vs <b>Piper ONNX</b> (TTS) directly on this Raspberry Pi 5.
      </div>
      <table class="bench-table" id="benchTable">
        <thead>
          <tr>
            <th>Stage</th>
            <th>Engine</th>
            <th>Latency</th>
            <th>Speed / RTF</th>
            <th>Output Sample</th>
          </tr>
        </thead>
        <tbody id="benchBody">
          <tr><td colspan="5" style="color:var(--muted);">Click "Run Live Pi 5 Benchmark" to measure hardware performance.</td></tr>
        </tbody>
      </table>
    </div>
  </div>
</div>

<script>
let activePersona = "machine_god";
let isRecording = false;
let mediaRecorder = null;
let audioChunks = [];
let activeAbortCtrl = null;

// --- 43-LED Concentric Ring & SH1106 OLED Digital Twin State ---
let twinState = "IDLE"; // IDLE | LISTENING | THINKING | SPEAKING
let activeLedAnim = 0;
let activeSpellName = "";
let oledTickerText = "READY FOR ORACLE QUERY OR SPELL...";
let oledScrollX = 0;
let sfxAudioCtx = null;

function setTwinState(st, spellTitle, tickerMsg) {
  twinState = st;
  const badge = document.getElementById("twinStateBadge");
  if (badge) badge.textContent = st;
  if (spellTitle !== undefined) activeSpellName = spellTitle;
  if (tickerMsg !== undefined) {
    oledTickerText = tickerMsg.toUpperCase();
    oledScrollX = 0;
  }
}

// Precompute 43-LED polar coordinates (Center 1 + Inner 6 + Mid 12 + Outer 24 = 43)
const LED_COORDS = (() => {
  const pts = [{ x: 0, y: 0, ring: 0, idx: 0, angle: 0, rNorm: 0 }];
  const rings = [
    { count: 6, r: 0.28, ring: 1 },
    { count: 12, r: 0.56, ring: 2 },
    { count: 24, r: 0.85, ring: 3 }
  ];
  rings.forEach(rg => {
    for (let i = 0; i < rg.count; i++) {
      const a = (i / rg.count) * Math.PI * 2 - Math.PI / 2;
      pts.push({ x: Math.cos(a) * rg.r, y: Math.sin(a) * rg.r, ring: rg.ring, idx: i, angle: a, rNorm: rg.r });
    }
  });
  return pts;
})();

function hslToRgb(h, s, l) {
  h = ((h % 360) + 360) % 360;
  const c = (1 - Math.abs(2 * l - 1)) * s;
  const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
  const m = l - c / 2;
  let r = 0, g = 0, b = 0;
  if (h < 60) { r = c; g = x; }
  else if (h < 120) { r = x; g = c; }
  else if (h < 180) { g = c; b = x; }
  else if (h < 240) { g = x; b = c; }
  else if (h < 300) { r = x; b = c; }
  else { r = c; b = x; }
  return [Math.round((r + m) * 255), Math.round((g + m) * 255), Math.round((b + m) * 255)];
}

function computeLedColor(p, t, animId) {
  if (twinState === "LISTENING") {
    const wave = 0.5 + 0.5 * Math.sin(t * 8 - p.ring * 1.1);
    return [Math.round(20 + 220 * wave), Math.round(40 * wave), Math.round(90 * wave)];
  }
  if (twinState === "THINKING") {
    const spin = 0.5 + 0.5 * Math.sin(p.angle * 3 - t * 6 + p.ring);
    return [Math.round(140 * spin), Math.round(60 * spin), Math.round(255 * spin)];
  }
  switch (animId) {
    case 4: // Rainbow Prism
    case 23: // Sailor Moon Prism
      return hslToRgb((p.angle * 180 / Math.PI) + t * 110 - p.ring * 35, 0.95, 0.55);
    case 6: // Electric Storm
      return (Math.sin(t * 16 + p.idx * 2.7 + p.ring * 3.1) > 0.65) ? [220, 245, 255] : [20, 90, 220];
    case 7: // Supernova
    case 10: { // Solar Flare
      const pulse = 0.5 + 0.5 * Math.sin(t * 7 - p.rNorm * 4);
      return [255, Math.round(70 + 150 * pulse), Math.round(20 * (1 - p.rNorm))];
    }
    case 15: { // Aegis Shield
      const sh = 0.45 + 0.55 * Math.cos(p.angle * 4 + t * 4);
      return [Math.round(20 * sh), Math.round(180 * sh), Math.round(255 * sh)];
    }
    case 24: // Hogwarts Gryffindor Gold/Crimson
      return ((p.idx + Math.floor(t * 5)) % 2 === 0) ? [220, 30, 35] : [245, 190, 25];
    case 25: // Pokeball Ring
      if (p.ring === 0) return [255, 255, 255];
      return (Math.sin(p.angle + t * 2) > 0) ? [235, 30, 35] : [240, 240, 248];
    case 26: { // eBay 4-Color Strobe
      const cols = [[229,50,56], [0,100,210], [245,175,2], [134,184,23]];
      return cols[(p.idx + p.ring + Math.floor(t * 8)) % 4];
    }
    case 28: { // Ouroboros Spiral Snake
      const s = 0.5 + 0.5 * Math.sin(p.angle * 2 - t * 5 + p.ring * 1.3);
      return [Math.round(20 * s), Math.round(245 * s), Math.round(130 * s)];
    }
    case 29: // Radial Shockwave
    case 30: { // Colliding Waves
      const w = 0.5 + 0.5 * Math.cos(p.rNorm * 7 - t * 6) * Math.cos(p.angle * 2);
      return [Math.round(56 + 190 * w), Math.round(189 * w), 255];
    }
    case 31: // Archimedes Spiral
    case 32: { // Sacred Polygon
      const m = 0.5 + 0.5 * Math.cos(p.angle * 3 + t * 4 - p.rNorm * 3);
      return [Math.round(180 * m), Math.round(85 + 140 * m), 255];
    }
    default: { // Cosmic Breathing
      const b = 0.35 + 0.65 * (0.5 + 0.5 * Math.sin(t * 2.2 - p.rNorm * 2.0));
      return [Math.round(35 * b), Math.round(165 * b), Math.round(245 * b)];
    }
  }
}

function renderTwinFrame(ts) {
  const t = (ts || 0) * 0.001;
  const ledCv = document.getElementById("ledRingCanvas");
  if (ledCv) {
    const ctx = ledCv.getContext("2d");
    const w = ledCv.width, h = ledCv.height, cx = w / 2, cy = h / 2, maxR = w * 0.44;
    ctx.clearRect(0, 0, w, h);

    // Subtle concentric guide tracks
    [0.28, 0.56, 0.85].forEach(r => {
      ctx.beginPath();
      ctx.arc(cx, cy, maxR * r, 0, Math.PI * 2);
      ctx.strokeStyle = "rgba(56, 189, 248, 0.12)";
      ctx.lineWidth = 1;
      ctx.stroke();
    });

    const boost = (twinState === "SPEAKING") ? (0.85 + 0.3 * Math.sin(t * 18)) : 1.0;
    for (let i = 0; i < LED_COORDS.length; i++) {
      const p = LED_COORDS[i];
      const [r0, g0, b0] = computeLedColor(p, t, activeLedAnim);
      const r = Math.min(255, Math.round(r0 * boost));
      const g = Math.min(255, Math.round(g0 * boost));
      const b = Math.min(255, Math.round(b0 * boost));
      const x = cx + p.x * maxR;
      const y = cy + p.y * maxR;
      const dotR = (p.ring === 0) ? 7.5 : (p.ring === 1 ? 5.5 : 4.6);

      ctx.beginPath();
      ctx.arc(x, y, dotR * 1.9, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(${r},${g},${b},0.22)`;
      ctx.fill();

      ctx.beginPath();
      ctx.arc(x, y, dotR, 0, Math.PI * 2);
      ctx.fillStyle = `rgb(${r},${g},${b})`;
      ctx.fill();
    }
  }

  const oledCv = document.getElementById("oledCanvas");
  if (oledCv) {
    const octx = oledCv.getContext("2d");
    const ow = oledCv.width, oh = oledCv.height;
    octx.fillStyle = "#02050c";
    octx.fillRect(0, 0, ow, oh);

    octx.fillStyle = "#38bdf8";
    octx.font = "bold 11px 'JetBrains Mono', monospace";
    const modelSel = document.getElementById("modelSelect");
    const mName = modelSel ? modelSel.value.toUpperCase() : "GEMMA4:E2B";
    octx.fillText(`PI5 SOTA | ${mName}`, 8, 16);

    octx.strokeStyle = "#1e293b";
    octx.beginPath();
    octx.moveTo(6, 22);
    octx.lineTo(ow - 6, 22);
    octx.stroke();

    octx.fillStyle = "#a855f7";
    octx.font = "bold 11px 'JetBrains Mono', monospace";
    const line2 = activeSpellName ? `SPELL: ${activeSpellName.toUpperCase().slice(0, 20)}` : `ROLE: ${activePersona.toUpperCase()} [${twinState}]`;
    octx.fillText(line2, 8, 40);

    octx.fillStyle = "#e2e8f0";
    octx.font = "12px 'JetBrains Mono', monospace";
    const msg = oledTickerText + "   •   " + oledTickerText;
    oledScrollX = (oledScrollX + 0.9) % (Math.max(180, oledTickerText.length * 7.4));
    octx.save();
    octx.beginPath();
    octx.rect(8, 48, ow - 16, 26);
    octx.clip();
    octx.fillText(msg, 8 - oledScrollX, 66);
    octx.restore();

    octx.fillStyle = "#64748b";
    octx.font = "10px 'JetBrains Mono', monospace";
    octx.fillText(`LED FX #${activeLedAnim}  |  43 WS2812B  |  16KHZ`, 8, 92);
  }
  requestAnimationFrame(renderTwinFrame);
}
requestAnimationFrame(renderTwinFrame);

// Web Audio 8-Bit SFX Synthesizer (14 canonical Staff SFX)
function playSfxById(sfxId) {
  try {
    if (!sfxAudioCtx) sfxAudioCtx = new (window.AudioContext || window.webkitAudioContext)();
    if (sfxAudioCtx.state === "suspended") sfxAudioCtx.resume();
    const now = sfxAudioCtx.currentTime;
    const osc = sfxAudioCtx.createOscillator();
    const gain = sfxAudioCtx.createGain();
    osc.connect(gain);
    gain.connect(sfxAudioCtx.destination);
    gain.gain.setValueAtTime(0.12, now);

    const id = Number(sfxId) || 0;
    if (id === 9) { // Quantum Quack
      osc.type = "sawtooth";
      osc.frequency.setValueAtTime(340, now);
      osc.frequency.exponentialRampToValueAtTime(580, now + 0.09);
      osc.frequency.exponentialRampToValueAtTime(260, now + 0.22);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.24);
      osc.start(now); osc.stop(now + 0.25);
    } else if (id === 10) { // eBay Cash Register Cha-Ching
      osc.type = "square";
      osc.frequency.setValueAtTime(987.77, now);
      osc.frequency.setValueAtTime(1318.51, now + 0.08);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.32);
      osc.start(now); osc.stop(now + 0.34);
    } else if (id === 5 || id === 1) { // Supernova / Plasma Blast
      osc.type = "sawtooth";
      osc.frequency.setValueAtTime(520, now);
      osc.frequency.exponentialRampToValueAtTime(65, now + 0.38);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.40);
      osc.start(now); osc.stop(now + 0.42);
    } else { // Cosmic R2-D2 / Chime arpeggio
      osc.type = "triangle";
      const base = 440 + (id * 55) % 440;
      osc.frequency.setValueAtTime(base, now);
      osc.frequency.setValueAtTime(base * 1.25, now + 0.07);
      osc.frequency.setValueAtTime(base * 1.5, now + 0.14);
      gain.gain.exponentialRampToValueAtTime(0.001, now + 0.28);
      osc.start(now); osc.stop(now + 0.30);
    }
  } catch (e) {}
}

function playSfxChain(sfxIds) {
  if (!Array.isArray(sfxIds) || !sfxIds.length) return;
  sfxIds.slice(0, 3).forEach((id, idx) => {
    setTimeout(() => playSfxById(id), idx * 320);
  });
}

document.getElementById("ledPreviewSelect").addEventListener("change", (e) => {
  activeLedAnim = parseInt(e.target.value, 10) || 0;
  setTwinState("IDLE", "", `PREVIEWING LED ANIMATION #${activeLedAnim}`);
});

document.getElementById("testSfxBtn").addEventListener("click", () => {
  playSfxChain([2, 7, 10]);
});

document.getElementById("abortBtn").addEventListener("click", () => {
  if (activeAbortCtrl) {
    try { activeAbortCtrl.abort(); } catch (e) {}
    activeAbortCtrl = null;
  }
  const player = document.getElementById("audioPlayer");
  if (player) {
    player.pause();
    player.currentTime = 0;
  }
  activeLedAnim = 0;
  setTwinState("IDLE", "", "ABORTED BY USER — READY");
  document.getElementById("responseText").textContent = "🛑 Operation aborted. Staff reset to IDLE.";
});

const audioPlayerEl = document.getElementById("audioPlayer");
audioPlayerEl.addEventListener("play", () => setTwinState("SPEAKING"));
audioPlayerEl.addEventListener("ended", () => setTwinState("IDLE"));
audioPlayerEl.addEventListener("pause", () => { if (twinState === "SPEAKING") setTwinState("IDLE"); });

async function fetchStatus() {
  try {
    const r = await fetch("/api/status");
    const d = await r.json();
    document.getElementById("stHost").textContent = d.hostname + " (" + (d.ips[0] || "") + ")";
    document.getElementById("stTemp").textContent = (d.cpu_temp_c !== null ? d.cpu_temp_c + "°C" : "N/A");
    document.getElementById("stRam").textContent = d.mem_used_gb + " / " + d.mem_total_gb + " GB";
    document.getElementById("stDisk").textContent = d.disk_free_gb + " GB";
    document.getElementById("stPower").textContent = d.throttled;

    const caps = d.capabilities || {};
    const badge = document.getElementById("engineStatusBadge");
    const parts = [];
    parts.push(caps.moonshine_stt ? "Moonshine ONNX ✓" : "Whisper.cpp ✓");
    parts.push(caps.kokoro_tts ? "Kokoro-82M ✓" : "Piper TTS ✓");
    badge.textContent = parts.join(" • ");
  } catch (e) {
    console.warn("Status poll failed:", e);
  }
}

document.querySelectorAll(".persona-btn").forEach(btn => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".persona-btn").forEach(b => b.classList.remove("active"));
    btn.classList.add("active");
    activePersona = btn.dataset.id;
    document.getElementById("customPromptBox").style.display = (activePersona === "custom") ? "block" : "none";
  });
});

document.querySelectorAll(".spell-chip").forEach(chip => {
  chip.addEventListener("click", () => {
    const q = chip.dataset.q;
    document.getElementById("promptInput").value = q;
    runTextQuery(q);
  });
});

function getSelectedParams() {
  return {
    persona_id: activePersona,
    custom_prompt: document.getElementById("customPromptInput").value,
    model: document.getElementById("modelSelect").value,
    stt_engine: document.getElementById("sttSelect").value,
    tts_engine: document.getElementById("ttsSelect").value,
    kokoro_voice: document.getElementById("voiceSelect").value
  };
}

function renderResult(d) {
  document.getElementById("transcriptLine").innerHTML =
    "Input Query: <b>" + (d.user_text || "(silence)") + "</b>";

  const sb = document.getElementById("spellBanner");
  const spoken = d.spoken_text || d.raw_answer || "";
  if (d.spell && d.spell.is_spell) {
    sb.style.display = "block";
    sb.innerHTML = "✨ <b>ARCANE SPELL TRIGGERED:</b> " + d.spell.name +
      " &nbsp;|&nbsp; <b>LED ANIM ID:</b> #" + d.spell.anim_id +
      " &nbsp;|&nbsp; <b>SFX CHAIN:</b> [" + (d.spell.sfx_ids || []).join(", ") + "]";
    activeLedAnim = Number(d.spell.anim_id) || 4;
    setTwinState("SPEAKING", d.spell.name || "ARCANE SPELL", spoken);
    playSfxChain(d.spell.sfx_ids || [7]);
  } else {
    sb.style.display = "none";
    activeLedAnim = 1;
    setTwinState("SPEAKING", "", spoken);
  }

  document.getElementById("responseText").textContent = spoken;
  document.getElementById("mStt").textContent = (d.stt_ms !== undefined ? d.stt_ms + " ms" : "0 ms");
  document.getElementById("mSttSub").textContent = "Engine: " + (d.stt_used || "text");
  document.getElementById("mLlm").textContent = (d.llm_ms || 0) + " ms";
  document.getElementById("mLlmSub").textContent = d.model_used || "";
  document.getElementById("mTts").textContent = (d.tts_ms || 0) + " ms";
  document.getElementById("mTtsSub").textContent = (d.tts_used || "") + " (RTF " + (d.tts_rtf || "--") + ")";
  document.getElementById("mTotal").textContent = ((d.total_ms || 0) / 1000).toFixed(2) + " s";
  document.getElementById("mAudioDur").textContent = "Audio: " + (d.audio_duration_s || 0) + " s";

  if (d.wav_base64) {
    const player = document.getElementById("audioPlayer");
    player.src = "data:audio/wav;base64," + d.wav_base64;
    player.play().catch(() => {});
  } else {
    setTwinState("IDLE");
  }
}

async function runTextQuery(overrideText) {
  const text = overrideText || document.getElementById("promptInput").value.trim();
  if (!text) return;
  const btn = document.getElementById("askTextBtn");
  btn.disabled = true;
  btn.textContent = "⏳ Running on Pi 5...";
  document.getElementById("responseText").textContent = "Synthesizing prophecy & audio on Raspberry Pi 5...";
  setTwinState("THINKING", "", "QUERYING PI 5 OLLAMA & KOKORO-82M...");
  activeAbortCtrl = new AbortController();
  try {
    const payload = Object.assign({ text: text }, getSelectedParams());
    const r = await fetch("/api/text-query", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
      signal: activeAbortCtrl.signal
    });
    const d = await r.json();
    renderResult(d);
  } catch (e) {
    if (e.name !== "AbortError") {
      document.getElementById("responseText").textContent = "Error: " + e;
      setTwinState("IDLE", "", "ERROR DURING QUERY");
    }
  } finally {
    activeAbortCtrl = null;
    btn.disabled = false;
    btn.textContent = "🔮 Ask Oracle";
    fetchStatus();
  }
}

document.getElementById("askTextBtn").addEventListener("click", () => runTextQuery());
document.getElementById("promptInput").addEventListener("keydown", e => {
  if (e.key === "Enter") runTextQuery();
});

document.getElementById("sampleWavBtn").addEventListener("click", async () => {
  const btn = document.getElementById("sampleWavBtn");
  btn.disabled = true;
  btn.textContent = "⏳ Transcribing & Thinking...";
  setTwinState("THINKING", "", "TRANSCRIBING SAMPLE WAV VIA MOONSHINE ONNX...");
  activeAbortCtrl = new AbortController();
  try {
    const payload = Object.assign({ use_sample_wav: true }, getSelectedParams());
    const r = await fetch("/api/voice-query", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
      signal: activeAbortCtrl.signal
    });
    const d = await r.json();
    renderResult(d);
  } catch (e) {
    if (e.name !== "AbortError") setTwinState("IDLE");
  } finally {
    activeAbortCtrl = null;
    btn.disabled = false;
    btn.textContent = "▶️ Test On-Device Sample WAV (query.wav)";
    fetchStatus();
  }
});

// Browser Web Audio API 16 kHz Mono PCM Recorder
let audioCtx = null;
let mediaStream = null;
let processorNode = null;
let recordedPcm16 = [];

const httpsUrl = "https://" + window.location.hostname + ":8443/";
if (!window.isSecureContext || !navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
  const notice = document.getElementById("httpsNotice");
  const link = document.getElementById("httpsSwitchLink");
  if (notice && link) {
    notice.style.display = "block";
    link.href = httpsUrl;
  }
}

document.getElementById("uploadAudioBtn").addEventListener("click", () => {
  document.getElementById("audioUploadInput").click();
});

document.getElementById("audioUploadInput").addEventListener("change", async (ev) => {
  const file = ev.target.files && ev.target.files[0];
  if (!file) return;
  const btn = document.getElementById("uploadAudioBtn");
  btn.disabled = true;
  btn.textContent = "⏳ Decoding & Transcribing...";
  setTwinState("THINKING", "", "DECODING & TRANSCRIBING AUDIO FILE...");
  activeAbortCtrl = new AbortController();
  try {
    const arrayBuf = await file.arrayBuffer();
    const decodeCtx = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 16000 });
    const audioBuf = await decodeCtx.decodeAudioData(arrayBuf);
    const ch = audioBuf.getChannelData(0);
    const pcm16 = new Int16Array(ch.length);
    for (let i = 0; i < ch.length; i++) {
      const s = Math.max(-1, Math.min(1, ch[i]));
      pcm16[i] = s < 0 ? s * 0x8000 : s * 0x7FFF;
    }
    await decodeCtx.close();
    const bytes = new Uint8Array(pcm16.buffer);
    let binary = "";
    for (let i = 0; i < bytes.byteLength; i++) {
      binary += String.fromCharCode(bytes[i]);
    }
    const pcmBase64 = btoa(binary);
    const payload = Object.assign({ pcm_base64: pcmBase64 }, getSelectedParams());
    const r = await fetch("/api/voice-query", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
      signal: activeAbortCtrl.signal
    });
    const d = await r.json();
    renderResult(d);
  } catch (err) {
    if (err.name !== "AbortError") {
      alert("Could not decode audio file: " + err);
      setTwinState("IDLE");
    }
  } finally {
    activeAbortCtrl = null;
    btn.disabled = false;
    btn.textContent = "📁 Upload / Capture Audio";
    ev.target.value = "";
    fetchStatus();
  }
});

document.getElementById("micRecordBtn").addEventListener("click", async () => {
  const btn = document.getElementById("micRecordBtn");
  if (!isRecording) {
    if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
      if (confirm("Browser blocks live microphone on plain HTTP over LAN IP.\n\nClick OK to switch to HTTPS (" + httpsUrl + ") to unlock live microphone recording (accept the self-signed Pi 5 certificate once), or open http://localhost:8080/")) {
        window.location.href = httpsUrl;
      }
      return;
    }
    try {
      mediaStream = await navigator.mediaDevices.getUserMedia({ audio: true });
      audioCtx = new (window.AudioContext || window.webkitAudioContext)({ sampleRate: 16000 });
      const source = audioCtx.createMediaStreamSource(mediaStream);
      processorNode = audioCtx.createScriptProcessor(4096, 1, 1);
      recordedPcm16 = [];
      processorNode.onaudioprocess = (e) => {
        if (!isRecording) return;
        const input = e.inputBuffer.getChannelData(0);
        const buf = new Int16Array(input.length);
        for (let i = 0; i < input.length; i++) {
          const s = Math.max(-1, Math.min(1, input[i]));
          buf[i] = s < 0 ? s * 0x8000 : s * 0x7FFF;
        }
        recordedPcm16.push(buf);
      };
      source.connect(processorNode);
      processorNode.connect(audioCtx.destination);
      isRecording = true;
      btn.classList.add("recording");
      btn.textContent = "🛑 Stop & Send Voice Query";
      setTwinState("LISTENING", "", "CAPTURING 16KHZ MONO PCM VOICE STREAM...");
    } catch (err) {
      alert("Microphone access requires HTTPS or localhost, or use 'Test On-Device Sample WAV': " + err);
    }
  } else {
    isRecording = false;
    btn.classList.remove("recording");
    btn.textContent = "⏳ Processing Voice...";
    btn.disabled = true;
    setTwinState("THINKING", "", "MOONSHINE STT -> OLLAMA -> KOKORO-82M...");
    activeAbortCtrl = new AbortController();
    try {
      if (processorNode) processorNode.disconnect();
      if (mediaStream) mediaStream.getTracks().forEach(t => t.stop());
      if (audioCtx) await audioCtx.close();

      let totalLen = recordedPcm16.reduce((acc, a) => acc + a.length, 0);
      let merged = new Int16Array(totalLen);
      let offset = 0;
      for (const arr of recordedPcm16) {
        merged.set(arr, offset);
        offset += arr.length;
      }
      const bytes = new Uint8Array(merged.buffer);
      let binary = "";
      for (let i = 0; i < bytes.byteLength; i++) {
        binary += String.fromCharCode(bytes[i]);
      }
      const pcmBase64 = btoa(binary);

      const payload = Object.assign({ pcm_base64: pcmBase64 }, getSelectedParams());
      const r = await fetch("/api/voice-query", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
        signal: activeAbortCtrl.signal
      });
      const d = await r.json();
      renderResult(d);
    } catch (e) {
      if (e.name !== "AbortError") setTwinState("IDLE");
    } finally {
      activeAbortCtrl = null;
      btn.disabled = false;
      btn.textContent = "🎙️ Record Microphone (Click to Start/Stop)";
      fetchStatus();
    }
  }
});

document.getElementById("saveConfigBtn").addEventListener("click", async () => {
  const btn = document.getElementById("saveConfigBtn");
  const r = await fetch("/api/save-config", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(getSelectedParams())
  });
  const d = await r.json();
  btn.textContent = d.ok ? "✅ Saved as Default Pi 5 Brain Config!" : "❌ Failed";
  setTimeout(() => { btn.textContent = "💾 Save as Default Pi 5 Brain Config"; }, 2200);
});

document.getElementById("runBenchBtn").addEventListener("click", async () => {
  const btn = document.getElementById("runBenchBtn");
  const tbody = document.getElementById("benchBody");
  btn.disabled = true;
  btn.textContent = "⏳ Benchmarking on Pi 5...";
  tbody.innerHTML = "<tr><td colspan='5'>Running STT & TTS benchmarks on Raspberry Pi 5 CPU...</td></tr>";
  try {
    const r = await fetch("/api/benchmark", { method: "POST" });
    const d = await r.json();
    tbody.innerHTML = (d.rows || []).map(row => `
      <tr>
        <td><b>${row.stage}</b></td>
        <td>${row.engine} ${row.sota ? '<span class="badge-sota">SOTA</span>' : ''}</td>
        <td style="font-family:monospace; color:var(--accent); font-weight:700;">${row.latency_ms} ms</td>
        <td style="font-family:monospace;">${row.metric}</td>
        <td style="color:var(--muted);">${row.sample}</td>
      </tr>
    `).join("");
  } finally {
    btn.disabled = false;
    btn.textContent = "⚡ Run Live Pi 5 Benchmark";
  }
});

fetchStatus();
setInterval(fetchStatus, 5000);
</script>
</body>
</html>
"""


class Pi5WebHandler(http.server.BaseHTTPRequestHandler):
    def log_message(self, format, *args):
        pass  # Keep systemd journal clean

    def _send_json(self, data, status=200):
        raw = json.dumps(data, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(raw)

    def do_GET(self):
        if self.path == "/" or self.path.startswith("/index.html"):
            raw = HTML_CONSOLE.encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)
        elif self.path.startswith("/api/status"):
            self._send_json(get_pi5_system_telemetry())
        else:
            self.send_error(404)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length) if length > 0 else b"{}"
        try:
            req = json.loads(body.decode("utf-8"))
        except Exception:
            req = {}

        if self.path == "/api/save-config":
            cfg = ai_brain_server.load_config()
            if req.get("persona_id"):
                cfg["persona_id"] = req["persona_id"]
            if "custom_prompt" in req:
                cfg["custom_persona_prompt"] = req["custom_prompt"]
            if req.get("model"):
                cfg["local_model"] = req["model"]
            if req.get("stt_engine"):
                cfg["stt_engine"] = req["stt_engine"]
            if req.get("tts_engine"):
                cfg["tts_engine"] = req["tts_engine"]
            if req.get("kokoro_voice"):
                cfg["kokoro_voice"] = req["kokoro_voice"]
            ai_brain_server.save_config(cfg)
            self._send_json({"ok": True})
            return

        if self.path == "/api/text-query":
            t_start = time.time()
            user_text = (req.get("text") or "Hello Oracle").strip()
            persona_id = req.get("persona_id") or "machine_god"
            custom_prompt = req.get("custom_prompt") or ""
            model = req.get("model") or "gemma4:e2b"
            tts_engine = req.get("tts_engine") or "auto"
            kokoro_voice = req.get("kokoro_voice") or "auto"

            matched_spell = ai_brain_server.match_fast_spell(user_text)

            t_llm0 = time.time()
            raw_answer = local_ai_engine.local_query_llm(
                user_text, "en-US", model=model,
                persona_id=persona_id, custom_prompt=custom_prompt,
                matched_spell=matched_spell
            )
            llm_ms = int((time.time() - t_llm0) * 1000)

            parsed_spell = ai_brain_server.parse_spell_tag(raw_answer, fallback_spell=matched_spell)
            spoken_text = parsed_spell["spoken_text"] or raw_answer

            t_tts0 = time.time()
            pcm, tts_used = local_ai_engine.local_synthesize_pcm_with_meta(
                spoken_text, "en-US",
                engine_pref=tts_engine,
                persona_id=persona_id,
                voice_override=kokoro_voice
            )
            tts_ms = int((time.time() - t_tts0) * 1000)
            audio_dur = round(len(pcm) / 32000.0, 2)
            tts_rtf = round((tts_ms / 1000.0) / max(audio_dur, 0.01), 2)
            wav_b64 = base64.b64encode(pcm16_to_wav_bytes(pcm, 16000)).decode("ascii") if pcm else ""

            self._send_json({
                "user_text": user_text,
                "raw_answer": raw_answer,
                "spoken_text": spoken_text,
                "spell": parsed_spell,
                "stt_ms": 0,
                "stt_used": "Direct Text",
                "llm_ms": llm_ms,
                "model_used": model,
                "tts_ms": tts_ms,
                "tts_used": tts_used,
                "tts_rtf": tts_rtf,
                "audio_duration_s": audio_dur,
                "total_ms": int((time.time() - t_start) * 1000),
                "wav_base64": wav_b64
            })
            return

        if self.path == "/api/voice-query":
            t_start = time.time()
            persona_id = req.get("persona_id") or "machine_god"
            custom_prompt = req.get("custom_prompt") or ""
            model = req.get("model") or "gemma4:e2b"
            stt_engine = req.get("stt_engine") or "auto"
            tts_engine = req.get("tts_engine") or "auto"
            kokoro_voice = req.get("kokoro_voice") or "auto"

            with tempfile.TemporaryDirectory(prefix="pi5-web-voice-") as tmpdir:
                if req.get("use_sample_wav"):
                    wav_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "query.wav")
                else:
                    pcm_raw = base64.b64decode(req.get("pcm_base64") or "")
                    cleaned = ai_brain_server.clean_pcm_samples(pcm_raw)
                    wav_path = os.path.join(tmpdir, "mic.wav")
                    with wave.open(wav_path, "wb") as wf:
                        wf.setnchannels(1)
                        wf.setsampwidth(2)
                        wf.setframerate(16000)
                        wf.writeframes(cleaned)

                t_stt0 = time.time()
                user_text, detected_lang, stt_used = local_ai_engine.local_transcribe_with_meta(
                    wav_path, lang_id=0, engine_pref=stt_engine
                )
                stt_ms = int((time.time() - t_stt0) * 1000)

            user_text = user_text or "What is this?"
            matched_spell = ai_brain_server.match_fast_spell(user_text)

            t_llm0 = time.time()
            raw_answer = local_ai_engine.local_query_llm(
                user_text, "en-US", model=model,
                persona_id=persona_id, custom_prompt=custom_prompt,
                matched_spell=matched_spell
            )
            llm_ms = int((time.time() - t_llm0) * 1000)

            parsed_spell = ai_brain_server.parse_spell_tag(raw_answer, fallback_spell=matched_spell)
            spoken_text = parsed_spell["spoken_text"] or raw_answer

            t_tts0 = time.time()
            pcm, tts_used = local_ai_engine.local_synthesize_pcm_with_meta(
                spoken_text, "en-US",
                engine_pref=tts_engine,
                persona_id=persona_id,
                voice_override=kokoro_voice
            )
            tts_ms = int((time.time() - t_tts0) * 1000)
            audio_dur = round(len(pcm) / 32000.0, 2)
            tts_rtf = round((tts_ms / 1000.0) / max(audio_dur, 0.01), 2)
            wav_b64 = base64.b64encode(pcm16_to_wav_bytes(pcm, 16000)).decode("ascii") if pcm else ""

            self._send_json({
                "user_text": user_text,
                "raw_answer": raw_answer,
                "spoken_text": spoken_text,
                "spell": parsed_spell,
                "stt_ms": stt_ms,
                "stt_used": stt_used,
                "llm_ms": llm_ms,
                "model_used": model,
                "tts_ms": tts_ms,
                "tts_used": tts_used,
                "tts_rtf": tts_rtf,
                "audio_duration_s": audio_dur,
                "total_ms": int((time.time() - t_start) * 1000),
                "wav_base64": wav_b64
            })
            return

        if self.path == "/api/benchmark":
            rows = local_ai_engine.run_sota_benchmark()
            self._send_json({"rows": rows})
            return

        self.send_error(404)


def start_web_console_daemon(port=WEB_PORT, https_port=HTTPS_PORT):
    """Starts both HTTP (:8080) and HTTPS (:8443, for browser microphone access) in background daemon threads."""
    def _serve_http():
        try:
            server = http.server.ThreadingHTTPServer(("0.0.0.0", port), Pi5WebHandler)
            print(f"  Web Console  : http://192.168.178.51:{port}/ (HTTP Lab)")
            server.serve_forever()
        except Exception as e:
            print(f"  [WEB CONSOLE WARN] Could not bind HTTP port {port}: {e}")

    def _serve_https():
        if not ensure_self_signed_cert():
            return
        try:
            server = http.server.ThreadingHTTPServer(("0.0.0.0", https_port), Pi5WebHandler)
            ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            ctx.load_cert_chain(certfile=TLS_CERT, keyfile=TLS_KEY)
            server.socket = ctx.wrap_socket(server.socket, server_side=True)
            print(f"  Web HTTPS Mic: https://192.168.178.51:{https_port}/ (HTTPS Live Microphone Enabled)")
            server.serve_forever()
        except Exception as e:
            print(f"  [WEB HTTPS WARN] Could not bind HTTPS port {https_port}: {e}")

    t1 = threading.Thread(target=_serve_http, daemon=True)
    t1.start()
    t2 = threading.Thread(target=_serve_https, daemon=True)
    t2.start()
    return t1


if __name__ == "__main__":
    print(f"Starting standalone Pi 5 Web Console on port {WEB_PORT}...")
    server = http.server.ThreadingHTTPServer(("0.0.0.0", WEB_PORT), Pi5WebHandler)
    server.serve_forever()

