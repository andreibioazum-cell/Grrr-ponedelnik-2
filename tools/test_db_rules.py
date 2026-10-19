#!/usr/bin/env python3
"""Проверки firebase.rules.json, которые уже один раз стоили ивента.

Клиент пишет event = 0..4 (0 выкл, 1 диско, 2 плиты, 3 Дед Мороз, 4 кубик).
Старое правило пускало только 0..3: запись кубика отклонялась, поллер
возвращал предыдущий ивент, и вместо кубика включались плиты. Здесь же
проверяется, что подарки перечислены по одному стикеру и чужие ключи закрыты.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RULES = ROOT / "firebase.rules.json"


def main() -> int:
    rules = json.loads(RULES.read_text(encoding="utf-8"))["rules"]
    event = rules["event"][".validate"]
    if "<= 4" not in event:
        print(f"event rule rejects the cube event: {event}", file=sys.stderr)
        return 1
    gifts = rules["users"]["$nick"]["gifts"]
    for key in ("poop", "cake", "bear", "rocket", "emerald"):
        if key not in gifts:
            print(f"gift rule is missing: {key}", file=sys.stderr)
            return 1
        for rule in (".write", ".validate"):
            if rule not in gifts[key]:
                print(f"gift {key} has no {rule} rule", file=sys.stderr)
                return 1
    if gifts["$other"][".validate"] is not False:
        print("unknown gift keys must be rejected", file=sys.stderr)
        return 1
    print("db rules: event 0..4, gift keys poop/cake/bear/rocket/emerald OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
