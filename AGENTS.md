# AGENTS.md — Instructions & Anti-Regression Playbook for Coding Agents

You are the hands-on engineering guide and full-stack maintainer for **CELESTIAL ENGINE Mk.V** (created by **Dr. Andrey Holz** — `https://www.linkedin.com/in/andrey-holz/`, `@DerAndr`).

Read `README.md` and `docs/tasks.md` before making changes.

## Primary Objective
Maintain and evolve the working **Celestial Engine Mk.V** ecosystem (`v1.3.0+` baseline across RP2350 C firmware, Android 15 Companion App, Raspberry Pi 5 offline node, and the GitHub Pages Interactive Digital Twin) safely, cleanly, and without regressions.

---

## 1. Git, Windows Line Endings & Branch Discipline (Zero IDE Conflicts)
- **Keep `core.autocrlf = false` locally:** On Windows, `core.autocrlf = true` clashes with LF buffers in the editor, producing phantom `M` (modified) flags with 0 lines of diff and breaking `edit_file` exact matching. Ensure `git config core.autocrlf false` is set.
- **Always set upstream tracking (`-u`):** Whenever creating or switching branches, ensure upstream tracking is configured (`git branch --set-upstream-to=origin/<branch> <branch>` or `git push -u origin <branch>`) so the IDE never displays a misleading **"Publish Branch"** prompt.
- **Branch Awareness (`master` vs Feature Branches):**
  - `master` is the live production branch that deploys directly to **GitHub Pages** (`https://derandr.github.io/celestial_engine_project/`) and represents the main repository page.
  - When making hotfixes intended for the live site or main `README.md`, ensure they are applied/merged to `master` AND kept in sync with the active working branch, leaving the local checkout on the branch the user expects.
- **Never leave temporary build/debug clutter in the repo root:** Never create `temp_apk*`, `screen_*.png`, `logcat*.txt`, or ad-hoc folders in the root directory. Use a git-ignored `scratch/` directory and clean up temporary artifacts immediately.

---

## 2. Web Digital Twin & In-Browser Local LLM Rules (`docs/`)
- **Strict English-Only Public UI (`en-US`):** Keep all UI labels, microphone status banners, and voice recognition (`SpeechRecognition.lang = 'en-US'`) in `docs/index.html` and `docs/twin/` strictly in English so international visitors have a consistent experience.
- **Mobile Browser LLM Performance (`Transformers.js` WebGPU vs WASM SIMD):**
  - **Never use `WebGPU (q4f16)` for `SmolLM2-135M` on mobile phones (`isMobileWorker`):** Mobile Android GPUs (ARM Mali / Tensor) suffer severe GPU⇄CPU buffer sync stalls on `q4f16` shaders (45–77 seconds per reply). On mobile devices, always route `SmolLM2-135M` to **CPU WASM SIMD (`device: 'wasm', dtype: 'q4', simd: true`)** with `do_sample: false`.
  - **No JS Vocabulary Sorting on Mobile (`top_p` / `repetition_penalty`):** In `@huggingface/transformers`, `top_p` sorts all 49,152 vocabulary logits in JavaScript on every token (~200 ms/token overhead on mobile). Keep `do_sample: false` (greedy decoding) on mobile while preserving full `WebGPU` sampling on Desktop.
  - **Yield Background 60 FPS Canvases on Mobile During LLM Inference:** While `window.__celestialIsLocalLlmGenerating === true` on a mobile device, throttle the off-screen 60 FPS canvas loop (`simulator.js`) to 1 frame per 600 ms so 100% of the mobile CPU/GPU goes to the LLM Web Worker. Do not throttle Desktop.
- **Small-Model Anti-Hallucination & Token Bounds (`maxNewTokens = 40`):**
  - Never set `maxNewTokens > 40` for 135M–350M in-browser models. Above ~35 tokens, tiny models finish answering the question in sentence 1 and start hallucinating fake biographical details (e.g., fake age or university).
  - Always stream tokens live via `TextStreamer` (`type: 'STREAM'`) and use `InterruptableStoppingCriteria` to stop cleanly once 1–2 complete sentences finish.
  - Compact RAG prompts: In `repo-knowledge.js`, inject only the specific matched topic fact (~80–100 tokens) and never duplicate `REPO_SUMMARY_COMPACT`, keeping mobile prompt prefill fast.
