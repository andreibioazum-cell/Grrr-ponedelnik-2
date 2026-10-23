#!/usr/bin/env python3
"""Static & schema checks for index.html (Geometrium Web Console)."""

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
HTML = (ROOT / "index.html").read_text(encoding="utf-8")


def main() -> int:
    assert "GEOMETRIUM" in HTML
    assert "https://nasksel-9104c-default-rtdb.firebaseio.com" in HTML
    assert "© 2026 Geometrium" in HTML
    assert "эпилепс" in HTML.lower()
    for btn_id in ("dbUrl", "btnRefresh", "btnBackup", "roomInput", "btnClearBlocks", "btnClearChat", "btnClearLog"):
        assert f'id="{btn_id}"' in HTML, f"Missing element #{btn_id}"
    for banned in ("cubic battle", "cubicbattle", "ccp", "cubicbattleserver"):
        assert banned not in HTML.lower(), f"Forbidden legacy reference found in index.html: {banned}"
    print("Geometrium Web Console static check: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
