#!/usr/bin/env python3
"""
Fast (<1s) zero-dependency CI & pre-push preflight check for Celestial Engine Mk.V.
Catches recurring compiler and test traps before triggering cloud Gradle or firmware builds:
1. Kotlin 2.0 experimental inline lambda `break` / `continue` inside `.use { ... }` blocks.
2. Illegal Android JVM bytecode characters (`<`, `>`, `.`, `;`, `[`, `]`, `/`) inside JUnit 4
   backtick test method names (`fun `...`()`).
3. Valid JSON syntax in `tools/brain_config.example.json` and `docs/twin/oracle/personas.json`.
"""

from pathlib import Path
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]

ILLEGAL_JUNIT_BACKTICK_CHARS = set("<>.;[]/")
BACKTICK_FUN_RE = re.compile(r"fun\s+`([^`]+)`\s*\(")
USE_BLOCK_START_RE = re.compile(r"\.use\s*\{")


def check_junit_backtick_names() -> list[str]:
    errors: list[str] = []
    test_root = ROOT / "android/app/src/test"
    if not test_root.exists():
        return errors
    for kt_file in test_root.rglob("*.kt"):
        text = kt_file.read_text(encoding="utf-8")
        for lineno, line in enumerate(text.splitlines(), start=1):
            m = BACKTICK_FUN_RE.search(line)
            if m:
                name = m.group(1)
                bad = sorted(ch for ch in set(name) if ch in ILLEGAL_JUNIT_BACKTICK_CHARS)
                if bad:
                    rel = kt_file.relative_to(ROOT)
                    errors.append(
                        f"{rel}:{lineno}: Illegal JVM bytecode character(s) {bad} in backtick test name `{name}`"
                    )
    return errors


def check_kotlin_inline_use_break_continue() -> list[str]:
    errors: list[str] = []
    src_root = ROOT / "android/app/src/main"
    if not src_root.exists():
        return errors
    for kt_file in src_root.rglob("*.kt"):
        lines = kt_file.read_text(encoding="utf-8").splitlines()
        for idx, line in enumerate(lines):
            if USE_BLOCK_START_RE.search(line):
                # Inspect the local .use { ... } body up to matching brace or 30 lines
                brace_depth = line.count("{") - line.count("}")
                for j in range(idx + 1, min(len(lines), idx + 35)):
                    sub = lines[j]
                    stripped = sub.split("//")[0]
                    if re.search(r"\b(break|continue)\b", stripped) and not re.search(r"\b(for|while)\s*\(", stripped):
                        # Check if a local loop was opened inside the .use block
                        inner_slice = "\n".join(lines[idx + 1 : j + 1])
                        if not re.search(r"\b(for|while)\s*\(", inner_slice):
                            rel = kt_file.relative_to(ROOT)
                            errors.append(
                                f"{rel}:{j + 1}: Non-local `break`/`continue` inside `.use {{ ... }}` inline lambda fails on Kotlin 2.0"
                            )
                    brace_depth += stripped.count("{") - stripped.count("}")
                    if brace_depth <= 0:
                        break
    return errors


def check_json_configs() -> list[str]:
    errors: list[str] = []
    for rel_path in ("tools/brain_config.example.json", "docs/twin/oracle/personas.json"):
        p = ROOT / rel_path
        if p.exists():
            try:
                json.loads(p.read_text(encoding="utf-8"))
            except Exception as exc:
                errors.append(f"{rel_path}: Invalid JSON ({exc})")
    return errors


def main() -> int:
    all_errors: list[str] = []
    all_errors.extend(check_junit_backtick_names())
    all_errors.extend(check_kotlin_inline_use_break_continue())
    all_errors.extend(check_json_configs())

    if all_errors:
        print("PREFLIGHT CHECK FAILED:")
        for err in all_errors:
            print(f"  [ERROR] {err}")
        return 1

    print("PREFLIGHT CHECK PASSED: Kotlin 2.0, JUnit 4 backtick names, and JSON schemas OK.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
