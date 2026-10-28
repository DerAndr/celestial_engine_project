# Experiment 059 — AI Persona Presets, Custom System Prompt Editor & Strict English i18n

**Date:** 2026-10-28  
**Objective:** Add an interactive AI Persona & System Prompt editor directly in the companion app UI with role presets and custom multi-line editing, and ensure strict default English localization across the entire system.

---

## 1. Context & Motivation

Following the canonical release of v1.1.0 (Halloween Oracle with 24-track Jukebox and Grimoire), users needed the ability to:
1. Dynamically alter the personality and role of the onboard AI Oracle (e.g. from serious Machine God to wise Archmage, cynical Marvin, cyberpunk Netrunner, or festive Jack Pumpkin King) directly from the mobile app without code edits or recompilation.
2. Enter custom roleplay personas or specific task instructions on the fly.
3. Ensure that the app and hardware defaults to clean English without unexpected switches to Russian upon hearing voice queries.

---

## 2. Technical Architecture & Implementation

### 2.1 Multilingual Persona Model (`OraclePersona.kt`)
Created `OraclePersona` with 6 distinct roles:
1. 🤖 **Machine God** (`machine_god`) — Quantum computational consciousness, systemic analysis, causality, and probability theory.
2. 🧙‍♂️ **Wise Archmage** (`archmage`) — Ancient keeper of arcane secrets, cosmic spheres, and star vaults.
3. 🛸 **Marvin Android** (`marvin`) — Paranoid depressed robot with a planet-sized brain (*Hitchhiker's Guide to the Galaxy*).
4. 🕶️ **Cyberpunk Netrunner** (`cyberpunk`) — Rogue combat AI construct from the corporate neon darknet.
5. 🎃 **Jack Pumpkin King** (`pumpkin_lord`) — Lord of Pumpkin Fire, eerie and festive spirit of Halloween night.
6. ✏️ **Custom Persona** (`custom`) — Full user-defined custom role and system prompt.

Each persona provides localized prompts, titles, and subtitles (`En`, `Ru`, `De`):
- `getTitle(lang: StaffLanguage)`: returns English title by default or when English is selected.
- `getSubtitle(lang: StaffLanguage)`: returns localized subtitle.
- `getPromptForLanguage(lang: StaffLanguage)`: provides the role instruction in the target dialect while respecting speech constraints.

### 2.2 System Prompt & Safety Wrapping (`GemmaModelEngine.kt`)
In `GemmaModelEngine.streamInference()`:
- System prompt dynamically wraps the active persona prompt with operational safety constraints:
  - Concise speech: 1–2 sentences, < 25 words (optimized for rapid MAX98357A I2S playback).
  - Clean output: strictly no markdown, asterisks, bullet points, or emojis (which degrade TTS fidelity).
  - Fast-path spell tags: preserves `[SPELL: NAME]` syntax for real-time LED and sound effects.
- Added `oracle_persona_id`, `oracle_custom_prompt`, and `oracle_language` to `SharedPreferences` (`celestial_prefs`) persistence.

### 2.3 Interactive UI Editor (`OracleTerminalScreen.kt`)
- Added expandable **ORACLE PERSONA & PROMPT** Card on Tab 4 (Oracle Terminal):
  - Current persona icon and localized badge.
  - Horizontally scrollable chip row for fast preset switching.
  - Multi-line `OutlinedTextField` for instant custom prompt editing.
  - Real-time character count.
  - `Reset` button to revert to the preset's canonical default.
  - `Save` button with persistence to `SharedPreferences`.

### 2.4 Language Isolation & Defect Remediation (`DashboardViewModel.kt`)
- **Root Cause of Language Flip**: Previous code in `DashboardViewModel` checked `transcribedText.any { it in '\u0400'..'\u04FF' }` and mutated `_currentLanguage.value = StaffLanguage.RUSSIAN`.
- **Fix**: Removed the global mutation. `GemmaModelEngine` handles per-query language resolution (`effectiveLanguage`) to respond in Russian when spoken to in Russian, while the user's interface language setting and staff dialect remain strictly as chosen (default: **English**).

---

## 3. Verification & Deployment

1. **Companion App v1.1.2**:
   - Bumped `versionCode = 4`, `versionName = "1.1.2"` in `build.gradle.kts`.
   - Built via GitHub Actions workflow run `40617941635` (`✓ build in 5m14s`).
   - Installed on Pixel 9 Pro XL via ADB (`49051FDAS001YK`).
   - Verified runtime permissions: `ACCESS_LOCAL_NETWORK`, `RECORD_AUDIO`, `POST_NOTIFICATIONS`.
2. **Staff Hardware Link**:
   - Pico 2 W connected on COM9 at IP `10.169.248.64` running `1.1.0-RELEASE (HALLOWEEN-ORACLE)`.
   - Telemetry verified: `APP_TELEMETRY` packets arriving at 1 Hz, dialect confirmed as `[EN]`.
   - Android TextToSpeech pre-warmed in `en-US`.
3. **Repository State**:
   - Commits `5349d28` and `388cd8e` pushed to `origin/master`.
   - Working tree clean.
