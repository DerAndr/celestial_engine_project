#!/usr/bin/env python3
"""
Extract and verify parity between Android's OraclePersona.kt and docs/twin/oracle/personas.json.
Usage:
  python tools/verify_personas_parity.py          # Verify parity (exits 1 on drift)
  python tools/verify_personas_parity.py --write  # Extract from Kotlin and write personas.json
"""

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
KT_FILE = ROOT / "android" / "app" / "src" / "main" / "java" / "com" / "celestial" / "companion" / "domain" / "model" / "OraclePersona.kt"
JSON_FILE = ROOT / "docs" / "twin" / "oracle" / "personas.json"


def parse_kotlin_personas(kt_text: str):
    """Extract OraclePersona blocks from OraclePersona.kt."""
    pattern = re.compile(
        r'val\s+([A-Z_]+)\s*=\s*OraclePersona\(\s*'
        r'id\s*=\s*"([^"]*)",\s*'
        r'titleEn\s*=\s*"([^"]*)",\s*'
        r'titleRu\s*=\s*"([^"]*)",\s*'
        r'titleDe\s*=\s*"([^"]*)",\s*'
        r'icon\s*=\s*"([^"]*)",\s*'
        r'subtitleEn\s*=\s*"([^"]*)",\s*'
        r'subtitleRu\s*=\s*"([^"]*)",\s*'
        r'subtitleDe\s*=\s*"([^"]*)",\s*'
        r'promptEn\s*=\s*"([^"]*)",\s*'
        r'promptRu\s*=\s*"([^"]*)",\s*'
        r'promptDe\s*=\s*"([^"]*)"\s*\)',
        re.MULTILINE
    )
    personas = []
    for m in pattern.finditer(kt_text):
        const_name, pid, title_en, _, _, icon, sub_en, _, _, prompt_en, _, _ = m.groups()
        personas.append({
            "id": pid,
            "constant": const_name,
            "titleEn": title_en,
            "icon": icon,
            "subtitleEn": sub_en,
            "promptEn": prompt_en,
        })
    return personas


def main():
    if not KT_FILE.exists():
        print(f"ERROR: Kotlin source not found at {KT_FILE}", file=sys.stderr)
        sys.exit(1)

    kt_text = KT_FILE.read_text(encoding="utf-8")
    extracted = parse_kotlin_personas(kt_text)
    if len(extracted) != 6:
        print(f"ERROR: Expected 6 personas in {KT_FILE}, found {len(extracted)}", file=sys.stderr)
        sys.exit(1)

    payload = {
        "source": "android/app/src/main/java/com/celestial/companion/domain/model/OraclePersona.kt",
        "version": "1.2.0",
        "constraints": {
            "maxSentences": 2,
            "maxWords": 25,
            "noMarkdown": True,
            "noEmojis": True
        },
        "personas": extracted
    }

    if "--write" in sys.argv:
        JSON_FILE.parent.mkdir(parents=True, exist_ok=True)
        JSON_FILE.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"Wrote {len(extracted)} personas to {JSON_FILE}")
        return

    if not JSON_FILE.exists():
        print(f"DRIFT DETECTED: {JSON_FILE} does not exist. Run with --write.", file=sys.stderr)
        sys.exit(1)

    current = json.loads(JSON_FILE.read_text(encoding="utf-8"))
    curr_personas = current.get("personas", [])
    if curr_personas != extracted:
        print("DRIFT DETECTED between OraclePersona.kt and personas.json!", file=sys.stderr)
        sys.exit(1)

    print(f"PARITY OK: {len(extracted)} personas match OraclePersona.kt exactly.")


if __name__ == "__main__":
    main()

