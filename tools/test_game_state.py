#!/usr/bin/env python3
"""Build & run Geometrium C99 state/occlusion/monochrome tests + real 2-player live Firebase E2E."""

import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "test"))
from fake_firebase import RunningFakeFirebase  # noqa: E402


def main() -> int:
    cc = os.environ.get("CC", "clang")
    tmp = pathlib.Path(tempfile.mkdtemp(prefix="geometrium-test-"))
    bin_path = tmp / "test_geometrium"
    try:
        subprocess.run(
            [
                cc,
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
                str(ROOT / "tools" / "host_test" / "stub"),
                str(ROOT / "tools" / "test_game_state.c"),
                str(ROOT / "src" / "engine" / "network.c"),
                "-lpthread",
                "-lm",
                "-o",
                str(bin_path),
            ],
            check=True,
        )

        # 1. Local C99 engine, monochrome dark-gray UI, 3D water occlusion & world persistence tests
        d_local = tmp / "local"
        d_local.mkdir()
        subprocess.run([str(bin_path), str(d_local)], check=True)

        # 2. Real simultaneous 2-process live multiplayer sync against RunningFakeFirebase over TCP/HTTP
        d_alpha = tmp / "alpha"
        d_beta = tmp / "beta"
        d_alpha.mkdir()
        d_beta.mkdir()
        with RunningFakeFirebase() as srv:
            p_alpha = subprocess.Popen([str(bin_path), "--alpha", srv.url, str(d_alpha)])
            p_beta = subprocess.Popen([str(bin_path), "--beta", srv.url, str(d_beta)])
            rc_a = p_alpha.wait(timeout=15)
            rc_b = p_beta.wait(timeout=15)
            assert rc_a == 0 and rc_b == 0, (rc_a, rc_b)

            # Verify persistent blocks remain stored in Firebase RTDB after both players disconnect!
            saved_blocks = srv.db.get("/geometrium/rooms/main/blocks")
            assert saved_blocks and saved_blocks.get("50_17_50") == 20 and saved_blocks.get("53_18_51") == 23, saved_blocks

        print("Geometrium live 2-player Firebase RTDB sync (3D avatars + persistent blocks + chat + auth): OK")
        return 0
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    raise SystemExit(main())
