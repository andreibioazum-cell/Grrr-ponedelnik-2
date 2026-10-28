#!/usr/bin/env python3
"""Compile and execute the platform-independent G.711 voice codec test."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
JAVA = ROOT / "platform/android/java/com/ponedelnik/messenger/VoiceCodec.java"
TEST = ROOT / "tools/host_test/VoiceCodecTest.java"


def main() -> int:
    if not shutil.which("javac") or not shutil.which("java"):
        print("voice codec: skipped (Java runtime/compiler not installed)")
        return 0
    with tempfile.TemporaryDirectory(prefix="ponedelnik-voice-codec-") as output:
        subprocess.run(
            ["javac", "-d", output, str(JAVA), str(TEST)],
            check=True,
            cwd=ROOT,
        )
        subprocess.run(
            ["java", "-cp", output, "com.ponedelnik.messenger.VoiceCodecTest"],
            check=True,
            cwd=ROOT,
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
