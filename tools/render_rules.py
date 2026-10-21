#!/usr/bin/env python3
"""Собирает боевые правила базы из firebase.rules.json.

В репозитории лежит шаблон: вместо секрета записи там плейсхолдер
__CB4_WRITE_SECRET__ (11 мест). Этот скрипт подставляет настоящее значение из
окружения и пишет готовый файл для публикации в консоли Firebase
(Realtime Database → Rules → вставить → Publish):

    CB4_WRITE_SECRET=... python3 tools/render_rules.py
    # -> firebase.rules.rendered.json (в .gitignore, в git не попадает)

Без CB4_WRITE_SECRET подставится публичное тестовое значение из
tools/gen_secrets.py — такой файл годится только для проверок.
"""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / "firebase.rules.json"
OUT = ROOT / "firebase.rules.rendered.json"
PLACEHOLDER = "__CB4_WRITE_SECRET__"
TEST_WRITE_SECRET = "Cb4TestWriteSecret0123456789abcd"


def main() -> int:
    secret = os.environ.get("CB4_WRITE_SECRET", "")
    from_env = bool(secret)
    if not from_env:
        secret = TEST_WRITE_SECRET
    text = SOURCE.read_text(encoding="utf-8")
    hits = text.count(PLACEHOLDER)
    if hits < 6:
        raise SystemExit(f"В firebase.rules.json всего {hits} плейсхолдеров — шаблон правился вручную?")
    rendered = text.replace(PLACEHOLDER, secret)
    json.loads(rendered)  # не пишем битую сборку
    OUT.write_text(rendered, encoding="utf-8")
    print("firebase.rules.rendered.json: %d подстановок, значение из %s"
          % (hits, "GitHub Secrets/окружения" if from_env else "ТЕСТОВОЕ (публичное)"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
