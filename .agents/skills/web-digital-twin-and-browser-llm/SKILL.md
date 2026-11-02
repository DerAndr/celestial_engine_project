---
name: web-digital-twin-and-browser-llm
description: >-
  Provides instructions to maintain, optimize, and verify the Celestial Engine Mk.V Interactive Web Digital Twin
  (GitHub Pages), in-browser Local LLM worker (Transformers.js WebGPU / WASM SIMD), 6 Oracle personas, and
  README/Mermaid documentation. Use this skill when the user asks to edit docs/index.html, docs/simulator.js,
  docs/twin/*, or README.md.
---

# Web Digital Twin & In-Browser Local LLM Runbook

## 1. Architecture & Parity Requirements
The Browser Digital Twin (`docs/`) is a 1:1 interactive replica of the physical **Celestial Engine Mk.V** staff (`v1.3.0+`):
- **28 Concentric WS2812B LED Patterns** (`7 Core + 12 Inner + 24 Outer = 43 LEDs`), matching `firmware/src/leds.c`.
- **21 8-Bit Chiptune Melodies** + **14 Cosmic / R2-D2 / Spell Sound FX**, matching `firmware/src/audio.c`.
- **6 Authoritative Oracle Personas** (`machine_god`, `archmage`, `marvin`, `cyberpunk`, `pumpkin_lord`, `custom`), strictly synchronized between `android/.../OraclePersona.kt`, `docs/twin/oracle/personas.json`, `docs/twin/oracle/provider.js`, and `docs/twin/oracle/scripted.js`.
- **English-Only UI (`en-US`)**: All UI strings, status badges, and Web Speech API recognition (`SpeechRecognition.lang = 'en-US'`, max 40s recording) must remain strictly in English.

---

## 2. In-Browser Local LLM (`docs/twin/oracle/worker.js`) Best Practices

### A. Mobile vs. Desktop Execution (`isMobileWorker`)
1. **Force WASM SIMD (`q4`) on Mobile for `SmolLM2-135M`**:
   - Mobile Android GPUs (ARM Mali / Tensor) hit severe GPU⇄CPU buffer sync stalls on `onnxruntime-web` `q4f16` WebGPU shaders (taking 45–77s per response).
   - Detect mobile in `worker.js`:
     `const isMobileWorker = /Android|iPhone|iPad|iPod|Mobile/i.test((typeof navigator !== 'undefined' && navigator.userAgent) || '');`
   - Set `const useWebgpu = isMobileWorker && spec.key === 'smollm2-135m' ? false : gpuCheck.ok;` and enable `env.backends.onnx.wasm.simd = true;`.
2. **Eliminate JS Logit Sorting on Mobile (`do_sample: false`)**:
   - In `@huggingface/transformers`, `top_p: 0.9` (`TopPLogitsWarper`) copies and `.sort()`s all 49,152 vocabulary logits in JavaScript on every token (~200 ms/token overhead on mobile CPUs).
   - On **Mobile (`isMobileWorker`)**, always pass `do_sample: false` (native greedy decoding, zero `top_p` or `repetition_penalty` overhead).
   - On **Desktop (`!isMobileWorker`)**, preserve full `WebGPU` sampling (`do_sample: loadedDevice === 'webgpu'`, `temperature: 0.5`, `top_p: 0.9`, `repetition_penalty: 1.1`).
3. **Throttle Off-Screen 60 FPS Canvases on Mobile During LLM Inference**:
   - In `docs/twin/controls.js`, set `window.__celestialIsLocalLlmGenerating = Boolean(useLocalAi)` before `oracleProvider.generate(...)` and reset to `false` on completion/error.
   - In `docs/simulator.js` (`animate(t)`), when `isMobileDevice && window.__celestialIsLocalLlmGenerating` is true, throttle canvas redraws to once every `600 ms` so 100% of mobile CPU/GPU goes to the LLM worker. Never throttle Desktop.

### B. Token Bounds, Streaming & Anti-Hallucination (`maxNewTokens = 40`)
1. **Strict `maxNewTokens = 40` Cap**:
   - Never raise `maxNewTokens` above `40` (or word limit above `34 words` / `2 sentences`) for 135M–350M models. Beyond ~35 tokens, small models finish answering the question in sentence 1 and hallucinate fake biographical filler (e.g., fake age or university).
2. **Live Token Streaming (`TextStreamer`) + Smart Early Stopping (`InterruptableStoppingCriteria`)**:
   - Always pass a `TextStreamer` instance so partial tokens (`type: 'STREAM'`) update `#oracle-response-text` in real time (`⚡ Streaming live from Local LLM...`).
   - Use `InterruptableStoppingCriteria` on mobile and on `creator_author` queries to stop immediately once a complete sentence (`>= 7–12 words` ending in `.!?`, excluding abbreviations like `Dr.`) finishes.
3. **Compact RAG Context (`docs/twin/oracle/repo-knowledge.js`)**:
   - Never append `REPO_SUMMARY_COMPACT` twice when a specific topic matches in `retrieveRepoKnowledge()`. Return only the matched topic's `fact` (~80–100 tokens) so mobile prompt prefill stays fast (`<1s`).

### C. Sanitization, Abbreviations & Clickable Links
1. **Protect Abbreviations Before Sentence Splitting**:
   - Before splitting sentences on `/(?<=[.!?])\s+(?=[A-ZА-ЯЁ0-9])/`, replace `\b(Dr|Mr|Mrs|Ms|Prof|Mk|St|vs)\.\s+` with `$1\u0000 ` and restore `.` afterward so `"Dr. Andrey Holz"` is never split at `"Dr."`.
2. **Creator Attribution & Link Restoration**:
   - Convert markdown links `[label](https://...)` to `$2` and replace bracketed placeholders like `[link to LinkedIn profile]` with `https://www.linkedin.com/in/andrey-holz/`.
   - Filter out any hallucinated biography sentences matching `/\b\d+-year-old\b|\buniversity\b|\bmoscow\b|\bborn in\b|\bgraduated\b/i`.
3. **Safe Clickable URLs in `setSafeText` (`docs/twin/controls.js`)**:
   - Render `https://...` URLs inside `#oracle-response-text` using `document.createTextNode` + `document.createElement('a')` (`a.textContent = rawUrl`)—never `innerHTML`.
4. **Natural TTS Pronunciation (`docs/twin/audio/speech.js`)**:
   - Replace `https://www.linkedin.com/in/andrey-holz/` with `'LinkedIn dot com, Andrey Holz'` in `SpeechSynthesisUtterance` so Browser TTS never spells out `h-t-t-p-s...`.

---

## 3. Cache-Busting & Verification Checklist
Whenever modifying any file in `docs/`:
1. **Bump `?v=1.3.x`** across all module entry points so browsers do not serve cached workers:
   - `docs/index.html` (`simulator.js?v=...`, `twin/controls.js?v=...`)
   - `docs/twin/controls.js` (`state.js?v=...`, `provider.js?v=...`, `staff-2d.js?v=...`, `oled.js?v=...`, `speech.js?v=...`)
   - `docs/twin/oracle/provider.js` (`scripted.js?v=...`, `llm.js?v=...`)
   - `docs/twin/oracle/llm.js` (`worker.js?v=...`)
   - `docs/twin/oracle/worker.js` (`repo-knowledge.js?v=...`)
2. **Run the Acceptance Suite**:
   ```powershell
   python tools/test_twin_acceptance.py
   ```
3. **Check `README.md` & Mermaid Syntax**:
   - Ensure `README.md` reflects current model names, version tags, and feature counts.
   - Never use escaped quotes `\"`, `\n`, or square brackets `[...]` inside double-quoted Mermaid labels in `README.md`.

