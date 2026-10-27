#!/usr/bin/env python3
"""Exercise direct-message access through the local Fake Firebase HTTP API."""
from __future__ import annotations

import importlib.util
import json
import os
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from http.server import ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("fake_firebase", ROOT / "test/fake_firebase.py")
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("could not load test/fake_firebase.py")
FAKE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(FAKE)


def request(base: str, method: str, path: str, body=None, token: str | None = None):
    query = {"auth": token} if token else {}
    suffix = urllib.parse.urlencode(query)
    url = base + path + ("&" if "?" in path and suffix else "?" if suffix else "") + suffix
    data = None if body is None else json.dumps(body, ensure_ascii=False).encode("utf-8")
    req = urllib.request.Request(url, data=data, method=method,
                                 headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=5) as response:
            raw = response.read()
            return response.status, json.loads(raw) if raw else None
    except urllib.error.HTTPError as exc:
        raw = exc.read()
        try:
            payload = json.loads(raw) if raw else None
        except json.JSONDecodeError:
            payload = raw.decode("utf-8", "replace")
        return exc.code, payload


def sign_up(base: str, nick: str) -> str:
    status, result = request(base, "POST", "/v1/accounts:signUp?key=test", {
        "email": nick.lower() + "@cb4.game", "password": "test-password",
    })
    assert status == 200, (nick, status, result)
    FAKE.DB["users"][nick] = FAKE.profile(nick)
    return result["idToken"]


def main() -> int:
    os.environ["FAKE_FB_QUIET"] = "1"
    server = ThreadingHTTPServer(("127.0.0.1", 0), FAKE.Handler)
    server.daemon_threads = True
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    base = "http://127.0.0.1:%d" % server.server_address[1]
    try:
        alice = sign_up(base, "DmTestA")
        bob = sign_up(base, "DmTestB")
        outsider = sign_up(base, "DmTestC")
        now = int(time.time() * 1000)
        first = {"from": "DmTestA", "to": "DmTestB", "text": "Привет", "ts": now,
                 "w": FAKE.SECRET}
        sender_path = "/direct/DmTestA/DmTestB/msg-a.json"
        mirror_path = "/direct/DmTestB/DmTestA/msg-a.json"

        assert request(base, "GET", "/direct/DmTestA.json?shallow=true")[0] == 403
        assert request(base, "PUT", sender_path, first, outsider)[0] == 403
        assert request(base, "PUT", sender_path, first, alice)[0] == 200
        assert request(base, "PUT", mirror_path, first, alice)[0] == 200

        status, inbox_a = request(base, "GET", "/direct/DmTestA.json?shallow=true", token=alice)
        assert status == 200 and "DmTestB" in inbox_a, (status, inbox_a)
        status, inbox_b = request(base, "GET", "/direct/DmTestB.json?shallow=true", token=bob)
        assert status == 200 and "DmTestA" in inbox_b, (status, inbox_b)
        assert request(base, "GET", "/direct/DmTestA.json", token=bob)[0] == 403
        assert request(base, "GET", "/direct/DmTestA/DmTestB.json", token=outsider)[0] == 403

        # A valid reply is mirrored the other way round, using the same participant rule.
        reply = {"from": "DmTestB", "to": "DmTestA", "text": "Здравствуйте", "ts": now + 1,
                 "w": FAKE.SECRET}
        assert request(base, "PUT", "/direct/DmTestB/DmTestA/msg-b.json", reply, bob)[0] == 200
        assert request(base, "PUT", "/direct/DmTestA/DmTestB/msg-b.json", reply, bob)[0] == 200

        spoof = dict(first, **{"from": "DmTestC"})
        assert request(base, "PUT", "/direct/DmTestA/DmTestB/spoof.json", spoof, outsider)[0] == 403
        mismatch = dict(first, **{"from": "DmTestB", "to": "DmTestA"})
        assert request(base, "PUT", "/direct/DmTestA/DmTestB/mismatch.json", mismatch, alice)[0] == 403
        assert request(base, "PUT", "/direct/DmTestA/DmTestB/no-seal.json",
                       dict(first, **{"w": "wrong"}), alice)[0] == 403
        assert request(base, "PUT", "/direct/DmTestA/DmTestB/too-long.json",
                       dict(first, **{"text": "x" * 321}), alice)[0] == 403
        self_message = dict(first, **{"to": "DmTestA"})
        assert request(base, "PUT", "/direct/DmTestA/DmTestA/self.json", self_message, alice)[0] == 403
        assert request(base, "PUT", "/direct/DmTestA/UnknownUser/no-user.json", first, alice)[0] == 403
        assert request(base, "PUT", sender_path, first, alice)[0] == 403  # immutable message
        assert request(base, "DELETE", sender_path, token=alice)[0] == 403
        print("direct messages: authenticated owner inboxes, mirrored participant writes, "
              "spoofing, tampering, oversize messages and deletion checks OK")
        return 0
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=5)


if __name__ == "__main__":
    sys.exit(main())
