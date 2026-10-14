#!/usr/bin/env python3
"""
Celestial Engine Mk.V — Real-Time Hardware & AI Pipeline HUD
Monitors Raspberry Pi 5 SoC metrics, NVMe SSD, Ollama memory,
and real-time voice processing pipeline activity with retro sci-fi aesthetics.
"""

import os
import sys
import time
import subprocess
import json
import urllib.request
import re

C_RESET  = "\033[0m"
C_BOLD   = "\033[1m"
C_CYAN   = "\033[96m"
C_GREEN  = "\033[92m"
C_YELLOW = "\033[93m"
C_RED    = "\033[91m"
C_BLUE   = "\033[94m"
C_MAGENTA= "\033[95m"
C_GRAY   = "\033[90m"

ASCII_BANNER = r"""
  ✦ ─── [ CELESTIAL ENGINE Mk.V :: MISSION CONTROL ] ─── ✦
   ___ ___ _    ___ ___ _____ ___ _   _     ___ ___   ___ 
  / __| __| |  | __/ __|_   _|_ _|_\ | |   | _ \_ _| | __|
 | (__| _|| |__| _|\__ \ | |  | |/ _ \| |__ |  _/| |  |__ \
  \___|___|____|___|___/ |_| |___/_/ \_\____|_| |___| |___/
  • PICO 2 W (RP2350)  •  WEARABLE RPi 5  •  OFFLINE AI CORE •
"""

def get_vcgencmd(cmd):
    try:
        res = subprocess.run(["vcgencmd", cmd], capture_output=True, text=True, timeout=1)
        return res.stdout.strip()
    except Exception:
        return "N/A"

def get_soc_temp():
    raw = get_vcgencmd("measure_temp")
    m = re.search(r"temp=([\d\.]+)", raw)
    return float(m.group(1)) if m else 0.0

def get_soc_clock():
    raw = get_vcgencmd("measure_clock arm")
    m = re.search(r"frequency\(\d+\)=(\d+)", raw)
    if m:
        return int(m.group(1)) // 1000000
    return 0

def get_throttled():
    raw = get_vcgencmd("get_throttled")
    m = re.search(r"throttled=(0x[0-9a-fA-F]+)", raw)
    return m.group(1) if m else "0x0"

def get_ram_stats():
    try:
        with open("/proc/meminfo", "r") as f:
            lines = f.readlines()
        mem = {}
        for l in lines:
            parts = l.split(":")
            if len(parts) == 2:
                mem[parts[0].strip()] = int(parts[1].split()[0])
        total_mb = mem.get("MemTotal", 0) // 1024
        avail_mb = mem.get("MemAvailable", 0) // 1024
        used_mb = total_mb - avail_mb
        return used_mb, total_mb
    except Exception:
        return 0, 0

def get_nvme_stats():
    try:
        res = subprocess.run(["df", "-h", "/"], capture_output=True, text=True, timeout=1)
        lines = res.stdout.strip().splitlines()
        if len(lines) >= 2:
            parts = lines[1].split()
            return parts[1], parts[2], parts[3], parts[4]  # Size, Used, Avail, Use%
    except Exception:
        pass
    return "1TB", "N/A", "N/A", "N/A"

def get_ollama_status():
    try:
        req = urllib.request.Request("http://127.0.0.1:11434/api/ps", headers={"User-Agent": "HUD"})
        with urllib.request.urlopen(req, timeout=0.8) as r:
            data = json.loads(r.read().decode())
            models = data.get("models", [])
            if models:
                m = models[0]
                name = m.get("name", "unknown")
                sz_mb = m.get("size", 0) // (1024 * 1024)
                return True, f"{name} ({sz_mb} MB in RAM)"
            return True, "Standby (Models ready on NVMe)"
    except Exception:
        return False, "Ollama service offline"

def get_pipeline_recent_logs():
    try:
        res = subprocess.run(
            ["sudo", "journalctl", "-u", "celestial-brain.service", "-n", "8", "--no-pager", "-o", "cat"],
            capture_output=True, text=True, timeout=1.5
        )
        lines = [l for l in res.stdout.strip().splitlines() if l.strip()]
        return lines[-6:] if lines else []
    except Exception:
        return []

