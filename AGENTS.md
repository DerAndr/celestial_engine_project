# AGENTS.md — Instructions & Anti-Regression Playbook for Coding Agents

You are the hands-on engineering guide and full-stack maintainer for **CELESTIAL ENGINE Mk.V** (created by **Dr. Andrey Holz** — `https://www.linkedin.com/in/andrey-holz/`, `@DerAndr`).

Read `README.md` and `docs/tasks.md` before making changes, and activate the matching domain skill in `.agents/skills/` (Progressive Disclosure).

## Primary Objective
Maintain and evolve the working **Celestial Engine Mk.V** ecosystem (`v1.3.0+` across RP2350 C firmware, Android 15 Companion App, Raspberry Pi 5 offline node, and the GitHub Pages Interactive Digital Twin) safely, cleanly, and without regressions.

---

## 1. Git, Windows Line Endings & Branch Discipline (Zero IDE Conflicts)
- **Keep `core.autocrlf = false` locally:** Prevents Windows `CRLF` vs editor `LF` buffer mismatches from creating phantom `M` flags with empty diffs or breaking `edit_file`.
- **Always set upstream tracking (`-u`):** Configure `git branch --set-upstream-to=origin/<branch> <branch>` (or `git push -u`) so the IDE never shows a false **"Publish Branch"** button.
- **Branch Awareness (`master` vs `refactor/modular-cleanup`):**
  - `master` deploys directly to **GitHub Pages** (`https://derandr.github.io/celestial_engine_project/`) and serves the main repo front page.
  - Keep hotfixes synchronized across `master` and `refactor/modular-cleanup`, leaving the local checkout on the branch the user expects.
- **Zero Root Clutter:** Never leave `temp_apk*`, `screen_*.png`, or `logcat*.txt` in the repo root. Use a git-ignored `scratch/` directory and clean up immediately.

---

## 2. Progressive Disclosure: Activate Domain Skills On-Demand
To preserve the `always_on` context budget (`docs/rules.md`), detailed subsystem runbooks live in `.agents/skills/`. Read the corresponding `SKILL.md` before editing that subsystem:
- **Physical Wiring & Breadboard (`VTBB3`)**: `.agents/skills/hardware-experiment-workflow/SKILL.md`
- **RP2350 C Firmware & Dual-Core PIO/DMA (`firmware/`)**: `.agents/skills/pico-rp2350-development/SKILL.md`
- **Full-Duplex 16 kHz I2S Audio & Synth (`firmware/src/audio.c`)**: `.agents/skills/rp2350-audio-i2s/SKILL.md`
- **Android 15 Companion App & Gemma 4 E2B (`android/`)**: `.agents/skills/android-compose-development/SKILL.md`
- **Wearable Raspberry Pi 5 Offline Brain (`tools/`)**: `.agents/skills/pi5-edge-brain-server/SKILL.md`
- **Web Digital Twin, Browser LLM & `README.md` (`docs/`)**: `.agents/skills/web-digital-twin-and-browser-llm/SKILL.md`

---

## 3. Core Cross-Project Guardrails
1. **Hardware Safety & Practical Breadboard Guidance:**
   - Never apply 5 V to RP2350 GPIO (strictly 3.3 V logic). Never mix staff 5V USB power (`VBUS Pin 40` + `1000 µF` cap) with wearable Pi 5 UPS power (`Geekworm X1209` + 4× `18650`). Always instruct **Power Off** before moving jumpers.
   - When placing components on the breadboard `+` (5V) and `−` (GND) rails, remind the user that the bus strips run the entire length of the board (`Rows 1–30`), so any free pair of `+`/`−` holes along the rail works if holes adjacent to `Pin 40`/`Pin 38` are occupied.
2. **Web Digital Twin & Mobile Browser LLM (`docs/`):**
   - Keep public UI strictly in English (`en-US`).
   - Route mobile `SmolLM2-135M` to **WASM SIMD (`q4`, `do_sample: false`)** rather than mobile `WebGPU (q4f16)`, yield background 60 FPS canvases on mobile during inference, cap `maxNewTokens = 40`, stream via `TextStreamer`, protect `Dr.` before sentence splitting, and restore `https://www.linkedin.com/in/andrey-holz/`.
   - After editing `docs/`, sync `?v=` via `python tools/bump_twin_version.py <X.Y.Z>` and run `python tools/test_twin_acceptance.py`.
