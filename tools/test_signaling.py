#!/usr/bin/env python3
"""Exercise the local rendezvous API and its no-chat-storage boundary."""
from __future__ import annotations

import base64
import json
import threading
import urllib.error
import urllib.request
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from server.signaling import make_server


def request(url: str, method: str = "GET", body: dict | None = None, token: str | None = None):
    data = None if body is None else json.dumps(body).encode()
    headers = {"Accept": "application/json"}
    if data is not None:
        headers["Content-Type"] = "application/json"
    if token:
        headers["Authorization"] = "Bearer " + token
    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=3) as response:
            return response.status, json.loads(response.read())
    except urllib.error.HTTPError as error:
        return error.code, json.loads(error.read())


def run() -> None:
    server = make_server("127.0.0.1", 0)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    root = f"http://127.0.0.1:{server.server_port}"
    try:
        status, health = request(root + "/health")
        assert status == 200 and health["chat_messages_stored"] is False
        _, left = request(root + "/v1/register", "POST", {})
        _, right = request(root + "/v1/register", "POST", {})
        sender, receiver = left["peer_id"], right["peer_id"]
        assert len(sender) == len(receiver) == 32
        assert all(char in "0123456789ABCDEF" for char in sender + receiver)
        hello = {
            "version": 1,
            "from": sender,
            "to": receiver,
            "nickname": "Alice",
            "identity": base64.b64encode(b"i" * 65).decode(),
            "ephemeral": base64.b64encode(b"e" * 65).decode(),
            "nonce": base64.b64encode(b"n" * 16).decode(),
            "candidates": [{"host": "192.168.1.20", "port": 41000}],
            "signature": base64.b64encode(b"s" * 64).decode(),
        }
        status, sent = request(root + "/v1/signal", "POST",
                               {"from": sender, "to": receiver, "kind": "offer", "payload": {"hello": hello}},
                               left["token"])
        assert status == 202 and sent["queued"] is True
        status, received = request(root + "/v1/poll?id=" + receiver, token=right["token"])
        assert status == 200 and received["signal"]["from"] == sender
        assert received["signal"]["payload"]["hello"] == hello
        status, _ = request(root + "/v1/signal", "POST",
                            {"from": sender, "to": receiver, "kind": "offer",
                             "payload": {"hello": hello, "message": "plain text"}}, left["token"])
        assert status == 400, "rendezvous must reject extra message fields"
        invalid_hello = {**hello, "nickname": "bad\nname"}
        status, _ = request(root + "/v1/signal", "POST",
                            {"from": sender, "to": receiver, "kind": "offer",
                             "payload": {"hello": invalid_hello}}, left["token"])
        assert status == 400, "signed display names must not contain control characters"
        status, _ = request(root + "/v1/signal", "POST",
                            {"from": sender[:12], "to": receiver, "kind": "offer",
                             "payload": {"hello": hello}}, left["token"])
        assert status == 400, "rendezvous must enforce the 128-bit Peer ID length"
        status, _ = request(root + "/v1/signal", "POST",
                            {"from": sender, "to": receiver, "kind": "offer",
                             "payload": {"hello": hello}}, "not-the-token")
        assert status == 401
        status, _ = request(root + "/v1/messages", "POST", {"text": "not supported"})
        assert status == 404, "there must be no chat-message endpoint"
        status, _ = request(root + "/v1/unregister", "POST", {}, left["token"])
        assert status == 404, "the signaling API should only relay setup messages"
        print("signaling: ok (ephemeral signed handshakes only; no chat API)")
    finally:
        server.shutdown()
        server.server_close()
        thread.join(timeout=2)


if __name__ == "__main__":
    run()
