# Experiment 030 — Utilitarian UI, Derpy Logo, Cosmic Scale Zoom, and 9-Second Speech Buffer

## Date: 2026-10-11
## Status: Verified & Flashed

## Objectives
1. Eliminate speech cut-off: expand response audio buffer from 80,000 samples (5.0s) to 144,000 samples (9.0s).
2. Simplify OLED menu from cryptic arcane jargon into a clean, utilitarian interface.
3. Add a retro derpy celestial star-orb logo on the main status card.
4. Implement a 3-stage cosmic zoom visualization:
   - `[02] MILKY WAY`: Spiral galaxy overview, Sagittarius A* core, spiral arms, and reticle pointing to the Sun in the Orion Spur (`SOL [HERE]`, `R: 26.4 kly`).
   - `[03] SOL SYSTEM`: Heliocentric orbital view of Sun, Mercury, Venus, Earth with orbiting Moon, Mars, with pointer to `[TERRA / SOL-3]`.
   - `[04] EARTH ZOOM`: Close-up rotating globe of Earth (Terra) with latitude grid, equator, orbiting Moon, and targeting reticle.
5. Clarify OpenRouter configuration in `tools/brain_config.json` and `tools/ai_brain_server.py`.

## Firmware Changes
- `firmware/src/mic.h`: Reduced `MIC_MAX_RECORD_SECONDS` to 2.0s (64 KB buffer) to free up 96 KB of RP2350 SRAM.
- `firmware/src/net_client.c`: Expanded `RESP_BUFFER_MAX_SAMPLES` to 144,000 (9.0s of 16 kHz audio = 288 KB).
- `firmware/src/main.c`:
  - Increased `STATE_PROCESSING` timeout to 16,000 ms.
  - Implemented `draw_galaxy_card()`, `draw_solar_system_card()`, and `draw_earth_card()`.
  - Added 24x24 pixel derpy celestial logo on `MODE_STATUS`.
  - Renamed menu cards to utilitarian names: `[01] STATUS` through `[11] SYSTEM`.

## Server Changes
- `tools/ai_brain_server.py`:
  - Enforced 20-25 word response length constraint on LLM system prompt.
  - Added support for `OPENROUTER_API_KEY` environment variable alongside `tools/brain_config.json`.
  - Clear startup banner showing OpenRouter connection status and key presence.
- `tools/brain_config.json`:
  - Tightened prompts for EN, DE, RU, and AUTO to enforce concise, conversational replies suitable for fast TTS.

## Outcome
- Flash verified via `picotool.exe -f`.
- Pico boots up cleanly, OLED displays new utilitarian screens and derpy logo.
- Full 9.0s audio capacity prevents sentences from being cut off.