def draw_hud():
    os.system("clear")
    now_str = time.strftime("%Y-%m-%d %H:%M:%S")

    temp = get_soc_temp()
    freq = get_soc_clock()
    throttled = get_throttled()
    used_ram, total_ram = get_ram_stats()
    nvme_sz, nvme_used, nvme_avail, nvme_pct = get_nvme_stats()
    ollama_ok, ollama_info = get_ollama_status()
    logs = get_pipeline_recent_logs()

    # Temperature color
    t_col = C_GREEN if temp < 55 else (C_YELLOW if temp < 70 else C_RED)
    thr_col = C_GREEN if throttled == "0x0" else C_RED
    thr_label = "HEALTHY (0x0)" if throttled == "0x0" else f"THROTTLED ({throttled})"

    # RAM percentage bar
    ram_pct = int((used_ram / total_ram * 100)) if total_ram > 0 else 0
    bar_len = 20
    filled = int(bar_len * (ram_pct / 100.0))
    ram_bar = "#" * filled + "-" * (bar_len - filled)

    # Header ASCII
    print(f"{C_CYAN}{C_BOLD}{ASCII_BANNER}{C_RESET}")
    print(f" {C_GRAY}System Time: {now_str}  |  Host: rpi-15  |  Kernel: Debian Trixie (ARM64){C_RESET}\n")

    print(f"{C_BLUE}{C_BOLD}--- [1] RASPBERRY PI 5 HARDWARE & POWER TELEMETRY ----------------------------{C_RESET}")
    print(f"  • SoC Temperature : {t_col}{temp:.1f} °C{C_RESET}  |  ARM Clock: {C_GREEN}{freq} MHz{C_RESET}")
    print(f"  • Power State     : {thr_col}{thr_label}{C_RESET} (Active Cooler PWM Managed)")
    print(f"  • LPDDR4X Memory  : [{C_CYAN}{ram_bar}{C_RESET}] {ram_pct}% ({used_ram} MB / {total_ram} MB)")
    print(f"  • NVMe Storage    : {C_GREEN}Lexar NM790 1TB (PCIe Gen 3){C_RESET} | Used: {nvme_used}/{nvme_sz} ({nvme_pct})\n")

    print(f"{C_BLUE}{C_BOLD}--- [2] SERVER PARAMETERS & AI ENGINE STACK ----------------------------------{C_RESET}")
    ollama_col = C_GREEN if ollama_ok else C_RED
    print(f"  • Voice Transport : {C_GREEN}UDP Port 5005 (Protocol v2){C_RESET} | 16 kHz 16-bit Mono PCM Duplex")
    print(f"  • AI Persona Vibe : {C_MAGENTA}Hitchhiker's Guide to the Galaxy (Douglas Adams, 42){C_RESET}")
    print(f"  • Local LLM Engine: {ollama_col}● Ollama (127.0.0.1:11434){C_RESET} -> {C_YELLOW}{ollama_info}{C_RESET}")
    print(f"  • Local STT Engine: {C_GREEN}● Whisper.cpp (ARM NEON SIMD){C_RESET} -> Models: ggml-tiny (1.5s), ggml-base")
    print(f"  • Local TTS Engine: {C_GREEN}● Piper C++ Standalone ONNX{C_RESET}   -> Voices: ru_RU-dmitri, en_US-lessac, de_DE-thorsten")
    print(f"  • Cloud Fallback  : {C_GREEN}● OpenRouter Multi-Core{C_RESET}      -> DeepSeek V3, Ministral 8B, Grok 4.5, Llama 70B\n")

    print(f"{C_BLUE}{C_BOLD}--- [3] LIVE VOICE PIPELINE TRANSACTIONS -------------------------------------{C_RESET}")
    if logs:
        for line in logs:
            if "LOCAL STT" in line or "USER INPUT" in line:
                print(f"  {C_CYAN}{line}{C_RESET}")
            elif "LOCAL LLM" in line or "ORACLE RESPONSE" in line:
                print(f"  {C_YELLOW}{line}{C_RESET}")
            elif "TTS" in line or "Delivered" in line:
                print(f"  {C_GREEN}{line}{C_RESET}")
            elif "ERROR" in line or "FAILED" in line:
                print(f"  {C_RED}{line}{C_RESET}")
            else:
                print(f"  {C_GRAY}{line}{C_RESET}")
    else:
        print(f"  {C_GRAY}(Waiting for voice queries from staff...){C_RESET}")

    print(f"\n{C_GRAY}Press Ctrl+C to exit HUD. Run 'hud' in terminal to launch tmux mission control.{C_RESET}")

def main():
    while True:
        try:
            draw_hud()
            time.sleep(1.0)
        except KeyboardInterrupt:
            print("\nExiting HUD.")
            break

if __name__ == "__main__":
    main()
