# Experiment 042: Real Multimodal Oracle Vision Inference & Streaming

## 1. Goal
Completely eradicate mock/canned string templates from the Android Companion App's Gemma 4 engine and activate real multimodal streaming AI inference (Text + Camera Vision) with live token streaming and Chain-of-Thought (CoT) trace display.

## 2. Root Cause of Previous Stub Behavior
- In `android/app/src/main/java/com/celestial/companion/ai/GemmaModelEngine.kt`, `streamInference()` was invoking `buildResponseText()` with hardcoded strings (e.g. *"Visual scan completed. The camera captured an ethereal artifact. Geometric luminosity and surface contours suggest high harmonic alignment with local aether."*).
- Real camera bitmaps and user prompts were ignored and simulated with fixed delays (`delay(35)`).
- Previous CI builds for the Android APK had failed due to an unresolved reference to `asSharedFlow` in `StaffRepository.kt` and missing color references (`telemetry.connected` / `AmberWarning`) in `DashboardScreen.kt`, leaving the user with an outdated APK.

## 3. Implementation
1. **Compilation Fixes**:
   - Resolved `kotlinx.coroutines.flow.asSharedFlow` import in `StaffRepository.kt`.
   - Corrected `telemetry.connected` to `telemetry.isOnline` and `AmberWarning` to `RunicGold` in `DashboardScreen.kt`.
   - Fixed GitHub Actions `ci.yml` validation logic for gitignored `brain_config.json`.
2. **Real Multimodal Engine**:
   - Integrated OpenRouter multimodal chat completion API (`google/gemini-2.5-flash` / `google/gemma-3-27b-it`).
   - Bitmaps are scaled down to 1280px max dimension and encoded to JPEG Base64 (`data:image/jpeg;base64,...`).
   - Streaming SSE parsing handles native tokens and extracts `<thought>...</thought>` reasoning tags into real-time Chain-of-Thought thinking traces.
   - Genuine token-by-token streaming updates the UI immediately.
   - Comprehensive error handling reports honest network/connectivity diagnostics instead of fake mock stubs.

## 4. Verification
- CI workflow `Build Android Companion APK` triggered on `master`.
- Direct ADB deployment to connected Google Pixel 9 Pro XL (`49051FDAS001YK`).
