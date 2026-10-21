#!/usr/bin/env python3
"""Меняет оба секрета базы: хвост пароля аккаунта и «пароль на запись».

Запуск без аргументов:
    python3 tools/rotate_db_secret.py

Что делает:
  1. Придумывает два новых секрета по 32 случайных символа [A-Za-z0-9]
     (генератор случайных чисел криптографический, secrets модуль).
  2. Переписывает куски NET_PASS_SECRET_* и NET_WRITE_SECRET_* в
     src/engine/network.h — в клиенте секреты хранятся не строкой, а кусками,
     каждый байт xor 0x5a, чтобы строку не находил grep по собранному apk.
  3. Меняет старый секрет записи на новый в firebase.rules.json.

После запуска обязательно:
  * открыть firebase.rules.json в консоли Firebase и опубликовать правила
    (Realtime Database → Rules → Publish), иначе база будет отвергать записи
    нового клиента;
  * собрать и выпустить новую версию игры.

Важно про хвост пароля. Пароль аккаунта = «пароль игрока + хвост», поэтому
смена хвоста меняет пароли всех аккаунтов. Клиент умеет пускать старые
аккаунты по предыдущему хвосту (FB_LEGACY_PASS_SUFFIX в write_secret.inc) и
сразу переписывает пароль на новый. Лестница знает только ОДИН старый хвост —
тот, что был раньше ("|cb4v1"). Если покрутить секрет ещё раз, аккаунты,
созданные в промежутке, не войдут: перед вторым запуском либо дайте игрокам
переехать, либо допишите ещё одну ступеньку в fb_sign_in().
"""

from __future__ import annotations

import argparse
import json
import re
import secrets
import string
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src" / "engine" / "network.h"
RULES = ROOT / "firebase.rules.json"

LENGTH = 32
CHUNK = 12
ALPHABET = string.ascii_letters + string.digits
MASK = 0x5A


def encode(secret: str, mask: int = MASK, chunk: int = CHUNK) -> list[str]:
    """Режет секрет на куски и кодирует каждый байт в восьмеричный escape."""
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


def read_secret(header: str, prefix: str) -> tuple[str, list[str]]:
    parts = []
    index = 0
    while True:
        found = re.search(r'#define\s+%s_%d\s+"((?:\\[0-7]{3})*)"' % (prefix, index), header)
        if not found:
            break
        parts.append(found.group(1))
        index += 1
    if not parts:
        raise SystemExit(f"В {HEADER.relative_to(ROOT)} нет макросов {prefix}_*")
    return decode(parts), parts


def write_secret(header: str, prefix: str, secret: str) -> str:
    chunks = encode(secret)
    for index, chunk in enumerate(chunks):
        pattern = re.compile(r'#define\s+%s_%d\s+"(?:\\?[0-7]{3})*"' % (prefix, index))
        if not pattern.search(header):
            raise SystemExit(f"В network.h нет макроса {prefix}_{index}")
        # Замену вставляем через лямбду: в кусках есть escape-последовательности,
        # и re.sub иначе съест обратные слеши.
        header = pattern.sub(lambda _m, line="#define %s_%d %s" % (prefix, index, chunk): line,
                             header, count=1)
    # Удаляем хвостовые куски, оставшиеся от более длинного секрета.
    index = len(chunks)
    while True:
        pattern = re.compile(r'#define\s+%s_%d\s+"(?:\\?[0-7]{3})*"\n' % (prefix, index))
        if not pattern.search(header):
            break
        header = pattern.sub("", header, count=1)
        index += 1
    header = re.sub(
        r"#define\s+%s_LEN\s+\d+" % prefix, "#define %s_LEN %d" % (prefix, len(secret)), header, count=1)
    return header


def new_secret() -> str:
    while True:
        secret = "".join(secrets.choice(ALPHABET) for _ in range(LENGTH))
        if all(ord(char) ^ MASK != 0 for char in secret):
            return secret


def main() -> int:
    parser = argparse.ArgumentParser(description="Смена секретов базы Firebase")
    parser.add_argument("--show", action="store_true", help="только показать текущие секреты")
    parser.add_argument("--secret-write", help="задать свой секрет записи (32 символа [A-Za-z0-9])")
    parser.add_argument("--secret-pass", help="задать свой хвост пароля (32 символа [A-Za-z0-9])")
    parser.add_argument("--yes", action="store_true", help="не спрашивать подтверждения")
    args = parser.parse_args()

    header = HEADER.read_text(encoding="utf-8")
    rules_text = RULES.read_text(encoding="utf-8")
    old_write = read_secret(header, "NET_WRITE_SECRET")[0]
    old_pass = read_secret(header, "NET_PASS_SECRET")[0]

    if args.show:
        print("секрет записи (поле w):  %s" % old_write)
        print("хвост пароля аккаунта:   %s" % old_pass)
        print("вхождений секрета записи в firebase.rules.json: %d" % rules_text.count(old_write))
        return 0

    for name, value in (("секрет записи", args.secret_write), ("хвост пароля", args.secret_pass)):
        if value is None:
            continue
        if len(value) < 24 or not value.isascii() or not value.isalnum():
            raise SystemExit(f"{name}: нужна строка из букв и цифр, не короче 24 символов")

    new_write = args.secret_write or new_secret()
    new_pass = args.secret_pass or new_secret()
    if new_write == new_pass:
        raise SystemExit("Секреты должны быть разными: секрет записи лежит в базе открыто")
    hits = rules_text.count(old_write)
    if hits < 6:
        raise SystemExit("В firebase.rules.json меньше 6 вхождений старого секрета записи — файл правился вручную?")

    print("Старый секрет записи: %s" % old_write)
    print("Новый секрет записи:  %s" % new_write)
    print("Новый хвост пароля:   %s" % new_pass)
    print()
    print("Будет изменено: src/engine/network.h и firebase.rules.json (%d вхождений)." % hits)
    if not args.yes:
        answer = input("Продолжить? [y/N] ").strip().lower()
        if answer not in ("y", "yes", "д", "да"):
            print("Отменено.")
            return 1

    header = write_secret(header, "NET_WRITE_SECRET", new_write)
    header = write_secret(header, "NET_PASS_SECRET", new_pass)
    HEADER.write_text(header, encoding="utf-8")
    RULES.write_text(rules_text.replace(old_write, new_write), encoding="utf-8")
    json.loads(RULES.read_text(encoding="utf-8"))

    print("\nГотово. Что сделать дальше:")
    print("  1. Firebase console → Realtime Database → Rules: заменить правила")
    print("     содержимым firebase.rules.json и нажать Publish.")
    print("  2. Проверить тесты:  CC=gcc python3 tools/test_resources.py")
    print("  3. Собрать и выпустить новую версию игры.")
    print("  4. Старые аккаунты зайдут по ступеньке FB_LEGACY_PASS_SUFFIX и сами")
    print("     переедут на новый хвост при первом входе.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
