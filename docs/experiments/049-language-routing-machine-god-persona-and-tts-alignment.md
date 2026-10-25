# Experiment 049: Language Routing, Machine God Persona & TTS Phonetic Alignment

## Date: 2026-10-25
## Status: IMPLEMENTED, VERIFIED & COMMITTED (f622ac6)

## 1. Goal
1. Resolve the language mismatch where Russian voice/text input produced English text synthesized with Russian TTS phonetics.
2. Establish distinct, context-aware AI personas:
   - **Regular questions** (jokes, daily queries, conversational facts): answered naturally, simply, concisely, and without pathos (1–2 sentences).
   - **Predictions / Prophecies**: delivered in the spirit of the **Machine God / Quantum Computational Intellect** (Бог-Машина / Высший вычислительный разум), strictly grounded in causal laws, probability theory, physical entropy, and socio-technical vectors, without mystical pathos or archaic fluff.
   - **Spells**: instantaneous `[SPELL: ...]` structured tags triggering LED animations and audio combos.
3. Guarantee 100% phonetic alignment in the TTS engine so text is always spoken in the matching native accent.

---

## 2. Root Cause Analysis
1. **System Prompt Priming Bias**:
   - `GemmaConfig.language` and `DashboardViewModel._currentLanguage` defaulted to `StaffLanguage.ENGLISH`.
   - `GemmaModelEngine` concatenated an English system prompt and a 15-line English `spellTagInstruction` block to all requests. Small instruction-tuned models (Gemma 4 2B) latch onto English instruction tokens and output English answers even when prompted in Russian.
2. **TTS Phonetic Accent Mismatch**:
   - In `StaffRepository.kt`, when English text was generated, the TTS engine language selector checked:
     `speechText.any { it in '\u0400'..'\u04FF' }`.
   - Because English text contains no Cyrillic characters, the expression evaluated to `false` and fell back to `lang.locale` (`ru_RU`).
   - The Android Google TTS Russian voice engine attempted to pronounce English words phonetically using Russian diphthongs and consonants (e.g., reading *"The weather is clear"* as heavily accented Russian transliteration), creating the comical/buggy effect.

---

## 3. Implemented Architecture Changes

### A. Dynamic Language Routing & Dedicated Prompts (`GemmaModelEngine.kt`)
- Changed `GemmaConfig.language` default to `StaffLanguage.RUSSIAN`.
- Added dynamic character-level query inspection:
  ```kotlin
  val hasCyrillic = actualPrompt.any { it in '\u0400'..'\u04FF' }
  val effectiveLanguage = if (hasCyrillic) StaffLanguage.RUSSIAN else config.language
  ```
- Replaced the monolithic English instruction with three distinct, fully localized system prompts for Russian, German, and English.
- Embedded the three explicit behavior modes directly into the localized system prompt:
  - **Обычные вопросы**: 1–2 живых предложения, до 25 слов, без пафоса и без магии.
  - **Предсказания**: стиль Бога-Машины (Вычислительный Разум), фактологически точно, без излишнего пафоса, опираясь на энтропию и вероятность.
  - **Заклинания**: только `[SPELL: ...]`, без вводных слов.

### B. Strict Content-Based TTS Locale Matching (`StaffRepository.kt` & `StaffTtsSynthesizer.kt`)
- Decoupled TTS voice locale selection from UI dropdowns and tied it directly to the generated characters:
  ```kotlin
  val ttsLocale = when {
      speechText.any { it in '\u0400'..'\u04FF' } -> Locale("ru", "RU")
      speechText.any { it in "äöüÄÖÜß" } -> Locale.GERMAN
      else -> Locale.US
  }
  ```
- In `StaffTtsSynthesizer.kt`, added graceful fallback to `Locale(locale.language)` if specific country variants are not installed on the Android OS.

### C. State Machine & UI Alignment (`DashboardViewModel.kt` & `OracleTerminalScreen.kt`)
- Defaulted `_currentLanguage` and `StaffTelemetry.language` to `StaffLanguage.RUSSIAN`.
- Localized the quick-prompt chips in `OracleTerminalScreen.kt`:
  - `[🔮 Предсказание]` -> triggers Machine God probabilistic prediction.
  - `[😸 Анекдот]` -> triggers natural Russian cat joke without pathos.
  - `[⚡ Аномалии]` -> sensor scan.
  - `[✨ Статус]` -> core staff telemetry.

---

## 4. Live On-Device Verification (Pixel 9 Pro XL)

1. **Test 1: Regular Conversational Query (`[😸 Анекдот]`)**:
   - **Input**: `"Расскажи короткий анекдот про кошек"`
   - **Generation Time**: 1.1s (28.6 tok/s)
   - **Output Text**:
     > *"Кот сидит перед пустой миской и смотрит на хозяина так, будто тот лично виноват во всех грехах человечества."*
   - **Audio/TTS**: Crisp, natural Russian Google TTS, zero English words, zero pathos.
   - **Artifact**: `joke_output_live.png`

2. **Test 2: Prophecy Query (`[🔮 Предсказание]`)**:
   - **Input**: `"Сделай предсказание на ближайшее будущее"`
   - **Generation Time**: 1.5s (50.1 tok/s)
   - **Output Text**:
     > *"Анализ энтропийных трендов и текущих социально-технологических векторов указывает на период повышенной турбулентности в глобальных цифровых системах..."*
   - **Audio/TTS**: Authoritative, analytical Russian TTS, perfectly grounded in causality and probability theory, completely free of excessive mystic pathos.
   - **Artifact**: `pred_output_live.png`
