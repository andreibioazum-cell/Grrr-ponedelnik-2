#!/usr/bin/env python3
"""Сторож: в репозитории не должно быть паролей и приватных ключей.

Зачем: репозиторий публичный. Однажды пароль админа (ADMIN_PASS) лежал прямо
в игровых константах, и любой читатель мог войти в админский аккаунт. Этот
скрипт ловит возврат такой практики на этапе CI.

Проверки (по файлам под контролем версий, кроме исключений):
  * объявления/присваивания паролей вида ADMIN_PASS = "...";
  * блоки приватных ключей (-----BEGIN ... PRIVATE KEY-----);
  * файлы, которые не должны попадать в репозиторий (.env, *.pem, *.p12,
    google-services.json, локальные файлы секретов);
  * строки из необязательного списка SECRET_DENY (по одной на строку,
    файл .secrets-deny, в git не входит) - сюда вносятся значения, которые
    уже утекли и не должны появиться снова.

Запуск: python3 tools/check_secrets.py
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DENY_FILE = ROOT / ".secrets-deny"

# Файлы, которые обязаны существовать и не содержать секретов.
TEXT_SUFFIXES = {".c", ".h", ".inc", ".cpp", ".py", ".js", ".mjs", ".json", ".md", ".yml", ".yaml", ".java", ".xml", ".txt", ".html", ""}

PASSWORD_ASSIGN = re.compile(
    r"""(?ix)
    \b(?:admin[0-9_]*_?pass(?:word)?|db_?password|firebase_?secret|api_?secret)\b
    \s*(?:=|:)\s*["'][^"']+["']
    """
)
PRIVATE_KEY = re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----")
FORBIDDEN_NAMES = {
    ".env", ".env.local", "google-services.json", "service-account.json",
    "secrets.local.json", "secrets.h",
}
FORBIDDEN_SUFFIXES = {".pem", ".p12", ".pfx", ".jks"}


def tracked_files() -> list[Path]:
    out = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, capture_output=True, text=True, check=True)
    return [ROOT / name for name in out.stdout.split("\0") if name]


def deny_list() -> list[str]:
    if not DENY_FILE.is_file():
        return []
    return [line.strip() for line in DENY_FILE.read_text().splitlines() if line.strip() and not line.startswith("#")]


def main() -> int:
    problems: list[str] = []
    deny = deny_list()
    for path in tracked_files():
        name = path.name
        if name in FORBIDDEN_NAMES or path.suffix.lower() in FORBIDDEN_SUFFIXES:
            problems.append(f"{path.relative_to(ROOT)}: файл с секретами не должен быть в репозитории")
            continue
        if path.suffix.lower() not in TEXT_SUFFIXES:
            continue
        try:
            text = path.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        for index, line in enumerate(text.splitlines(), 1):
            if PASSWORD_ASSIGN.search(line) and "example" not in line.lower():
                problems.append(f"{path.relative_to(ROOT)}:{index}: пароль в исходнике")
            if PRIVATE_KEY.search(line):
                problems.append(f"{path.relative_to(ROOT)}:{index}: приватный ключ")
            for value in deny:
                if value in line:
                    problems.append(f"{path.relative_to(ROOT)}:{index}: утёкшее значение из .secrets-deny")

    if problems:
        print("Найдены секреты в репозитории:")
        for item in problems:
            print("  " + item)
        return 1
    print("Секретов в репозитории нет: пароли не хранятся в коде, ключей и .env нет.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
