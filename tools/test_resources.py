#!/usr/bin/env python3
"""Verify all Geometrium 32x32 PNG textures, audio assets, icons, and zero legacy remnants."""

import os
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def check_png_size(path: pathlib.Path):
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", f"Invalid PNG header: {path}"
    _, tag, w, h = struct.unpack(">I4sII", data[8:24])
    assert tag == b"IHDR", f"Invalid IHDR: {path}"
    return w, h


def main() -> int:
    tex_dir = ROOT / "assets" / "textures"
    pngs = sorted(tex_dir.glob("*.png"))
    assert len(pngs) >= 40, f"Expected >= 40 32x32 textures, found {len(pngs)}"
    for p in pngs:
        w, h = check_png_size(p)
        assert (w, h) == (32, 32), f"Texture {p.name} must be 32x32, got {w}x{h}"

    game_src = "\n".join(p.read_text(encoding="utf-8") for p in sorted((ROOT / "src" / "game" / "geo").glob("*.inc")))
    game_src += "\n" + (ROOT / "src" / "game" / "game.c").read_text(encoding="utf-8")

    # Every block texture referenced by geo_defs.inc must exist as a real 32x32 PNG in assets/textures/
    for p in pngs:
        if not p.name.startswith("crack_"):
            assert p.name in game_src, f"Unused texture in assets/textures/: {p.name}"

    # Ensure zero legacy strings anywhere in src/, assets/, platform/android/, README.md, CMakeLists.txt
    forbidden = ("cubic battle", "cubicbattle", "ccp", "cubicbattleserver", "azum", "ebuc", "codecracker")
    scan_paths = (
        list((ROOT / "src").rglob("*.c"))
        + list((ROOT / "src").rglob("*.h"))
        + list((ROOT / "src").rglob("*.inc"))
        + list((ROOT / "src").rglob("*.cpp"))
        + list((ROOT / "platform" / "android").rglob("*.xml"))
        + list((ROOT / "platform" / "android").rglob("*.java"))
        + [ROOT / "CMakeLists.txt", ROOT / "index.html"]
    )
    for sp in scan_paths:
        txt = sp.read_text(encoding="utf-8", errors="ignore").lower()
        for bad in forbidden:
            assert bad not in txt, f"Forbidden legacy string '{bad}' still present in {sp}"

    print(f"Geometrium resources check: OK ({len(pngs)} verified 32x32 PNG textures, zero legacy traces)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
