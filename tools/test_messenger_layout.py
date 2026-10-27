#!/usr/bin/env python3
"""Guard the product boundary: native Vulkan app, no game/Firebase in the APK."""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def run() -> None:
    cmake = (ROOT / "CMakeLists.txt").read_text()
    manifest = (ROOT / "platform/android/AndroidManifest.xml").read_text()
    native = (ROOT / "src/platform/android/android_main.c").read_text()
    app = (ROOT / "src/messenger/app.c").read_text()
    java = "\n".join(p.read_text() for p in (ROOT / "platform/android/java/com/ponedelnik/messenger").glob("*.java"))
    assert "messenger/app.c" in cmake and "src/game" not in cmake
    assert "vulkan" in cmake and "ponedelnik_messenger" in manifest
    assert "messenger_app_draw" in native and "messenger_app_touch" in native
    assert "firebase" not in app.lower() and "firebase" not in java.lower()
    assert "crypto" in java.lower() and "AES/GCM/NoPadding" in java
    assert (ROOT / "src/game").exists() is False
    assert (ROOT / "src/engine/network.c").exists() is False
    assert (ROOT / "src/platform/android/network_http.inc").exists() is False
    assert (ROOT / "FIREBASE.md").exists() is False
    assert (ROOT / "assets/icons/ponedelnik_messenger.png").is_file()
    print("messenger layout: native C/Vulkan, no game/Firebase in Android target")


if __name__ == "__main__":
    run()
