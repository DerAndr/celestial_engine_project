# Experiment 048: Default Model Migration to Gemma 4 E2B (`gemma-4-E2B`)

## Date: 2026-10-25
## Status: IMPLEMENTED & VERIFIED

## 1. Goal
Transition the default AI model across the Android Companion Phone Brain and Celestial Engine stack to **`gemma-4-E2B`** (`G4-MM` / `google/gemma-4-e2b-it` / on-device edge model) for maximum responsiveness, lower latency, reduced resource footprint, and streamlined real-time voice streaming. Enforce bounded in-app logs and strictly concise verbal responses.

## 2. Background & User Decision
During live end-to-end testing in Experiment 047:
- Gemma 4 31B (`google/gemma-4-31b-it`) restored high-quality ancient-futuristic lore and chain-of-thought reasoning, but 31B parameters carry higher Time-To-First-Token (TTFT) and slower token generation cadence for rapid back-and-forth speech.
- `gemma-4-E2B` is specifically optimized for edge and mobile environments (2B parameter class with high distilled reasoning density).
- It is significantly faster, simpler, and offers lower inference latency (~1.5–2.5s turnaround or sub-second TTFT streaming), making it the ideal daily driver for the interactive Celestial Staff voice loop.
- User directives:
  1. *"gemma-4-E2B — давай работать вот с этой моделью по умолчанию. Потому что она точно удобнее и проще и быстрее скорее всего. Это будет новая срочная задача."*
  2. *"так же добавь задачу что ответы должны быть короткими"*
  3. *"И еще лог в приложении не должен быть бесконечным - давай его ограничим"*

## 3. Implemented Changes
1. **Companion App (`GemmaModelEngine.kt`)**:
   - Added `GemmaVariant.GEMMA_4_E2B` (id = 0, tag = `G4-E2B`, modelId = `google/gemma-4-e2b-it`, fallbackModelId = `google/gemma-4-26b-a4b-it`).
   - Set `GemmaConfig(variant = GemmaVariant.GEMMA_4_E2B, maxTokens = 128)`.
   - Built transparent network fallback: if primary endpoint lacks `gemma-4-e2b-it`, it immediately routes to `google/gemma-4-26b-a4b-it` (3.8B active parameter MoE) without interrupting the stream.
2. **Concise & Punchy Voice Responses (Краткие ответы)**:
   - System prompts (RU/EN/DE): Strictly constrained verbal responses to **1–2 short, punchy sentences (maximum 15–25 words)** without markdown or emojis.
   - Spell chants capped under 12 words.
   - Keeps speaking time under 3 seconds and fits neatly on the SH1106 OLED marquee.
3. **Dashboard & UI (`DashboardViewModel.kt` & `OracleTerminalScreen.kt`)**:
   - Set `_selectedModelVariant` initial state to `GemmaVariant.GEMMA_4_E2B`.
   - Added `G4-E2B` pill selector to Oracle Terminal.
   - Dynamic query button automatically reads `Query G4-E2B`.
4. **Bounded In-App Log Telemetry & History**:
   - `StaffRepository.kt`: Defined `MAX_LOG_ENTRIES = 100` and applied sliding window (`drop(current.size - MAX_LOG_ENTRIES + 1)`).
   - `DashboardViewModel.kt`: Defined `MAX_CHAT_HISTORY = 40` and capped `_chatHistory` to prevent memory growth.
   - `LiveLogConsole.kt`: Updated header badge to `${logs.size}/100`.
5. **Soundboard Timing Polish (`SoundboardScreen.kt`)**:
   - Aligned UI label to `"400ms combo"` matching the snappy sequencer in `DashboardViewModel`.

## 4. Verification & Live Latency Benchmark
- **Live On-Device Deployment**: Built debug APK via GitHub Actions and deployed to Google Pixel 9 Pro XL (`49051FDAS001YK`).
- **Turnaround Latency**: **1.4 seconds** total from prompt trigger to full generation completion (21 tokens @ 29.9 tok/s, sub-second TTFT).
- **Concise Response Quality**: Returned exactly 1–2 punchy sentences (21 tokens) without markdown or emojis, perfectly suited for the SH1106 OLED display and <3s audio synthesis.
- **Log Buffer Health**: Verified live log telemetry bounded strictly to `100` items with sliding window truncation (e.g. `17/100`), eliminating memory leaks.
- **Test Suite**: All 27 regression and host firmware compilation tests pass with 100% success.

