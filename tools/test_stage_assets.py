#!/usr/bin/env python3
"""Exercise generic, path-preserving Android asset staging."""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from stage_assets import stage_assets


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="pona-assets-") as temporary:
        root = Path(temporary)
        source = root / "source"
        destination = root / "staged" / "assets"
        (source / "fonts").mkdir(parents=True)
        (source / "textures" / "characters").mkdir(parents=True)
        (source / "fonts" / "Demo.ttf").write_bytes(b"font-bytes")
        (source / "textures" / "characters" / "hero.png").write_bytes(b"image-bytes")
        staged = stage_assets(source, destination)
        assert staged == [Path("fonts/Demo.ttf"), Path("textures/characters/hero.png")]
        assert (destination / "fonts" / "Demo.ttf").read_bytes() == b"font-bytes"
        assert (destination / "textures" / "characters" / "hero.png").read_bytes() == b"image-bytes"
        (destination / "stale.txt").write_text("stale", encoding="utf-8")
        stage_assets(source, destination)
        assert not (destination / "stale.txt").exists()
    print("Pona asset staging: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
