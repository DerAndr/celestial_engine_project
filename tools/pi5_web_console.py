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
        <label>A. Voice Input (Browser Microphone or On-Board Sample WAV)</label>
        <div class="btn-row">
          <button class="primary" id="micRecordBtn">🎙️ Record Microphone (Click to Start/Stop)</button>
          <button class="secondary" id="sampleWavBtn">▶️ Test On-Device Sample WAV (query.wav)</button>
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
  if (d.spell && d.spell.is_spell) {
    sb.style.display = "block";
    sb.innerHTML = "✨ <b>ARCANE SPELL TRIGGERED:</b> " + d.spell.name +
      " &nbsp;|&nbsp; <b>LED ANIM ID:</b> #" + d.spell.anim_id +
      " &nbsp;|&nbsp; <b>SFX CHAIN:</b> [" + (d.spell.sfx_ids || []).join(", ") + "]";
  } else {
    sb.style.display = "none";
  }

  document.getElementById("responseText").textContent = d.spoken_text || d.raw_answer || "";
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
  }
}

async function runTextQuery(overrideText) {
  const text = overrideText || document.getElementById("promptInput").value.trim();
  if (!text) return;
  const btn = document.getElementById("askTextBtn");
  btn.disabled = true;
  btn.textContent = "⏳ Running on Pi 5...";
  document.getElementById("responseText").textContent = "Synthesizing prophecy & audio on Raspberry Pi 5...";
  try {
    const payload = Object.assign({ text: text }, getSelectedParams());
    const r = await fetch("/api/text-query", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });
    const d = await r.json();
    renderResult(d);
  } catch (e) {
    document.getElementById("responseText").textContent = "Error: " + e;
  } finally {
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
  try {
    const payload = Object.assign({ use_sample_wav: true }, getSelectedParams());
    const r = await fetch("/api/voice-query", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload)
    });
    const d = await r.json();
    renderResult(d);
  } finally {
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

document.getElementById("micRecordBtn").addEventListener("click", async () => {
  const btn = document.getElementById("micRecordBtn");
  if (!isRecording) {
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
    } catch (err) {
      alert("Microphone access requires HTTPS or localhost, or use 'Test On-Device Sample WAV': " + err);
    }
  } else {
    isRecording = false;
    btn.classList.remove("recording");
    btn.textContent = "⏳ Processing Voice...";
    btn.disabled = true;
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
        body: JSON.stringify(payload)
      });
      const d = await r.json();
      renderResult(d);
    } finally {
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


def start_web_console_daemon(port=WEB_PORT):
    """Starts the HTTP test console in a background daemon thread."""
    def _serve():
        try:
            server = http.server.ThreadingHTTPServer(("0.0.0.0", port), Pi5WebHandler)
            print(f"  Web Console  : http://192.168.178.51:{port}/ (Interactive SOTA AI Test Lab)")
            server.serve_forever()
        except Exception as e:
            print(f"  [WEB CONSOLE WARN] Could not bind port {port}: {e}")

    t = threading.Thread(target=_serve, daemon=True)
    t.start()
    return t


if __name__ == "__main__":
    print(f"Starting standalone Pi 5 Web Console on port {WEB_PORT}...")
    server = http.server.ThreadingHTTPServer(("0.0.0.0", WEB_PORT), Pi5WebHandler)
    server.serve_forever()

