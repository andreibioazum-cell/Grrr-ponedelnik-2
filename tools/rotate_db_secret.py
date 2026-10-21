#!/usr/bin/env python3
"""Ротация секретов базы: «пароль на запись» (поле w) и хвост пароля аккаунта.

Новая схема: настоящие значения НЕ пишутся в репозиторий. Они живут в
GitHub Actions secrets и подставляются в сборку скриптом tools/gen_secrets.py;
локальная копия для владельца — в SECRETS.local.txt (файл в .gitignore).
Шаблон правил firebase.rules.json содержит плейсхолдер __CB4_WRITE_SECRET__,
боевой файл собирает tools/render_rules.py.

Что делает этот скрипт:
  1. Придумывает два новых секрета по 32 случайных символа [A-Za-z0-9]
     (криптографический генератор, модуль secrets).
  2. Записывает предыдущий хвост пароля ступенькой миграции в
     src/engine/network/write_secret.inc (fb_legacy_pass_suffix): старые
     аккаунты один раз войдут по нему и сами перепишут пароль на новый.
  3. Пишет все значения и готовые строки в SECRETS.local.txt и пытается
     положить секреты в GitHub сам (`gh secret set`); если у токена нет
     прав — печатает, что именно вставить в Settings → Secrets and variables.
  4. Рендерит firebase.rules.rendered.json с новым секретом записи.

После запуска обязательно (это действия в чужих аккаунтах, скрипт их не умеет):
  * вставить секреты в GitHub, если скрипт не смог сам;
  * опубликовать firebase.rules.rendered.json в консоли Firebase
    (Realtime Database → Rules → Publish) ДО раздачи новой сборки;
  * сменить пароли админских аккаунтов (готовые строки — в SECRETS.local.txt);
  * пересобрать игру (CI подставит новые секреты сам).

Старые значения после ротации дописываются в .secrets-deny и в секрет
CB4_SECRET_DENY, чтобы сторож tools/check_secrets.py ловил их возврат в код.

Запуск:
    python3 tools/rotate_db_secret.py            # полная ротация
    python3 tools/rotate_db_secret.py --show     # что сейчас в generated-заголовке
"""

from __future__ import annotations

import argparse
import json
import os
import re
import secrets
import string
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
GENERATED = ROOT / "src" / "engine" / "network" / "secrets_generated.h"
HEADER = ROOT / "src" / "engine" / "network.h"
WRITE_SECRET_INC = ROOT / "src" / "engine" / "network" / "write_secret.inc"
RULES = ROOT / "firebase.rules.json"
RENDERED = ROOT / "firebase.rules.rendered.json"
LOCAL_FILE = ROOT / "SECRETS.local.txt"
DENY_FILE = ROOT / ".secrets-deny"

TEST_WRITE_SECRET = "Cb4TestWriteSecret0123456789abcd"
TEST_PASS_SECRET = "Cb4TestPassSecret0123456789abcde"
PLACEHOLDER = "__CB4_WRITE_SECRET__"

LENGTH = 32
CHUNK = 12
ALPHABET = string.ascii_letters + string.digits
MASK = 0x5A


def encode(secret: str, mask: int = MASK, chunk: int = CHUNK) -> list[str]:
    parts = []
    for start in range(0, len(secret), chunk):
        piece = secret[start:start + chunk]
        octets = []
        for char in piece:
            value = ord(char) ^ mask
            assert value != 0, "байт секрета кодируется в нуль, строка оборвётся"
            octets.append("\\%03o" % value)
        parts.append('"%s"' % "".join(octets))
    return parts


def decode(parts: list[str], mask: int = MASK) -> str:
    out = []
    for part in parts:
        for octet in re.findall(r"\\([0-7]{3})", part):
            out.append(chr(int(octet, 8) ^ mask))
    return "".join(out)


def read_secret(text: str, prefix: str) -> str | None:
    parts = []
    index = 0
    while True:
        found = re.search(r'#define\s+%s_%d\s+"((?:\\[0-7]{3})*)"' % (prefix, index), text)
        if not found:
            break
        parts.append(found.group(1))
        index += 1
    return decode(parts) if parts else None


def current_secrets() -> tuple[str | None, str | None, str]:
    """Текущие значения: generated-заголовок, иначе fallback из network.h."""
    if GENERATED.is_file():
        text = GENERATED.read_text(encoding="utf-8")
        return read_secret(text, "NET_WRITE_SECRET"), read_secret(text, "NET_PASS_SECRET"), "secrets_generated.h"
    text = HEADER.read_text(encoding="utf-8")
    return read_secret(text, "NET_WRITE_SECRET"), read_secret(text, "NET_PASS_SECRET"), "network.h (тестовый fallback)"


def new_secret() -> str:
    while True:
        secret = "".join(secrets.choice(ALPHABET) for _ in range(LENGTH))
        if all(ord(char) ^ MASK != 0 for char in secret):
            return secret


def update_legacy_chunks(old_pass: str) -> None:
    """Предыдущий хвост становится ступенькой миграции в write_secret.inc."""
    text = WRITE_SECRET_INC.read_text(encoding="utf-8")
    chunks = encode(old_pass)
    lines = ",\n".join("                                   %s" % c for c in chunks)
    pattern = re.compile(
        r'static const char \*chunks\[\] = \{".*?"\};', re.DOTALL)
    if not pattern.search(text):
        raise SystemExit("В write_secret.inc не нашёл куски fb_legacy_pass_suffix()")
    text = pattern.sub("static const char *chunks[] = {%s};" % lines, text, count=1)
    WRITE_SECRET_INC.write_text(text, encoding="utf-8")


