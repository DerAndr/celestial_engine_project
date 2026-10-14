#!/usr/bin/env python3
"""
Celestial Engine Mk.V — Compact Real-Time Hardware & Pipeline HUD
Optimized for tmux split panes (adaptive width and height).
Zero-scroll layout: dynamically fits 11 to 15 lines without wrapping or vertical scrolling.
"""

import os
import sys
import time
import subprocess
import json
import urllib.request
import re

# ANSI Color Palette
C_RESET  = "\033[0m"
C_BOLD   = "\033[1m"
C_CYAN   = "\033[96m"
C_GREEN  = "\033[92m"
C_YELLOW = "\033[93m"
C_RED    = "\033[91m"
C_BLUE   = "\033[94m"
C_MAGENTA= "\033[95m"
C_GRAY   = "\033[90m"

ANSI_RE = re.compile(r'\x1b\[[0-9;]*[a-zA-Z]')

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
    return int(m.group(1)) // 1000000 if m else 0

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
            return parts[1], parts[2], parts[4]  # Size, Used, Use%
    except Exception:
        pass
    return "938G", "11G", "2%"

def get_ollama_status():
    try:
        req = urllib.request.Request("http://127.0.0.1:11434/api/ps", headers={"User-Agent": "HUD"})
        with urllib.request.urlopen(req, timeout=0.6) as r:
            data = json.loads(r.read().decode())
            models = data.get("models", [])
            if models:
                m = models[0]
                name = m.get("name", "gemma2:2b")
                sz_mb = m.get("size", 0) // (1024 * 1024)
                return True, f"{name} ({sz_mb}M RAM)"
            # When standby on NVMe, show configured model name
            cfg_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "brain_config.json")
            cfg_model = "gemma2:2b"
            try:
                with open(cfg_path, "r", encoding="utf-8") as f:
                    cfg = json.load(f)
                    cfg_model = cfg.get("local_model", "gemma2:2b")
            except Exception:
                pass
            return True, f"{cfg_model} (NVMe Standby)"
    except Exception:
        return False, "Offline"

def get_adult_mode():
    try:
        cfg_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "brain_config.json")
        with open(cfg_path, "r", encoding="utf-8") as f:
            data = json.load(f)
            return data.get("adult_mode", False)
    except Exception:
        return False

def pad_line(text, width):
    """Pads an inner line so that '│ ' + text + padding + ' │' equals width."""
    plain = ANSI_RE.sub('', text)
    inner_width = width - 4
    if len(plain) > inner_width:
        return f"│ {text[:inner_width]} │"
    padding = " " * (inner_width - len(plain))
    return f"│ {text}{padding} │"