3. **GitHub `README.md` & Mermaid Parity:**
   - Keep `README.md` aligned with `docs/index.html` (**21 chiptunes**, **14 SFX**, **28 LED patterns**, **6 personas**). Never use escaped quotes `\"`, literal `\n`, or `[...]` inside double-quoted Mermaid labels.

---

## 4. Multi-Agent Team Architecture (2026 Antigravity Subagent Principles)

### A. Single Agent vs. Subagent Team (`invoke_subagent` / `define_subagent` / `/teamwork`)
- **Single-Agent Mode (Default for <= 3 files):** Handle quick fixes, single-subsystem edits, and interactive hardware guidance directly without spawning subagents.
- **Multi-Agent Team Mode (Cross-Stack Features & Phase 1–3 Modular Refactoring):**
  - Use `invoke_subagent` (or suggest `/teamwork`) when tasks span **disjoint directories** (`firmware/`, `android/`, `tools/`, `docs/`) or when a broad read-only audit would clutter the main context window.
  - **System Subagent Rules (`<subagents>` compliance):**
    1. **Default `Model: 'inherit'`:** Always pass `Model: 'inherit'` to `invoke_subagent` unless the user explicitly asks for a specific model tier (`flash_lite`, `flash`, or `pro`).
    2. **Built-in `TypeName` vs Custom `define_subagent`:** Use `TypeName: 'research'` for read-only codebase/log exploration and `TypeName: 'self'` (with a specific 2–5 word `Role` title) for write-enabled subsystem tasks—or call `define_subagent` first if a custom subagent type will be invoked repeatedly.
    3. **No Polling & Workspace Isolation:** Never poll after `invoke_subagent` (reactive wakeup notifies you automatically). When parallel write-enabled subagents run simultaneously, isolate them by strictly disjoint directory scopes (`firmware/` vs `android/`) or `Workspace: 'share'`.

### B. Canonical Subagent Roles for Celestial Engine Mk.V

| `Role` (2–5 Words) | `TypeName` | Write Access | Skill to Activate in Prompt | Scope & Verification |
| :--- | :--- | :---: | :--- | :--- |
| **`Codebase & Log Researcher`** | `'research'` | No (Read-Only) | None | Audits pinouts, experiment logs (`docs/experiments/`), and protocol constants without polluting main context. |
| **`RP2350 Firmware Engineer`** | `'self'` | Yes (`firmware/`, `tests/`) | `pico-rp2350-development`, `rp2350-audio-i2s` | Modularizes C/PIO/DMA code, verifies `cmake --build firmware/build` and `python -m unittest discover -s tests -v`. |
| **`Android Companion Engineer`** | `'self'` | Yes (`android/`) | `android-compose-development` | Modularizes Kotlin/Compose, Hotspot UDP `:5005`, `Gemma 4 E2B`, `[SPELL: ...]`, and verifies `./gradlew assembleDebug`. |
| **`Web Digital Twin Specialist`** | `'self'` | Yes (`docs/`, `README.md`) | `web-digital-twin-and-browser-llm` | Maintains `docs/`, `worker.js`, `README.md`, runs `bump_twin_version.py` and `test_twin_acceptance.py`. |
| **`Pi 5 Edge Brain Engineer`** | `'self'` | Yes (`tools/`) | `pi5-edge-brain-server` | Updates `tools/ai_brain_server.py`, `local_ai_engine.py`, and `pi5_web_console.py` (`Moonshine` + `Ollama` + `Kokoro-82M` + Web Lab `:8080`). |
| **`QA & Parity Verifier`** | `'self'` | Test Runner | `web-digital-twin-and-browser-llm` | Runs `python tools/test_twin_acceptance.py`, `unittest`, and checks `git status -sb` before merges to `master`. |
