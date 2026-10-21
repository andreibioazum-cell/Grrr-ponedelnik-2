#!/usr/bin/env python3
"""Генерирует src/engine/network/secrets_generated.h из окружения.

Зачем: репозиторий публичный, поэтому настоящих значений секретов базы в нём
быть не должно. Они живут в GitHub Actions secrets (Settings → Secrets and
variables → Actions) и подставляются только на время сборки:

    CB4_WRITE_SECRET  — «пароль на запись» (поле w в правилах базы);
    CB4_PASS_SECRET   — хвост пароля аккаунта (пароль игрока + этот хвост).

CI передаёт оба секрета в этот скрипт перед сборкой APK и .exe; сгенерированный
заголовок в .gitignore и в git не попадает. Локально значения можно передать так:

    CB4_WRITE_SECRET=... CB4_PASS_SECRET=... python3 tools/gen_secrets.py

Без переменных окружения подставляются ПУБЛИЧНЫЕ ТЕСТОВЫЕ значения: с ними
игра собирается и ходит в подставную базу (test/fake_firebase.py), но боевая
база такие записи отвергнет. Это осознанно: форк или локальная сборка не должны
молчком получать боевые секреты.

Сами значения скрипт не печатает — только факт, откуда они взяты.
"""

from __future__ import annotations

import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "src" / "engine" / "network" / "secrets_generated.h"

# Публичные тестовые значения. Совпадают с test/fake_firebase.py (SECRET).
TEST_WRITE_SECRET = "Cb4TestWriteSecret0123456789abcd"
TEST_PASS_SECRET = "Cb4TestPassSecret0123456789abcde"

MASK = 0x5A
CHUNK = 12


def encode(secret: str) -> list[str]:
    """Режет секрет на куски, каждый байт — восьмеричный escape после xor."""
    parts = []
    for start in range(0, len(secret), CHUNK):
        piece = secret[start:start + CHUNK]
        octets = []
        for char in piece:
            value = ord(char) ^ MASK
            if value == 0:
                raise SystemExit("секрет содержит символ, кодирующийся в ноль ('Z')")
            octets.append("\\%03o" % value)
        parts.append('"%s"' % "".join(octets))
    return parts


def check(name: str, value: str) -> str:
    if not re.fullmatch(r"[A-Za-z0-9]{24,64}", value or ""):
        raise SystemExit(f"{name}: нужна строка из букв и цифр, 24-64 символа")
    return value


def defines(prefix: str, secret: str) -> list[str]:
    lines = [f"#define {prefix}_LEN {len(secret)}"]
    for index, chunk in enumerate(encode(secret)):
        lines.append(f"#define {prefix}_{index} {chunk}")
    return lines


def main() -> int:
    write_secret = os.environ.get("CB4_WRITE_SECRET", "")
    pass_secret = os.environ.get("CB4_PASS_SECRET", "")
    from_env = bool(write_secret and pass_secret)
    if bool(write_secret) != bool(pass_secret):
        raise SystemExit("CB4_WRITE_SECRET и CB4_PASS_SECRET задаются только парой")
    if not from_env:
        write_secret, pass_secret = TEST_WRITE_SECRET, TEST_PASS_SECRET
    check("CB4_WRITE_SECRET", write_secret)
    check("CB4_PASS_SECRET", pass_secret)
    if write_secret == pass_secret:
        raise SystemExit("Секреты должны быть разными: секрет записи лежит в базе открыто")

    lines = [
        "/* СГЕНЕРИРОВАНО tools/gen_secrets.py — руками не править, в git не коммитить. */",
        "#ifndef CB4_SECRETS_GENERATED_H",
        "#define CB4_SECRETS_GENERATED_H",
        *defines("NET_WRITE_SECRET", write_secret),
        *defines("NET_PASS_SECRET", pass_secret),
        "#endif",
        "",
    ]
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print("secrets_generated.h: значения из %s" % ("GitHub Secrets/окружения" if from_env else "ТЕСТОВЫЕ (публичные)"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
