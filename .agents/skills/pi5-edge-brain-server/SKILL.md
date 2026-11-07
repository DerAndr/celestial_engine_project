---
name: pi5-edge-brain-server
description: >-
  Provides instructions to maintain, configure, and verify the Wearable Raspberry Pi 5 Offline AI Brain Server
  (tools/ai_brain_server.py, tools/local_ai_engine.py, tools/pi5_web_console.py, Moonshine ONNX + Whisper.cpp STT,
  Ollama local LLMs, Kokoro-82M + Piper ONNX TTS, and celestial-brain.service over UDP port 5005 and Web Lab port 8080).
  Use this skill when the user asks to edit or deploy the Raspberry Pi 5 edge server in tools/.
---

# Wearable Raspberry Pi 5 Offline AI Brain Runbook (`tools/`)

## 1. Hardware & Service Profile
- **Compute**: Raspberry Pi 5 (16 GB RAM) + `Lexar NM790` 1 TB NVMe SSD (PCIe Gen 3).
- **Power**: Isolated `Geekworm X1209` 5.1V 5A UPS HAT + 4× `Molicel INR18650-M35A` cells (strictly separate from the staff's 5V USB power bank).
- **Systemd Service**: `celestial-brain.service` listening on binary UDP v2 port `5005` + Interactive Web Test Console (`tools/pi5_web_console.py`) on HTTP port `8080` (`http://192.168.178.51:8080/`).
- **2026 SOTA Offline AI Pipeline**:
  1. **STT**: `Moonshine Tiny INT8 ONNX` (`sherpa-onnx`, variable-length RoPE, ~`120–220 ms` on English audio) with automatic fallback to `Whisper.cpp` (`ggml-tiny.bin`).
  2. **Local LLM (`Ollama`)**: `gemma4:e2b` (default, `"think": False`, `keep_alive: -1`, `num_ctx: 512`), `qwen2.5:0.5b` (~`0.35s`), `qwen2.5:1.5b`, `llama3.2:1b`, `deepseek-r1:1.5b`, `gemma2:2b`.
  3. **Neural TTS**: `Kokoro-82M INT8 ONNX` (`kokoro-onnx`, 4 Cortex-A76 threads, multi-voice persona mapping: `am_adam`, `bm_george`, `am_michael`, `af_bella`, `af_heart`, `bf_emma`) resampled in-memory to `16 kHz` 16-bit mono PCM, with automatic fallback to `Piper C++ ONNX`.

## 2. Parity & Protocol Requirements
1. **6 Oracle Personas Parity**:
   - Keep persona definitions (`machine_god`, `archmage`, `marvin`, `cyberpunk`, `pumpkin_lord`, `custom`) and the `<= 25 words` concise oral response rule in `tools/ai_brain_server.py` / `tools/local_ai_engine.py` aligned with `android/.../OraclePersona.kt` and `docs/twin/oracle/personas.json`.
   - Strip all markdown (`*`, `#`, backticks) and emojis before passing text to `Piper` TTS or the `SH1106` OLED ticker.
2. **Arcane `[SPELL: ...]` Hardware Choreography**:
   - Parse `[SPELL: Name | ANIM: Animation | SOUNDS: Sfx1, Sfx2, ...]` from the LLM response stream, strip the tag from spoken TTS text, and send the corresponding binary UDP LED/SFX commands (`NET_MSG_APP_COMMAND`) to the staff before streaming TTS audio chunks.
3. **Downlink Audio Pacing (`32 ms` per `1024 B`)**:
   - Pace outgoing `1024-byte` (`512 samples` @ `16 kHz`) PCM packets at ~`28–30 ms` intervals so the RP2350's `64 KB` circular ring buffer (`2.04s` capacity) never overflows or underruns.

## 3. Useful Pi 5 Management Commands
```bash
# Check daemon status and live logs on Raspberry Pi 5
sudo systemctl status celestial-brain.service
sudo journalctl -u celestial-brain.service -f

# Restart after updating tools/ai_brain_server.py
sudo systemctl restart celestial-brain.service
```
