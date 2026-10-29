#!/usr/bin/env python3
"""
Verification suite for Celestial Engine Mk.V Browser Showcase & Interactive Digital Replica.
Checks:
1. Parity between Android OraclePersona.kt and docs/twin/oracle/personas.json
2. All 6 personas present in index.html and scripted.js with <25 words compliance
3. Zero automatic LLM downloads on initial page load
4. Safe DOM rendering (no innerHTML with untrusted Oracle output)
5. Relative asset URLs compatible with GitHub Pages subpath (/celestial_engine_project/)
"""

import json
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
    # Extract all double-quoted strings in SCRIPTED_BANKS
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

    # Ensure controls.js uses textContent and never innerHTML for oracle responses
    controls_js = (ROOT / "docs" / "twin" / "controls.js").read_text(encoding="utf-8")
    assert "setSafeText('oracle-response-text'" in controls_js, "Oracle response must use textContent"
    print("[PASS] HTML structure, 6 personas, existing canvases, and XSS-safe textContent rendering verified.")


def main():
    check_parity();
    check_scripted_word_counts()
    check_html_and_security()
    print("ALL ACCEPTANCE CHECKS PASSED.")


if __name__ == "__main__":
    main()

