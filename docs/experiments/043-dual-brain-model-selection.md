# Experiment 043: Dual-Brain Host Switching & Distilled Qwen / Gemma 4 / DeepSeek-R1 Models

## 1. Goal
Enable staff rotary encoder menu `[04] AI CORE` to dynamically toggle execution hosts (`< PI 5 >`, `< PHONE >`, `< AUTO >`) and select from live model catalogs containing distilled local models (Qwen 2.5 0.5B, Gemma 4 Distilled 2B, DeepSeek-R1 1.5B, Qwen 2.5 1.5B) alongside cloud presets.

## 2. Hardware
- Raspberry Pi Pico 2 W (RP2350)
- SH1106 I2C OLED display (128x64)
- PEC11R Rotary Encoder with Push Switch
- 3 Concentric WS2812B LED rings (43 LEDs)
- INMP441 I2S Microphone
- MAX98357A I2S Amplifier & 3W Speaker
- Raspberry Pi 5 (8GB / 16GB NVMe, IP: 192.168.178.51)
- Google Pixel 9 Pro XL Companion Phone

## 3. Host Switching Architecture
1. **Menu Item 0:** Top row displays `HOST: < AUTO > (STB)`, `HOST: < PI 5 > (PI5)`, or `HOST: < PHONE > (PHN)`.
2. **Switch Click on Host Item:** Cycles preference between Auto Mesh -> Pi 5 Brain -> Phone Brain -> Auto.
3. **Dynamic Catalog Refresh:** Swapping the host immediately swaps the model catalog to the target host's presets.
4. **Model Selection:** Rotating down from row 0 scrolls through the host's models with the OLED vertical scrollbar. Clicking selects and confirms the core, immediately updating the concentric LED rings to the model's signature theme.

## 4. Benchmark Results on Raspberry Pi 5
- `qwen2.5:0.5b`: **3.48 s** (End-to-End Latency, ~40 tok/s)
- `gemma4:e2b`: **10.26 s** (Hitchhiker Oracle Personality)
- `deepseek-r1:1.5b`: **28.72 s** (Full chain-of-thought `<think>` reasoning)
- `qwen2.5:1.5b`: **6.52 s** (Multilingual polyglot)

## 5. Deployment
- **Firmware:** Compiled with CMake/Ninja on RP2350, flashed to Pico 2 W via UF2.
- **Pi 5 Service:** Updated `ai_brain_server.py`, `brain_transport.py`, `local_ai_engine.py` deployed to `/home/andr/celestial_engine_project/tools/` and service restarted (`celestial-brain.service`).
- **Companion App:** `StaffRepository.kt` configured with 4 phone models; pushed to GitHub repository.
