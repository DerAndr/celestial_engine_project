# Experiment 041: 100% Standalone Offline AI on Raspberry Pi 5

**Date:** 2026-10-14  
**Status:** PASSED (Verified End-to-End over UDP Protocol v2)  
**Target Hardware:** Raspberry Pi 5 (16 GB RAM) + Lexar NM790 1 TB NVMe SSD + Pico 2 W  

---

## 1. Objective
Achieve 100% autonomous, offline voice intelligence for Celestial Engine Mk.V directly on the wearable Raspberry Pi 5 without internet access, external API keys, or cloud dependencies. The offline loop must accept live voice audio over UDP from the Pico 2 W staff, transcribe it locally, generate a witty retro-space sci-fi response locally, synthesize neural audio locally, and stream PCM back into the staff MAX98357A speaker with sub-5–9s total latency.

---

## 2. Autonomous Offline AI Architecture

```
[ Pico 2 W Staff ]
   INMP441 Microphone (16 kHz 16-bit Mono PCM)
        │
        │ UDP Protocol v2 (Port 5005)
        ▼
[ Raspberry Pi 5 Wearable Core ]
   1. Local STT: Whisper.cpp with ARM NEON SIMD
      - Binary: /home/andr/local_ai/whisper.cpp/build/bin/whisper-cli
      - Model: ggml-tiny.bin (77 MB) & ggml-base.bin (147 MB)
      - Processing Time: 1.54s – 1.60s (Auto-detects RU, EN, DE)
        │
        ▼
   2. Local LLM: Ollama Engine
      - Models: llama3.2:1b (Q4_0, 1.3 GB) & gemma2:2b (Q4_0, 1.6 GB)
      - Speed: 9.1 – 9.6 tokens/sec (Llama 1B), 7.6 – 7.7 tokens/sec (Gemma 2B)
      - Warm Latency: ~2.1s – 5.6s
      - Persona: Hitchhiker's Guide to the Galaxy / Douglas Adams retro sci-fi
        │
        ▼
   3. Local TTS: Piper Standalone C++ ONNX Engine
      - Binary: /home/andr/local_ai/piper/piper
      - Models: ru_RU-dmitri-medium, en_US-lessac-medium, de_DE-thorsten-medium
      - Synthesis Speed: RTF 0.13 (0.47s for 3.55s audio — 8x faster than real time!)
      - Piped direct to ffmpeg for 16 kHz 16-bit mono PCM stream
        │
        │ UDP Protocol v2 Response Chunks
        ▼
[ Pico 2 W Staff ]
   MAX98357A Class-D Amplifier + Speaker
```

---

## 3. Measured Benchmark Results on Pi 5 (Cortex-A76)

| Pipeline Component | Engine / Model | Resource Footprint | Latency / Throughput |
| :--- | :--- | :--- | :--- |
| **Speech-to-Text (STT)** | `whisper.cpp` (`ggml-tiny.bin`) | 77 MB RAM | **1.54 – 1.60 s** |
| **Speech-to-Text (STT)** | `whisper.cpp` (`ggml-base.bin`) | 147 MB RAM | **3.46 s** |
| **Local LLM (Warm)** | `llama3.2:1b` (Ollama Q4_0) | 1.3 GB RAM | **9.6 tok/s (~2.1 – 5.6 s)** |
| **Local LLM (Warm)** | `gemma2:2b` (Ollama Q4_0) | 1.6 GB RAM | **7.7 tok/s (~3.2 s)** |
| **Text-to-Speech (TTS)** | `piper` (`ru_RU-dmitri-medium`) | ~120 MB RAM | **0.47 s (RTF 0.13)** |
| **Text-to-Speech (TTS)** | `piper` (`en_US-lessac-medium`) | ~120 MB RAM | **0.88 s** |
| **Full Offline Loop** | Whisper Tiny + Llama 1B + Piper | ~1.6 GB total RAM | **~4.5 – 8.9 s total** |

---

## 4. Operational Modes & Seamless Hybrid Fallback

1. **Staff Explicit Selection:**
   - Under staff menu `[05] TARGET AI CORE`:
     - `[0] DEEP THOUGHT` (Cloud DeepSeek V3)
     - `[1] EDDIE COMP` (Cloud Ministral 8B)
     - `[2] GROK 4.5` (Cloud Grok 4.5)
     - `[3] MARVIN 70B` (Cloud Llama 3.3 70B)
     - `[4] PI 5 LOCAL` (**100% Autonomous Pi 5 Core — Whisper + Llama/Gemma + Piper**)
2. **Hybrid Cloud-Offline Fallback:**
   - If the user selects a cloud model (0–3), but the internet drops or OpenRouter / Google STT errors out, the server automatically catches the failure and diverts the query to the local offline core without dropping the transaction or speaking an error.

---

## 5. Artifacts and File Changes
- `firmware/src/net_protocol.h`: Added `AI_MODEL_LOCAL = 4` to `staff_ai_model_id_t`.
- `firmware/src/main.c`: Added `PI 5 LOCAL` / `LOC` badges, menu items, and card displays.
- `tools/local_ai_engine.py`: Standalone offline engine module for Whisper, Ollama, and Piper.
- `tools/ai_brain_server.py`: Integrated `local_ai_engine` as Preset 4 and automatic cloud-drop fallback.
- `firmware/build/celestial_firmware.uf2`: Compiled release binary ready for Pico 2 W.
