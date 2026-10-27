#!/usr/bin/env python3
"""Check that APK staging ships only the messenger's required brand assets."""
from __future__ import annotations

import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import stage_assets


def run() -> None:
    with tempfile.TemporaryDirectory() as temp:
        output = Path(temp) / "apk-assets"
        staged = stage_assets.stage_assets(ROOT / "assets", output)
        assert Path("icons/ponedelnik_messenger.png") in staged
        assert Path("fonts/DejaVuSans.ttf") in staged
        assert (output / "icons/ponedelnik_messenger.png").is_file()
        assert (output / "fonts/DejaVuSans.ttf").is_file()
        assert not (output / "textures").exists()
        assert not (output / "sounds").exists()
    print("messenger assets: ok")


if __name__ == "__main__":
    run()