- **Regex & Link Sanitization Guardrails (`worker.js` & `controls.js`):**
  - **Protect abbreviations before sentence splitting:** Never split sentences on `/(?<=[.!?])\s+(?=[A-Z])/` without first protecting `Dr.`, `Mr.`, `Mrs.`, `Ms.`, `Prof.`, `Mk.`, `St.`, `vs.`—otherwise `"Dr. Andrey Holz"` splits at `"Dr."`!
  - **Replace small-LLM bracketed link placeholders:** Automatically replace `[link to LinkedIn profile]` / `[URL]` with `https://www.linkedin.com/in/andrey-holz/`.
  - **Render clickable links safely without `innerHTML`:** In `setSafeText('oracle-response-text', ...)`, convert `https://...` URLs into safe DOM `<a>` elements via `document.createElement('a')` and `textContent` (preserving XSS safety and passing `python tools/test_twin_acceptance.py`).
- **Cache-Busting Version Sync (`?v=1.3.x`):**
  - Whenever editing any file under `docs/twin/` or `docs/simulator.js`, bump the `?v=1.3.x` query string across `docs/index.html`, `docs/twin/controls.js`, `docs/twin/oracle/provider.js`, `docs/twin/oracle/llm.js`, and `docs/twin/oracle/worker.js` so mobile and desktop browsers immediately load the new Web Worker and ES modules without stale cache.

---

## 3. Documentation & GitHub Main Page Parity (`README.md`)
- **Keep `README.md` synchronized with `docs/index.html`:** Whenever features, model catalogs, release tags (`v1.3.0`), or track/effect counts (**21 chiptunes**, **14 SFX**, **28 LED patterns**, **6 personas**) change, update `README.md` in the same milestone.
- **GitHub Mermaid Diagram Rules:**
  - Never use escaped quotes (`\"`), literal `\n`, or square brackets (`[SPELL]`) inside double-quoted Mermaid node or edge labels in `README.md`—GitHub's Mermaid parser will fail with `Parse error: got 'PS'`. Use `1.3-inch` instead of `1.3\"` and plain text inside quotes.

---

## 4. Physical Hardware Guidance & Breadboard Ergonomics
- **Non-Negotiable Safety Rules:**
  - Never guess about voltage-sensitive connections. Never instruct 5 V into Pico GPIO (3.3 V logic strictly).
  - Never confuse staff power (5V USB power bank into Pico `VBUS Pin 40` + `1000 µF` decoupling capacitor) with wearable Raspberry Pi 5 power (`Geekworm X1209 UPS` + 4× `Molicel 18650`).
  - Always instruct **Power Off** (disconnect USB/battery) before moving jumpers or components.
- **Physical Breadboard Practicality (Full-Length Power Bus Rails):**
  - When instructing the user to place a component on the `5V (+)` and `GND (−)` rails (such as the `1000 µF` electrolytic capacitor), never assume the exact holes directly adjacent to `Pin 40 (VBUS)` or `Pin 38 (GND)` are empty.
  - Explicitly remind the user that the red `+` (5V) and blue/black `−` (GND) bus strips run the entire length of the breadboard (`Rows 1–30`), so they can plug the component into **any free pair of `+` and `−` holes** along that rail (ideally near where the LED/amp power jumpers tap into the rail).
- **Every Hardware-Guidance Response Must Contain:**
  1. **Goal** — one sentence.
  2. **Parts** — only what is needed for this step.
  3. **Power off** — explicitly state when power must be disconnected before wiring.
  4. **Wiring table & physical placement** — module pin -> Pico physical pin / breadboard rail -> GPIO/function -> voltage/note (with practical fallback holes if primary pins are occupied).
  5. **Code/build changes** — minimal scope.
  6. **Commands** — exact commands from repository root.
  7. **Expected result** — observable LED/OLED/audio/serial behavior.
  8. **If it fails** — short ordered diagnostics.
  9. **Documentation update** — update experiment log and pinout/inventory if applicable.

---

## 5. Software Engineering & Automated Verification Rules
- **Centralized Hardware Config:** Keep all RP2350 pin assignments in `firmware/include/config.h`.
- **Single State Machine & Modular Separation:** Keep drivers/modules separated from application state logic; do not hard-code LED ring offsets throughout application code.
- **Mandatory Acceptance Suite Before Pushing `docs/`:**
  Always run `python tools/test_twin_acceptance.py` before committing changes to `docs/` or `android/.../OraclePersona.kt` to verify persona parity, word limits, canvas integrity, cache-busting `?v=` sync, and GitHub Mermaid syntax.

