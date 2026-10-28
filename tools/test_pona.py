#!/usr/bin/env python3
"""Host checks for the Pona AOT compiler, incremental GC, and input state."""

from __future__ import annotations

import os
import shlex
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))

from ponac import PonaError, compile_source  # noqa: E402

FEATURE_SCRIPT = r'''global title: str = nil
function first(): str
    return later()
end
function later(): str
    return "quoted: \"Pona\" — UTF-8 🌍"
end
function draw()
    local integer: i32 = 5 % 2
    local fraction: f32 = 5.5 % 2.0
    local half: f32 = 1 / 2
    local label: str = first()
    if integer == 1 and fraction > half and label == "quoted: \"Pona\" — UTF-8 🌍" then
        text(label, 0.0, 0.0, #FFFFFF)
    end
    if not integer == 1 then
        text(label, 0.0, 16.0, #FFFFFF)
    end
end
'''


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def c_compiler() -> list[str]:
    configured = shlex.split(os.environ.get("CC", ""))
    candidates = [configured, ["clang"], ["gcc"], ["cc"]]
    for candidate in candidates:
        if candidate and shutil.which(candidate[0]):
            return candidate
    raise RuntimeError("A host C compiler (Clang or GCC) is required for Pona tests")


def run(command: list[str]) -> None:
    subprocess.run(command, cwd=ROOT, check=True)


def main() -> int:
    demo = ROOT / "examples" / "pond2d" / "main.pona"
    run([sys.executable, str(ROOT / "tools" / "ponac.py"), str(demo), "--check"])
    demo_c = compile_source(demo.read_text(encoding="utf-8"), str(demo))
    feature_c = compile_source(FEATURE_SCRIPT, "compiler-features.pona")

    check("pona_script_update(float delta)" in demo_c, "update wrapper missing from generated C")
    check("pona_user_draw();" in demo_c, "draw wrapper missing from generated C")
    check("p2d_array_length(" in demo_c, "array length should use the typed i32 adapter")
    user_code = demo_c.split("void pona_script_init", 1)[0]
    check('pona_string_new("left")' not in user_code, "string literals must not allocate each frame")
    check("fmodf(" in feature_c, "floating-point modulo was not lowered to fmodf")
    check("((float)(1) / (float)(2))" in feature_c, "integer operands must still use floating-point division")
    check("\\360\\237\\214\\215" in feature_c, "UTF-8 string literal should be emitted as byte escapes")

    invalid_programs = (
        "function update(delta: i32) end",
        "function draw()\n    clear(42)\nend",
        "function draw()\n    missing()\nend",
        "global broken: f32 = missing\n",
        "global while: i32 = 1\n",
        "function draw()\n    local value = nil\nend",
    )
    for source in invalid_programs:
        try:
            compile_source(source, "invalid.pona")
        except PonaError:
            continue
        raise AssertionError(f"invalid Pona program unexpectedly compiled: {source!r}")

    compiler = c_compiler()
    warning_flags = ["-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic", "-Isrc", "-I."]
    platform_flags = warning_flags + [
        "-Wno-missing-field-initializers", "-Wno-unused-function", "-Itools/host_test/stub",
    ]
    run(compiler + platform_flags + ["-fsyntax-only",
        "src/engine2d/api.c", "src/engine2d/app.c", "src/engine2d/gc.c",
        "src/engine2d/input.c", "src/engine/runtime.c",
    ])

    with tempfile.TemporaryDirectory(prefix="pona-host-test-") as temporary:
        temp = Path(temporary)
        generated_paths = {}
        for name, source in (("demo", demo_c), ("features", feature_c)):
            generated = temp / f"{name}.c"
            generated.write_text(source, encoding="utf-8")
            generated_paths[name] = generated
            run(compiler + warning_flags + ["-fsyntax-only", str(generated)])

        script_binary = temp / "test_pona_script"
        run(compiler + platform_flags + [
            "tools/host_test/test_pona_script.c", str(generated_paths["demo"]),
            "src/engine2d/api.c", "src/engine2d/gc.c", "src/engine2d/input.c",
            "-lm", "-o", str(script_binary),
        ])
        run([str(script_binary)])

        gc_binary = temp / "test_pona_gc"
        run(compiler + warning_flags + [
            "tools/host_test/test_pona_gc.c", "src/engine2d/gc.c", "-o", str(gc_binary),
        ])
        run([str(gc_binary)])

        input_binary = temp / "test_pona_input"
        run(compiler + warning_flags + [
            "tools/host_test/test_pona_input.c", "src/engine2d/input.c", "-o", str(input_binary),
        ])
        run([str(input_binary)])

    print("Pona AOT C output, incremental GC, and input tests: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
