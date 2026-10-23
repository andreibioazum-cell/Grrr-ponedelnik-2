#!/usr/bin/env python3
"""Builds the 60 FPS WebAssembly + WebGL2 browser edition of Cubic Battle 4: 3D Minecraft."""
from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "web"

C_SOURCES = (
    "src/game/game.c",
    "src/engine/runtime.c",
    "src/platform/web/web_main.c",
)
CPP_SOURCES = ("src/game/cpp/ui_fx.cpp",)
INCLUDES = ("-I.", "-Isrc", "-Isrc/platform/web/shim", "-Isrc/platform/pc/shim")


def zig_driver() -> list[str]:
    found = shutil.which("zig")
    if found:
        return [found]
    cand = Path.home() / ".local" / "bin" / "zig"
    if cand.is_file():
        return [str(cand)]
    return [sys.executable, "-m", "ziglang"]


def run(cmd: list[str]) -> None:
    print("[web]", " ".join(cmd), flush=True)
    res = subprocess.run(cmd, cwd=ROOT)
    if res.returncode != 0:
        raise SystemExit(res.returncode)


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    staged = OUT / "assets"
    shutil.rmtree(staged, ignore_errors=True)
    run([sys.executable, "stage_assets.py", "assets", str(staged)])

    zig = zig_driver()
    objs: list[str] = []
    for src in C_SOURCES:
        obj = OUT / (Path(src).stem + ".o")
        run([*zig, "cc", "-target", "wasm32-wasi", "-std=c99", "-DCB_PC", "-DCB_WASM", "-O2", *INCLUDES, "-c", src, "-o", str(obj)])
        objs.append(str(obj))
    for src in CPP_SOURCES:
        obj = OUT / (Path(src).stem + ".o")
        run([*zig, "c++", "-target", "wasm32-wasi", "-std=c++17", "-DCB_PC", "-DCB_WASM", "-O2", *INCLUDES, "-c", src, "-o", str(obj)])
        objs.append(str(obj))

    wasm_out = OUT / "cb4.wasm"
    run([*zig, "c++", "-target", "wasm32-wasi", "-mexec-model=reactor", "-O2", *objs, "-o", str(wasm_out)])
    for o in objs:
        Path(o).unlink(missing_ok=True)

    asset_files = sorted(str(p.relative_to(OUT)).replace(os.sep, "/") for p in staged.rglob("*") if p.is_file())
    (OUT / "assets_manifest.json").write_text(json.dumps(asset_files, indent=2), encoding="utf-8")

    shutil.copy2(ROOT / "web" / "play.html", OUT / "play.html")
    shutil.copy2(ROOT / "web" / "worker.js", OUT / "worker.js")
    shutil.copy2(ROOT / "index.html", OUT / "admin.html")
    print(f"[web] Ready: {wasm_out} ({wasm_out.stat().st_size:,} bytes), {len(asset_files)} assets.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