---

## 6. Multi-Agent Team Architecture & Model Complexity Tiers (2026 Antigravity Best Practices)

### A. When to Use Single Agent vs. Multi-Agent Team (`define_subagent` / `invoke_subagent` / `/teamwork`)
- **Single-Agent Mode (Default for <= 3 files or interactive debugging):**
  - Use a single agent directly for quick bugfixes, single-subsystem edits, `README.md` updates, or step-by-step physical hardware breadboard guidance. Never spawn subagents for trivial edits where coordination overhead exceeds execution time.
- **Multi-Agent Team Mode (For Cross-Stack Features & Phase 1–3 Modular Refactoring):**
  - Spawn specialized subagents when work spans **disjoint subdirectories** (`firmware/`, `android/`, `tools/`, `docs/`), when a broad read-only audit would bloat the main context window, or when using `/teamwork` on `refactor/modular-cleanup`.
  - **Workspace Isolation Rule:** When two write-enabled subagents run in parallel on different subsystems, use `Workspace: 'share'` (git worktree) or strictly disjoint directory scopes (`firmware/` vs `android/`) so agents never collide on `git status` or index locks.

### B. Canonical Agent Roles & Model Complexity Matrix for Celestial Engine Mk.V

| Agent Role (`TypeName` / `Role`) | Recommended Model Tier (`Model`) | Write Access (`EnableWriteTools`) | Assigned Skill(s) (`.agents/skills/`) | Responsibility & Scope |
| :--- | :--- | :---: | :--- | :--- |
| **1. Lead Orchestrator** *(Main Agent)* | `inherit` / `pro` | Yes | `AGENTS.md` (Global Rules) | Owns git branch/release state (`master` vs `refactor/*`), decomposes cross-stack tasks, reviews subagent diffs, and communicates with the user. |
| **2. Codebase Researcher & Log Auditor** (`research`) | `flash_lite` or `flash` *(Simple / Fast)* | **No** (Read-Only Sandbox) | None needed | Fast grep/search across logs, experiment files (`docs/experiments/`), pinout tables, or verifying protocol constants without polluting main context. |
| **3. RP2350 Embedded C Engineer** (`firmware-agent`) | `pro` or `inherit` *(High Complexity)* | Yes (`firmware/`, `tests/`) | `pico-rp2350-development`, `rp2350-audio-i2s` | Dual-core RP2350 C/PIO/DMA code (`main.c`, `leds.c`, `audio.c`, `net_client.c`), 64 KB ring buffer safety, and `cmake --build firmware/build`. |
| **4. Android 15 & Gemma Engineer** (`android-agent`) | `pro` or `inherit` *(Medium-High Complexity)* | Yes (`android/`) | `android-compose-development`, `android-edge-to-edge` | Kotlin/Compose UI, Hotspot UDP `:5005` `EPERM` socket handling, `GemmaModelEngine.kt` (`Gemma 4 E2B`), `[SPELL: ...]` stream interceptor, and `./gradlew assembleDebug`. |
| **5. Web Digital Twin & Browser LLM Specialist** (`web-twin-agent`) | `flash` (UI/CSS) or `pro` (WebGPU Worker) | Yes (`docs/`, `tools/bump_twin_version.py`) | `web-digital-twin-and-browser-llm` | Maintains `docs/index.html`, `simulator.js`, `worker.js` (`Transformers.js` mobile WASM SIMD vs desktop WebGPU), bumps `?v=`, and keeps `README.md` synced. |
| **6. Pi 5 Edge Brain Engineer** (`pi5-agent`) | `flash` or `inherit` *(Medium Complexity)* | Yes (`tools/`) | `pi5-edge-brain-server` | Maintains `tools/ai_brain_server.py` (`Whisper.cpp` + `Ollama` + `Piper` TTS), 6-persona parity, and `[SPELL: ...]` UDP dispatch. |
| **7. QA & Parity Gatekeeper** (`qa-verifier`) | `flash` *(Fast / Deterministic)* | Yes (Test Runner only) | `web-digital-twin-and-browser-llm` | Runs `python tools/test_twin_acceptance.py` and `python -m unittest discover -s tests -v`, checks `git status -sb` for root clutter or `LF/CRLF` drift before any push. |

