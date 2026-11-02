#!/usr/bin/env python3
"""
Automated cache-busting version bumper for the Celestial Engine Mk.V Web Digital Twin.
Usage:
    python tools/bump_twin_version.py 1.3.5
Updates all ?v=X.Y.Z asset/module query parameters across docs/ while preserving file line endings.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TARGET_FILES = [
    ROOT / "docs" / "index.html",
    ROOT / "docs" / "twin" / "controls.js",
    ROOT / "docs" / "twin" / "oracle" / "provider.js",
    ROOT / "docs" / "twin" / "oracle" / "llm.js",
    ROOT / "docs" / "twin" / "oracle" / "worker.js",
]


def bump_version(new_ver: str) -> None:
    if not re.match(r"^\d+\.\d+\.\d+$", new_ver):
        raise ValueError(f"Version must be X.Y.Z, got: {new_ver}")

    for path in TARGET_FILES:
        raw = path.read_bytes().decode("utf-8")
        updated = re.sub(r"\?v=\d+\.\d+\.\d+", f"?v={new_ver}", raw)
        if updated != raw:
            path.write_bytes(updated.encode("utf-8"))
        print(f"[UPDATED] {path.relative_to(ROOT)} -> ?v={new_ver}")


if __name__ == "__main__":
    ver = sys.argv[1] if len(sys.argv) > 1 else "1.3.4"
    bump_version(ver)
