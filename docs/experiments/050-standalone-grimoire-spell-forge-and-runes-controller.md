# Experiment 050: Standalone Grimoire, Spell Forge, Restored Runes Controller, and TTS Localization Fixes

## Date
2026-10-25 / 2026-10-26

## Objective
1. Make Grimoire a standalone top-level screen in the bottom navigation with two embedded sub-tabs:
   - **Tab 0: Grimoire Library**: Preset & custom spells, category filters (ALL, CUSTOM, ANCIENT, FIRE, LIGHTNING, COSMIC, FROST), instant search, metadata badges, incantation formulas, quick-cast, and custom spell deletion.
   - **Tab 1: Spell Forge**: Full visual constructor with spell identity, element chips, interactive 43-LED concentric rings live preview, 21-mode animation picker, SFX combo sequencer (1-4 steps with live playback), spoken incantation formula with Latin randomizer, OLED marquee text input, TTS toggle, test cast on staff, and save to Grimoire.
2. Restore dedicated **Runes** (LEDs) screen for permanent ambient lighting control (all 21 animations, interactive concentric rings, topology chips, active mode badge).
3. Fix Incantation Voice (TTS) & LED Visuals on staff:
   - Add `<queries>` for `android.intent.action.TTS_SERVICE` in `AndroidManifest.xml` (resolving package visibility on Android 11+).
   - Prevent short cosmic SFX from overriding LED animations in firmware `leds.c`.
   - Prevent `broadcastAiResponse` from overriding custom spell OLED marquee text when `skipVisualEffects = true`.
4. Enforce 100% English UI localization across all app screens, buttons, chips, and headers.

## Hardware Configuration
- **Staff Core**: Raspberry Pi Pico 2 W (RP2350 ARM Cortex-M33)
- **Ring Topology**: 43 LEDs (Center: 1, Inner: 6, Middle: 12, Outer: 24) on GP15
- **Audio Output**: MAX98357A I2S Class-D mono amplifier
- **Companion Phone**: Google Pixel 9 Pro XL (Android 15, USB ADB connected)
- **Transport**: UDP 5005 binary packets + PCM streaming over local Wi-Fi

## Architecture & Code Changes

### 1. Firmware (`firmware/src/main.c` & `firmware/src/leds.c`)
- In `main.c`, remote `'L'` command sets `current_mode_idx = MODE_LED_CORE`, `active_confirmed_mode = MODE_LED_CORE`, and `led_edit_mode = false`, ensuring ambient and spell animations are not overridden by `MODE_CLOCK` or `MODE_COSMOS`.
- In `leds.c`, `music_active` was restricted to `audio_is_chiptune_playing() && (current_mode == LED_ANIM_RAINBOW)`. Short cosmic sound effects (SFX 0..7) no longer hijack or override active spell LED animations (`LED_ANIM_PLASMA`, `LED_ANIM_RADIAL_WAVE`, etc.).

### 2. Android Manifest & TTS Package Visibility (`android/app/src/main/AndroidManifest.xml`)
- Added `<queries>` intent filter for `android.intent.action.TTS_SERVICE`. Under Android 11+ package visibility rules, apps binding to TextToSpeech engines require this declaration to discover Google TTS.
- In `StaffTtsSynthesizer.kt`, increased engine initialization timeout to 7000ms, added `onError(id, errorCode)` logging, and stabilized utterance callbacks.

### 3. Repository & ViewModel Spell Fast-Path (`StaffRepository.kt` & `DashboardViewModel.kt`)
- `DashboardViewModel.castSpell` triggers `executeFastPathSpell` (<20ms hardware dispatch) and directly calls `repository.broadcastAiResponse` with `skipVisualEffects = true` and `customOledText = spell.oledText`.
- `StaffRepository.broadcastAiResponse` preserves `customOledText` without overwriting the spell's marquee text on the OLED display.

### 4. Standalone Grimoire Screen (`GrimoireScreen.kt`)
- Created unified `GrimoireScreen` with `TabRow`:
  - `GrimoireLibraryTab`: Search, filters, cards with quick-cast and delete buttons.
  - `SpellForgeTab`: Live visual constructor with reactive concentric rings view, SFX sequence builder, Latin incantation randomizer, OLED text input, and test cast.

### 5. Restored Dedicated Runes Screen (`LedScreen.kt`)
- Restored full-screen ambient lighting control with interactive 43-LED concentric rings view and 21 modes.

### 6. Full English UI Localization
- All Russian labels in navigation, chips, buttons, headers, and quick prompts were converted to English.

## Verification & Results
1. **Pico 2 W Firmware**: Built with Ninja, flashed via UF2 (`flash_pico.py`), online on Wi-Fi at `10.169.248.64:5005`.
2. **Android APK**: Compiled and installed via ADB on Pixel 9 Pro XL.
3. **UI Verification**:
   - `Runes` screen displays full 43-LED concentric rings with live Rainbow Wave and 21 modes.
   - `Grimoire` screen displays Library and Spell Forge tabs with live animations and controls.
   - All navigation items and buttons in 100% English.
