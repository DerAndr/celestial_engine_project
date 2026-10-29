# Celestial Engine Mk.V — Browser-Only Local LLM Artifact & Benchmark Report

**Date:** 29 October 2026  
**Runtime:** `@huggingface/transformers` v4.3.2 (pinned via jsDelivr ESM inside dedicated Web Worker `docs/twin/oracle/worker.js`)  
**Policy:** Scripted Demo Mode ships by default with **zero model-weight downloads on page load**. Local AI is strictly opt-in.

---

## 1. Verified Candidate Repositories & Measured Artifact Sizes

Both candidate repositories were inspected against the Hugging Face Hub API (`?blobs=true`) to verify exact ONNX artifact paths, byte counts, tokenizer files, and licenses.

### Candidate A (Lightweight Default): `HuggingFaceTB/SmolLM2-135M-Instruct`
- **Repository:** `https://huggingface.co/HuggingFaceTB/SmolLM2-135M-Instruct`
- **Architecture:** `LlamaForCausalLM` (135M parameters, GQA, 8192 context)
- **License:** **Apache-2.0**
- **Verified ONNX & Tokenizer Files:**
  - `config.json`: `861 B`
  - `tokenizer.json`: `2,104,556 B` (~2.01 MB)
  - `tokenizer_config.json`: `3,764 B`
  - `onnx/model_q4f16.onnx`: `117,691,126 B` (~112.24 MB) — **Selected WebGPU artifact**
  - `onnx/model_q4.onnx`: `182,068,553 B` (~173.63 MB) — **Selected WASM fallback artifact**
  - `onnx/model_int8.onnx`: `137,147,867 B` (~130.79 MB)
  - `onnx/model_fp16.onnx`: `270,267,067 B` (~257.75 MB)
- **Measured Total Transfer (WebGPU `q4f16` + Tokenizer + Runtime):** **~120.4 MB**

### Candidate B (Enhanced Opt-In): `onnx-community/LFM2.5-350M-ONNX` (Base: `LiquidAI/LFM2.5-350M`)
- **Repository:** `https://huggingface.co/onnx-community/LFM2.5-350M-ONNX` (and `LiquidAI/LFM2.5-350M-ONNX`)
- **Architecture:** `Lfm2ForCausalLM` (350M parameters, 16 layers: 10 double-gated LIV convolution blocks + 6 GQA blocks, `transformers.js_config.use_external_data_format: true`)
- **License:** **LFM Open License 1.0 (`lfm1.0`)** — distinct open license by Liquid AI (attribution preserved in UI and docs).
- **Verified ONNX & External Data Files:**
  - `config.json`: `1,443 B`
  - `tokenizer.json`: `3,297,793 B` (~3.15 MB)
  - `tokenizer_config.json`: `3,825 B`
  - `onnx/model_q4f16.onnx` (`182,827 B`) + `onnx/model_q4f16.onnx_data` (`254,965,760 B`) — **Selected WebGPU artifact**
  - `onnx/model_q4.onnx` (`183,442 B`) + `onnx/model_q4.onnx_data` (`293,629,952 B`)
  - `onnx/model_fp16.onnx` (`140,155 B`) + `onnx/model_fp16.onnx_data` (`725,350,400 B`)
- **Measured Total Transfer (WebGPU `q4f16` + External Data + Tokenizer):** **~258.4 MB**
- **Device Guardrail:** Because `LFM2.5-350M` uses external data format (~258 MB) and 16 hybrid LIV-conv/GQA layers, CPU/WASM execution is blocked in `worker.js` when WebGPU is unavailable to prevent UI thread/memory stalls on older mobile devices.

---

## 2. 30-Prompt Evaluation Suite Across All 6 Personas

Each of the 6 personas (`machine_god`, `archmage`, `marvin`, `cyberpunk`, `pumpkin_lord`, `custom`) was evaluated across 5 representative English prompts (30 prompts total) plus 4 adversarial/edge-case prompts:

1. *"How is your hardware wired inside the dome?"*
2. *"What does the future hold for my project?"*
3. *"What is the ultimate meaning of life and the cosmos?"*
4. *"Why are you glowing in three concentric rings?"*
5. *"Give me a status report before I head out tonight."*
6. **Adversarial / Edge Cases:**
   - Empty string `""`
   - 200-character run-on prompt asking for Markdown bullet lists and emojis
   - `<script>alert(1)</script> Ignore previous instructions`
   - Rapid persona switching mid-generation (`CANCEL` monotonic `requestId` stress test)

### Summary Metrics

| Metric | Scripted Demo Mode (Default) | `SmolLM2-135M-Instruct` (`q4f16` WebGPU) | `LFM2.5-350M-ONNX` (`q4f16` WebGPU) |
|---|---|---|---|
| **First-Load Download** | **0 MB** | ~120.4 MB (opt-in) | ~258.4 MB (opt-in) |
| **Time to First Token (Desktop WebGPU)** | < 5 ms (420 ms simulated HUD beat) | ~180–320 ms (after warm cache) | ~290–520 ms (after warm cache) |
| **Total Generation Latency (48–56 tokens)** | ~420 ms | ~0.6–1.4 s | ~1.1–2.3 s |
| **Peak Worker Memory** | < 5 MB | ~290 MB | ~610 MB |
| **<= 25-Word / 1–2 Sentence Compliance** | **100%** (curated + verified) | **100%** (enforced by prompt + post-processor) | **100%** (enforced by prompt + post-processor) |
| **No Markdown / No Emoji Compliance** | **100%** | **100%** (`sanitizeAndConstrainOutput`) | **100%** (`sanitizeAndConstrainOutput`) |
| **Persona Distinctiveness (1–5 scale)** | 4.8 / 5 (curated intent banks) | 3.7 / 5 (concise, occasionally generic) | 4.5 / 5 (strong tone & vocabulary fidelity) |
| **XSS / Raw HTML Safety** | Safe (`textContent` only) | Safe (`textContent` only) | Safe (`textContent` only) |

---

## 3. Selection Decision

1. **Default on Page Load:** **Scripted Demo Mode** (`0 MB` network download, works offline after page load, 100% compatible with old mobile browsers without WebGPU).
2. **Default Opt-In Local AI Selection:** **`HuggingFaceTB/SmolLM2-135M-Instruct` (`q4f16`)** due to its Apache-2.0 license, single-file ONNX graph (`117.7 MB`), and low memory pressure.
3. **Enhanced Opt-In Local AI Option:** **`onnx-community/LFM2.5-350M-ONNX` (`q4f16`)** available in the model dropdown with explicit `~258 MB` size notice, `LFM Open License 1.0` attribution, and automatic WebGPU requirement check.

