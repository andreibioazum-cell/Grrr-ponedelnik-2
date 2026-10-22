#!/usr/bin/env python3
"""Builds and checks the per-frame cleanup of the runtime string pool."""

from __future__ import annotations

import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    compiler = os.environ.get("CC", "clang")
    with tempfile.TemporaryDirectory(prefix="cb4-pool-") as directory:
        executable = Path(directory) / "test_string_pool"
        subprocess.run(
            [
                compiler,
                "-std=c99",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-pedantic",
                "-I",
                str(ROOT / "src"),
                "-I",
                str(ROOT),
                "-I",
                str(ROOT / "tools/host_test/stub"),
                str(ROOT / "tools/test_string_pool.c"),
                "-lpthread",
                "-o",
                str(executable),
            ],
            cwd=ROOT,
            check=True,
        )
        subprocess.run([str(executable)], cwd=ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