def render_rules(write_secret: str) -> int:
    text = RULES.read_text(encoding="utf-8")
    hits = text.count(PLACEHOLDER)
    if hits < 6:
        raise SystemExit("В firebase.rules.json меньше 6 плейсхолдеров — шаблон правился вручную?")
    rendered = text.replace(PLACEHOLDER, write_secret)
    json.loads(rendered)
    RENDERED.write_text(rendered, encoding="utf-8")
    return hits


def gh_secret_set(name: str, value: str) -> bool:
    try:
        result = subprocess.run(
            ["gh", "secret", "set", name, "--body", value],
            cwd=ROOT, capture_output=True, text=True, timeout=30)
    except (OSError, subprocess.TimeoutExpired):
        return False
    return result.returncode == 0


def append_deny(values: list[str]) -> None:
    existing = []
    if DENY_FILE.is_file():
        existing = DENY_FILE.read_text(encoding="utf-8").splitlines()
    merged = list(dict.fromkeys([line for line in existing if line.strip()] + values))
    DENY_FILE.write_text("\n".join(merged) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description="Ротация секретов базы Firebase")
    parser.add_argument("--show", action="store_true", help="показать текущие значения из generated-заголовка")
    parser.add_argument("--secret-write", help="задать свой секрет записи (32 символа [A-Za-z0-9])")
    parser.add_argument("--secret-pass", help="задать свой хвост пароля (32 символа [A-Za-z0-9])")
    parser.add_argument("--old-write-secret", help="уходящий секрет записи (если его нет в generated-заголовке)")
    parser.add_argument("--old-pass-secret", help="уходящий хвост пароля (если его нет в generated-заголовке)")
    parser.add_argument("--yes", action="store_true", help="не спрашивать подтверждения")
    args = parser.parse_args()

    cur_write, cur_pass, source = current_secrets()
    if args.show:
        print(f"источник: {source}")
        print("секрет записи (поле w):  %s" % (cur_write or "<нет>"))
        print("хвост пароля аккаунта:   %s" % (cur_pass or "<нет>"))
        return 0

    old_write = args.old_write_secret or cur_write
    old_pass = args.old_pass_secret or cur_pass
    if not old_write or not old_pass or old_write == TEST_WRITE_SECRET or old_pass == TEST_PASS_SECRET:
        raise SystemExit(
            "Не знаю уходящие боевые значения: generated-заголовка нет или в нём тестовые. "
            "Передай --old-write-secret и --old-pass-secret явно.")
    for name, value in (("секрет записи", args.secret_write), ("хвост пароля", args.secret_pass)):
        if value is not None and (len(value) < 24 or not value.isascii() or not value.isalnum()):
            raise SystemExit(f"{name}: нужна строка из букв и цифр, не короче 24 символов")

    new_write = args.secret_write or new_secret()
    new_pass = args.secret_pass or new_secret()
    if new_write == new_pass:
        raise SystemExit("Секреты должны быть разными: секрет записи лежит в базе открыто")
    if new_write == old_write or new_pass == old_pass:
        raise SystemExit("Новые значения совпали со старыми — это не ротация")

    if not args.yes:
        answer = input("Сменить оба секрета базы? Старые аккаунты переедут по ступеньке. [y/N] ").strip().lower()
        if answer not in ("y", "yes", "д", "да"):
            print("Отменено.")
            return 1

    update_legacy_chunks(old_pass)
    hits = render_rules(new_write)
    deny_values = [old_write, old_pass]
    append_deny(deny_values)

    deny_text = "\n".join(deny_values)
    LOCAL_FILE.write_text(
        "СЕКРЕТЫ CUBIC BATTLE 4 — копия владельца. В git не коммитить!\n"
        "\n"
        "GitHub Actions secrets (Settings → Secrets and variables → Actions):\n"
        "CB4_WRITE_SECRET=" + new_write + "\n"
        "CB4_PASS_SECRET=" + new_pass + "\n"
        "CB4_SECRET_DENY (одно значение, внутри — переводы строк):\n" + deny_text + "\n"
        "\n"
        "Новые пароли админов генерирует владелец при смене в консоли Firebase:\n"
        "пароль для консоли = <новый пароль из игры><хвост CB4_PASS_SECRET>.\n",
        encoding="utf-8")

    env = dict(os.environ, CB4_WRITE_SECRET=new_write, CB4_PASS_SECRET=new_pass)
    subprocess.run([sys.executable, "tools/gen_secrets.py"], cwd=ROOT, env=env, check=True)
    subprocess.run([sys.executable, "tools/render_rules.py"], cwd=ROOT, env=env, check=True)

    pushed = all([
        gh_secret_set("CB4_WRITE_SECRET", new_write),
        gh_secret_set("CB4_PASS_SECRET", new_pass),
        gh_secret_set("CB4_SECRET_DENY", "\n".join(deny_values)),
    ])

    print("\nГотово: секреты перевыпущены, ступенька миграции обновлена,")
    print("firebase.rules.rendered.json собран (%d подстановок)." % hits)
    if pushed:
        print("Секреты положены в GitHub Actions самим скриптом.")
    else:
        print("В GitHub секреты положить не вышло (нет прав у токена): вставь три")
        print("значения из SECRETS.local.txt в Settings → Secrets and variables → Actions.")
    print("\nОсталось сделать владельцу:")
    print("  1. Опубликовать firebase.rules.rendered.json в консоли Firebase")
    print("     (Realtime Database → Rules → Publish) — до раздачи новой сборки.")
    print("  2. Сменить пароли админских аккаунтов в Firebase Console →")
    print("     Authentication; старый пароль с новым хвостом — в SECRETS.local.txt")
    print("     их нет: пароль придумывает владелец, игра допишет хвост сама.")
    print("  3. Пересобрать игру: CI подставит новые секреты из GitHub Secrets.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
