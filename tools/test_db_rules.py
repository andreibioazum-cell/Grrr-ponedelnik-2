#!/usr/bin/env python3
"""Verify firebase.rules.json and network endpoint configuration for Geometrium."""

import json
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def main() -> int:
    rules = json.loads((ROOT / "firebase.rules.json").read_text(encoding="utf-8"))
    assert rules == {"rules": {".read": True, ".write": True}}, rules

    net_h = (ROOT / "src" / "engine" / "network.h").read_text(encoding="utf-8")
    assert 'https://nasksel-9104c-default-rtdb.firebaseio.com' in net_h
    print("Firebase rules & endpoint check: OK (https://nasksel-9104c-default-rtdb.firebaseio.com)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
