# Experiment 048: Default Model Migration to Gemma 4 E2B (`gemma-4-E2B`)

## Date: 2026-10-25
## Status: SPECIFIED & SCHEDULED (NEXT URGENT TASK)

## 1. Goal
Transition the default AI model across the Android Companion Phone Brain and Celestial Engine stack to **`gemma-4-E2B`** (`G4-MM` / `google/gemma-4-e2b-it` / on-device edge model) for maximum responsiveness, lower latency, reduced resource footprint, and streamlined real-time voice streaming.

## 2. Background & User Decision
During live end-to-end testing in Experiment 047:
- Gemma 4 31B (`google/gemma-4-31b-it`) restored high-quality ancient-futuristic lore and chain-of-thought reasoning, but 31B parameters carry higher Time-To-First-Token (TTFT) and slower token generation cadence for rapid back-and-forth speech.
- `gemma-4-E2B` is specifically optimized for edge and mobile environments (2B parameter class with high distilled reasoning density).
- It is significantly faster, simpler, and offers lower inference latency (~1.5–2.5s turnaround or sub-second TTFT streaming), making it the ideal daily driver for the interactive Celestial Staff voice loop.

## 3. Scope of Changes for Next Session
1. **Companion App (`GemmaModelEngine.kt`)**:
   - Update `GemmaVariant` definitions so that `GEMMA_4_E2B` (or `E2B_MULTIMODAL`) is the primary default variant.
   - Configure model endpoint: `google/gemma-4-e2b-it` (or verified OpenRouter ID / on-device edge runtime) with fallback to `google/gemma-3-27b-it` / `google/gemini-2.5-flash`.
   - Update `GemmaConfig(variant = GemmaVariant.GEMMA_4_E2B)`.
2. **Dashboard & UI (`DashboardViewModel.kt` & `OracleTerminalScreen.kt`)**:
   - Set `_selectedModelVariant` initial state to `GemmaVariant.GEMMA_4_E2B`.
   - Update UI pill labels and button text to `Query G4-E2B` / `Gemma 4 (E2B)`.
   - Retain thinking trace parsing and rich techno-magical spell formatting (`[SPELL: ...]`).
3. **Firmware & Network Model Alignment (`net_client.c`, `net_protocol.h`)**:
   - Ensure `AI_MODEL_GEMMA4_E2B` (`G4-MM` index 0 / 4) is default in phone mode.
4. **Verification Criteria**:
   - Voice press on staff GP9 (QUERY) -> 16 kHz PCM to Phone Brain -> Transcription -> `gemma-4-E2B` generation -> Structured prophecy / spell execution -> TTS burst to MAX98357A speaker with minimal latency.
