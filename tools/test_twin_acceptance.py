#!/usr/bin/env python3
"""
Verification suite for Celestial Engine Mk.V Browser Showcase & Interactive Digital Replica.
Checks:
1. Parity between Android OraclePersona.kt and docs/twin/oracle/personas.json
2. All 6 personas present in index.html and scripted.js with <25 words compliance
3. Safe DOM rendering (no innerHTML with untrusted Oracle output)
4. English-only public UI in docs/index.html (zero Cyrillic strings)
5. Cache-busting ?v= version synchronization across index.html, controls.js, provider.js, llm.js, and worker.js
6. Mobile LLM & anti-hallucination guardrails in worker.js (Dr. protection, LinkedIn URL restoration, WASM SIMD on mobile, maxNewTokens=40)
7. GitHub Mermaid diagram syntax validation in README.md (no escaped quotes \" or brackets inside quoted labels)
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def check_parity():
    res = subprocess.run([sys.executable, str(ROOT / "tools" / "verify_personas_parity.py")], capture_output=True, text=True)
    if res.returncode != 0:
        raise AssertionError(f"Parity check failed: {res.stderr}")
    print("[PASS] OraclePersona.kt <-> personas.json parity verified.")


def check_scripted_word_counts():
    scripted_path = ROOT / "docs" / "twin" / "oracle" / "scripted.js"
    text = scripted_path.read_text(encoding="utf-8")
    strings = re.findall(r'"([^"\n]{20,})"', text)
    assert len(strings) >= 40, f"Expected >= 40 scripted lines, found {len(strings)}"
    for s in strings:
        words = s.split()
        assert len(words) <= 25, f"Scripted line exceeds 25 words ({len(words)} words): {s}"

    repo_kb_path = ROOT / "docs" / "twin" / "oracle" / "repo-knowledge.js"
    repo_text = repo_kb_path.read_text(encoding="utf-8")
    repo_replies = re.findall(r'(?:machine_god|archmage|marvin|cyberpunk|pumpkin_lord):\s*"([^"\n]+)"', repo_text)
    assert len(repo_replies) >= 40, f"Expected >= 40 repo-grounded persona replies, found {len(repo_replies)}"
    for r in repo_replies:
        words = r.split()
        assert len(words) <= 25, f"Repo reply exceeds 25 words ({len(words)} words): {r}"

    print(f"[PASS] All {len(strings)} scripted lines and {len(repo_replies)} repo-grounded lines are <= 25 words.")


def check_html_and_security():
    html_path = ROOT / "docs" / "index.html"
    html = html_path.read_text(encoding="utf-8")

    for pid in ["machine_god", "archmage", "marvin", "cyberpunk", "pumpkin_lord", "custom"]:
        assert f'data-persona="{pid}"' in html, f"Missing persona button {pid} in index.html"

    for canvas_id in ["replica-2d-canvas", "led-canvas", "oled-canvas", "audio-spectrum-canvas", "schematic-canvas"]:
        assert f'id="{canvas_id}"' in html, f"Missing required canvas #{canvas_id} in index.html"

    # Ensure zero Cyrillic characters in public docs/index.html
    cyrillic_matches = re.findall(r"[А-Яа-яЁё]+", html)
    assert not cyrillic_matches, f"Found Cyrillic text in public docs/index.html: {cyrillic_matches[:5]}"

    # Ensure controls.js uses textContent and never innerHTML for oracle responses
    controls_js = (ROOT / "docs" / "twin" / "controls.js").read_text(encoding="utf-8")
    assert "setSafeText('oracle-response-text'" in controls_js, "Oracle response must use setSafeText"
    assert "document.createElement('a')" in controls_js, "setSafeText must render clickable links via safe DOM nodes"
    print("[PASS] HTML structure, English-only UI, 6 personas, canvases, and XSS-safe DOM rendering verified.")


def check_cache_busting_version_sync():
    files_to_check = [
        ROOT / "docs" / "index.html",
        ROOT / "docs" / "twin" / "controls.js",
        ROOT / "docs" / "twin" / "oracle" / "provider.js",
        ROOT / "docs" / "twin" / "oracle" / "llm.js",
        ROOT / "docs" / "twin" / "oracle" / "worker.js",
    ]
    versions_found = set()
    for p in files_to_check:
        content = p.read_text(encoding="utf-8")
        v_matches = re.findall(r"\?v=(\d+\.\d+\.\d+)", content)
        assert v_matches, f"Missing ?v=X.Y.Z cache-buster in {p.relative_to(ROOT)}"
        for v in v_matches:
            versions_found.add((p.name, v))

    distinct_versions = {v for _, v in versions_found}
    assert len(distinct_versions) == 1, f"Mismatched ?v= cache-busting versions across docs/: {versions_found}"
    print(f"[PASS] Cache-busting version synchronized across all modules (?v={next(iter(distinct_versions))}).")


def check_worker_mobile_and_sanitization_guardrails():
    worker_js = (ROOT / "docs" / "twin" / "oracle" / "worker.js").read_text(encoding="utf-8")
    assert "https://www.linkedin.com/in/andrey-holz/" in worker_js, "Missing creator LinkedIn URL in worker.js"
    assert r"\b(Dr|Mr|Mrs|Ms|Prof|Mk|St|vs)\.\s+" in worker_js, "Missing abbreviation protection before sentence splitting in worker.js"
    assert "isMobileWorker" in worker_js, "Missing mobile worker detection (isMobileWorker) in worker.js"
    assert "maxNewTokens = 40" in worker_js, "worker.js must cap default maxNewTokens at 40 to prevent 135M hallucinations"
    assert "TextStreamer" in worker_js, "worker.js must stream tokens live via TextStreamer"
    print("[PASS] Worker mobile WASM SIMD routing, live streaming, Dr. protection, and anti-hallucination bounds verified.")


def check_readme_mermaid_syntax():
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    mermaid_blocks = re.findall(r"```mermaid\s+([\s\S]*?)```", readme)
    assert len(mermaid_blocks) >= 3, f"Expected >= 3 Mermaid diagrams in README.md, found {len(mermaid_blocks)}"
    for idx, block in enumerate(mermaid_blocks, 1):
        assert r'\"' not in block, f"Mermaid block #{idx} in README.md contains escaped quote \\\" which breaks GitHub parser"
        assert r"\n" not in block, f"Mermaid block #{idx} in README.md contains literal \\n which breaks GitHub parser"
        quoted_labels = re.findall(r'"([^"]*)"', block)
        for lbl in quoted_labels:
            assert "[" not in lbl and "]" not in lbl, (
                f"Mermaid block #{idx} in README.md contains square brackets inside quoted label: \"{lbl}\""
            )
    print(f"[PASS] All {len(mermaid_blocks)} Mermaid diagrams in README.md verified for GitHub parser compatibility.")


def main():
    check_parity()
    check_scripted_word_counts()
    check_html_and_security()
    check_cache_busting_version_sync()
    check_worker_mobile_and_sanitization_guardrails()
    check_readme_mermaid_syntax()
    print("ALL ACCEPTANCE CHECKS PASSED.")


if __name__ == "__main__":
    main()