def draw_hud():
    try:
        cols, rows = os.get_terminal_size()
        width = max(58, min(cols - 2, 70))
    except Exception:
        width = 68
        rows = 20

    inner_w = width - 2
    now_str = time.strftime("%H:%M:%S")

    temp = get_soc_temp()
    freq = get_soc_clock()
    throttled = get_throttled()
    used_ram, total_ram = get_ram_stats()
    nvme_sz, nvme_used, nvme_pct = get_nvme_stats()
    ollama_ok, ollama_info = get_ollama_status()
    is_adult = get_adult_mode()
    safety_tag = f"{C_RED}[18+]{C_RESET}" if is_adult else f"{C_GREEN}[PG-13]{C_RESET}"

    # Temperature color
    t_col = C_GREEN if temp < 55 else (C_YELLOW if temp < 70 else C_RED)
    t_status = "[COOL]" if temp < 55 else ("[WARM]" if temp < 70 else "[HOT!]")
    thr_col = C_GREEN if throttled == "0x0" else C_RED
    thr_label = "HEALTHY" if throttled == "0x0" else f"WARN({throttled})"

    # RAM percentage bar (10 segments)
    ram_pct = int((used_ram / total_ram * 100)) if total_ram > 0 else 0
    bar_len = 10
    filled = int(bar_len * (ram_pct / 100.0))
    ram_bar = "#" * filled + "-" * (bar_len - filled)

    ollama_col = C_GREEN if ollama_ok else C_RED

    out = []
    # Cursor home & clear without scrollback inflation
    out.append("\033[H\033[J")

    # Adapt layout based on pane row height
    if rows <= 14:
        # Ultra-compact mode: 11 lines total
        out.append(f"{C_CYAN}{C_BOLD}┌{'─' * inner_w}┐{C_RESET}")
        out.append(pad_line(f"{C_CYAN}{C_BOLD}CELESTIAL ENGINE Mk.V :: MISSION CONTROL{C_RESET} [{C_YELLOW}{now_str}{C_RESET}]", width))
        out.append(f"{C_CYAN}{C_BOLD}├{'─' * inner_w}┤{C_RESET}")
        out.append(pad_line(f"• SoC Temp: {t_col}{temp:4.1f} °C {t_status}{C_RESET}  • ARM Clock: {C_GREEN}{freq} MHz{C_RESET}", width))
        out.append(pad_line(f"• Power: {thr_col}{thr_label}{C_RESET} | RAM: {used_ram/1024:3.1f}/{total_ram/1024:3.1f}G | NVMe: {nvme_used}/{nvme_sz}", width))
        out.append(f"{C_CYAN}{C_BOLD}├{'─' * inner_w}┤{C_RESET}")
        out.append(pad_line(f"• Local LLM : {ollama_col}● Ollama (11434){C_RESET} -> {C_YELLOW}{ollama_info}{C_RESET} {safety_tag}", width))
        out.append(pad_line(f"• Local STT : {C_GREEN}● Whisper.cpp{C_RESET}     -> ARM NEON (tiny/base)", width))
        out.append(pad_line(f"• Local TTS : {C_GREEN}● Piper C++ ONNX{C_RESET}  -> ru_RU, en_US, de_DE", width))
        out.append(pad_line(f"• Voice/Link: {C_GREEN}● UDP 5005 (v2){C_RESET}   • Cloud: {C_GREEN}● OpenRouter{C_RESET}", width))
        out.append(f"{C_CYAN}{C_BOLD}└{'─' * inner_w}┘{C_RESET}")
    else:
        # Full mode: 15 lines total
        out.append(f"{C_CYAN}{C_BOLD}┌{'─' * inner_w}┐{C_RESET}")
        out.append(pad_line(f"{C_CYAN}{C_BOLD}CELESTIAL ENGINE Mk.V :: PI 5 MISSION CONTROL{C_RESET}", width))
        out.append(pad_line(f"{C_GRAY}Host: rpi-15 | Cortex-A76 Quad | Time: {C_YELLOW}{now_str}{C_RESET}", width))
        out.append(f"{C_CYAN}{C_BOLD}├{'─' * inner_w}┤{C_RESET}")
        out.append(pad_line(f"• SoC Temp : {t_col}{temp:4.1f} °C {t_status}{C_RESET}    • ARM Clock : {C_GREEN}{freq} MHz{C_RESET}", width))
        out.append(pad_line(f"• Power    : {thr_col}{thr_label:8s}{C_RESET}        • Cooling   : {C_GREEN}PWM Active Fan{C_RESET}", width))
        out.append(pad_line(f"• Memory   : [{C_CYAN}{ram_bar}{C_RESET}] {ram_pct:2d}% ({used_ram/1024:3.1f} / {total_ram/1024:3.1f} GB)", width))
        out.append(pad_line(f"• NVMe SSD : {C_GREEN}Lexar NM790 1TB{C_RESET} (Gen 3) [{nvme_used}/{nvme_sz} ({nvme_pct})]", width))
        out.append(f"{C_CYAN}{C_BOLD}├{'─' * inner_w}┤{C_RESET}")
        out.append(pad_line(f"• Local LLM : {ollama_col}● Ollama (11434){C_RESET} -> {C_YELLOW}{ollama_info}{C_RESET} {safety_tag}", width))
        out.append(pad_line(f"• Local STT : {C_GREEN}● Whisper.cpp{C_RESET}     -> ARM NEON (tiny/base)", width))
        out.append(pad_line(f"• Local TTS : {C_GREEN}● Piper C++ ONNX{C_RESET}  -> ru_RU, en_US, de_DE", width))
        out.append(pad_line(f"• Cloud AI  : {C_GREEN}● OpenRouter{C_RESET}      -> DeepSeek, Ministral, Grok", width))
        out.append(pad_line(f"• Voice UDP : {C_GREEN}● Port 5005 (v2){C_RESET}  -> Active Duplex Link", width))
        out.append(f"{C_CYAN}{C_BOLD}└{'─' * inner_w}┘{C_RESET}")

    sys.stdout.write("\n".join(out) + "\n")
    sys.stdout.flush()

def main():
    # Hide cursor
    sys.stdout.write("\033[?25l")
    sys.stdout.flush()
    try:
        while True:
            draw_hud()
            time.sleep(1.0)
    except KeyboardInterrupt:
        pass
    finally:
        # Restore cursor
        sys.stdout.write("\033[?25h\n")
        sys.stdout.flush()

if __name__ == "__main__":
    main()
