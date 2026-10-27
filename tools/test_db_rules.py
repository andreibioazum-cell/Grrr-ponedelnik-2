#!/usr/bin/env python3
"""Checks of firebase.rules.json that already cost an event once.

The client writes event = 0..4 (0 off, 1 disco, 2 plates, 3 Santa, 4 cube). The old
rule allowed only 0..3: the cube write was rejected, the poller returned the
previous event, and plates turned on instead of the cube. The same file checks
that gifts are listed one sticker at a time and foreign keys stay closed.
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
    # The emerald's hue is chosen by an admin and stored next to the counters:
    # the rule has to allow 0..7 and to let only the admin write it.
    hue = gifts.get("emerald_hue", {})
    if "<= 7" not in hue.get(".validate", ""):
        print(f"emerald_hue rule is missing or too narrow: {hue}", file=sys.stderr)
        return 1
    if "dimasi4ek229@cb4.game" not in hue.get(".write", ""):
        print(f"emerald_hue must be written by an admin only: {hue}", file=sys.stderr)
        return 1
    profile = rules["users"]["$nick"]
    # The CodeCracker keeps a level branch of its own, so the profile has to
    # accept its two fields - and it has to accept the packed skin flags
    # (azum_skin carries two bits, 0..3), or a save with a skin is rejected.
    for key in ("codecracker_level", "codecracker_levels"):
        rule = profile.get(key, {}).get(".validate")
        if not rule or "<= 3" not in rule:
            print(f"profile rule for {key} is missing or too narrow: {rule}", file=sys.stderr)
            return 1
    skin_rule = profile.get("azum_skin", {}).get(".validate", "")
    if "<= 3" not in skin_rule:
        print(f"azum_skin rule rejects the packed skin flags: {skin_rule}", file=sys.stderr)
        return 1
    direct = rules.get("direct", {})
    owner = direct.get("$owner", {})
    read_rule = owner.get(".read", "")
    message = owner.get("$peer", {}).get("$msg", {})
    write_rule = message.get(".write", "")
    validate_rule = message.get(".validate", "")
    required_fields = ("from", "to", "text", "ts", "w")
    if not all(token in read_rule for token in ("auth != null", "@cb4[.]game", "$owner.toLowerCase()")):
        print(f"direct-message reads are not scoped to the authenticated owner: {read_rule}", file=sys.stderr)
        return 1
    if not all(token in write_rule for token in ("!data.exists()", "$owner", "$peer", "from", "to", "w", "root.child('users')")):
        print(f"direct-message writes must be participant-only and immutable: {write_rule}", file=sys.stderr)
        return 1
    if not all(field in validate_rule for field in required_fields) or "<= 320" not in message.get("text", {}).get(".validate", ""):
        print(f"direct-message schema or 320-character limit is missing: {message}", file=sys.stderr)
        return 1
    if message.get("$other", {}).get(".validate") is not False:
        print("unknown direct-message fields must be rejected", file=sys.stderr)
        return 1
    print("db rules: event 0..4, gifts, participant-only direct messages, "
          "320-byte message limit, codecracker levels and packed skins OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
