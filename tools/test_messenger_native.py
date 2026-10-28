#!/usr/bin/env python3
"""Host syntax checks for the native messenger, JNI bridge, and shared runtime."""
from __future__ import annotations

import os
import shlex
import shutil
import subprocess

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def compiler_command() -> list[str]:
    configured = shlex.split(os.environ.get("CC", ""))
    for candidate in (configured, ["clang"], ["gcc"], ["cc"]):
        if candidate and shutil.which(candidate[0]):
            return candidate
    raise RuntimeError("A host C compiler (Clang or GCC) is required")


def main() -> int:
    command = compiler_command() + [
        "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic",
        "-Wno-missing-field-initializers", "-Wno-unused-function", "-D__ANDROID__",
        "-Itools/host_test/stub", "-Isrc", "-I.", "-fsyntax-only",
        "src/messenger/app.c",
        "src/platform/android/messenger_bridge.c",
        "src/engine/runtime.c",
    ]
    subprocess.run(command, cwd=ROOT, check=True)
    print("messenger native C/JNI/runtime syntax: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
