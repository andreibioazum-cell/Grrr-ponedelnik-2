#!/usr/bin/env python3
"""Ephemeral rendezvous for Ponedelnik P2P; it never accepts chat messages."""
from __future__ import annotations

import argparse
import base64
import collections
import hashlib
import hmac
import json
import secrets
import ssl
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlsplit

MAX_BODY = 16 * 1024
MAX_SIGNAL = 12 * 1024
SIGNAL_TTL = 120.0
POLL_SECONDS = 18.0
HELLO_FIELDS = {"version", "from", "to", "nickname", "identity", "ephemeral", "nonce", "candidates", "signature"}


def valid_hello(hello: object, source: str, target: str) -> bool:
    if not isinstance(hello, dict) or set(hello) != HELLO_FIELDS:
        return False
    nickname = hello.get("nickname")
    if type(hello.get("version")) is not int or hello.get("version") != 1 \
            or hello.get("from") != source or hello.get("to") != target:
        return False
    if not isinstance(nickname, str) or not nickname.strip() or len(nickname) > 32:
        return False
    if any(ord(char) < 32 or 127 <= ord(char) <= 159 for char in nickname):
        return False
    try:
        identity = base64.b64decode(hello["identity"], validate=True)
        ephemeral = base64.b64decode(hello["ephemeral"], validate=True)
        nonce = base64.b64decode(hello["nonce"], validate=True)
        signature = base64.b64decode(hello["signature"], validate=True)
    except (TypeError, ValueError):
        return False
    if not (32 <= len(identity) <= 256 and 32 <= len(ephemeral) <= 256
            and len(nonce) == 16 and 48 <= len(signature) <= 128):
        return False
    candidates = hello.get("candidates")
    if not isinstance(candidates, list) or not 1 <= len(candidates) <= 8:
        return False
    for candidate in candidates:
        if not isinstance(candidate, dict) or set(candidate) != {"host", "port"}:
            return False
        host, port = candidate.get("host"), candidate.get("port")
        if not isinstance(host, str) or len(host) > 15 or not all(part.isdigit() for part in host.split(".")):
            return False
        octets = host.split(".")
        if len(octets) != 4 or any(not 0 <= int(part) <= 255 or len(part) > 3 for part in octets):
            return False
        if type(port) is not int or not 1 <= port <= 65535:
            return False
    return True


class Peer:
    def __init__(self, token: str) -> None:
        self.token_hash = hashlib.sha256(token.encode("ascii")).digest()
        self.queue: collections.deque[tuple[float, dict]] = collections.deque()
        self.sent_at: collections.deque[float] = collections.deque()


class Rendezvous:
    """In-memory peer registry. Restarting this process deletes all IDs/signals."""

    def __init__(self) -> None:
        self.condition = threading.Condition()
        self.peers: dict[str, Peer] = {}
        self.ip_registrations: dict[str, collections.deque[float]] = {}

    def register(self, address: str) -> tuple[str, str]:
        now = time.monotonic()
        with self.condition:
            times = self.ip_registrations.setdefault(address, collections.deque())
            while times and now - times[0] > 60:
                times.popleft()
            if len(times) >= 8:
                raise ValueError("rate limited")
            times.append(now)
            peer_id = secrets.token_hex(16).upper()
            token = secrets.token_urlsafe(32)
            self.peers[peer_id] = Peer(token)
            return peer_id, token

    def authenticate(self, peer_id: str, token: str) -> Peer | None:
        peer = self.peers.get(peer_id)
        if not peer or len(token) > 128:
            return None
        candidate = hashlib.sha256(token.encode("utf-8")).digest()
        return peer if hmac.compare_digest(peer.token_hash, candidate) else None

    def send(self, source: str, token: str, target: str, kind: str, payload: dict) -> None:
        now = time.monotonic()
        with self.condition:
            sender = self.authenticate(source, token)
            recipient = self.peers.get(target)
            if sender is None:
                raise PermissionError("invalid token")
            if recipient is None:
                raise LookupError("peer not online")
            while sender.sent_at and now - sender.sent_at[0] > 60:
                sender.sent_at.popleft()
            if len(sender.sent_at) >= 30:
                raise OverflowError("rate limited")
            sender.sent_at.append(now)
            self._expire(recipient, now)
            if len(recipient.queue) >= 24:
                raise OverflowError("recipient signal queue full")
            item = {"from": source, "kind": kind, "payload": payload}
            recipient.queue.append((now + SIGNAL_TTL, item))
            self.condition.notify_all()

    @staticmethod
    def _expire(peer: Peer, now: float) -> None:
        while peer.queue and peer.queue[0][0] <= now:
            peer.queue.popleft()

    def poll(self, peer_id: str, token: str) -> dict | None:
        until = time.monotonic() + POLL_SECONDS
        with self.condition:
            peer = self.authenticate(peer_id, token)
            if peer is None:
                raise PermissionError("invalid token")
            while True:
                now = time.monotonic()
                self._expire(peer, now)
                if peer.queue:
                    return peer.queue.popleft()[1]
                remaining = until - now
                if remaining <= 0:
                    return None
                self.condition.wait(min(remaining, 2.0))
                if self.authenticate(peer_id, token) is None:
                    raise PermissionError("expired peer")

class Handler(BaseHTTPRequestHandler):
    server: "SignalHTTPServer"
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt: str, *args: object) -> None:
        # Default access logs include peer IDs from the poll URL; keep them disabled.
        return

    def _reply(self, status: int, value: dict) -> None:
        body = json.dumps(value, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def _body(self) -> dict:
        raw_len = self.headers.get("Content-Length", "0")
        try:
            size = int(raw_len)
        except ValueError as exc:
            raise ValueError("invalid content length") from exc
        if size < 0 or size > MAX_BODY:
            raise OverflowError("request body too large")
        raw = self.rfile.read(size)
        value = json.loads(raw.decode("utf-8"))
        if not isinstance(value, dict):
            raise ValueError("JSON object required")
        return value

    def _token(self) -> str:
        value = self.headers.get("Authorization", "")
        if not value.startswith("Bearer "):
            return ""
        return value[7:]

    def do_GET(self) -> None:
        parsed = urlsplit(self.path)
        if parsed.path == "/health":
            self._reply(200, {"service": "ponedelnik-rendezvous", "chat_messages_stored": False,
                              "signal_ttl_seconds": int(SIGNAL_TTL)})
            return
        if parsed.path != "/v1/poll":
            self._reply(404, {"error": "not found"})
            return
        query = parse_qs(parsed.query)
        peer_id = query.get("id", [""])[0].upper()
        try:
            signal = self.server.registry.poll(peer_id, self._token())
        except PermissionError:
            self._reply(401, {"error": "unauthorized"})
            return
        self._reply(200, {"signal": signal})

    def do_POST(self) -> None:
        try:
            body = self._body()
        except OverflowError as exc:
            self._reply(413, {"error": str(exc)})
            return
        except (ValueError, UnicodeDecodeError, json.JSONDecodeError) as exc:
            self._reply(400, {"error": str(exc)})
            return

        if self.path == "/v1/register":
            try:
                peer_id, token = self.server.registry.register(self.client_address[0])
            except ValueError:
                self._reply(429, {"error": "rate limited"})
                return
            self._reply(201, {"peer_id": peer_id, "token": token})
            return

        if self.path == "/v1/signal":
            source = str(body.get("from", "")).upper()
            target = str(body.get("to", "")).upper()
            kind = body.get("kind")
            payload = body.get("payload")
            if not (len(source) == 32 and len(target) == 32
                    and all(c in "0123456789ABCDEF" for c in source + target)) or source == target:
                self._reply(400, {"error": "invalid peer id"})
                return
            if kind not in ("offer", "answer") or not isinstance(payload, dict) or set(payload) != {"hello"}:
                self._reply(400, {"error": "invalid signal"})
                return
            if not valid_hello(payload.get("hello"), source, target):
                self._reply(400, {"error": "invalid signed hello"})
                return
            if len(json.dumps(payload, separators=(",", ":")).encode("utf-8")) > MAX_SIGNAL:
                self._reply(413, {"error": "signal too large"})
                return
            try:
                self.server.registry.send(source, self._token(), target, kind, payload)
            except PermissionError:
                self._reply(401, {"error": "unauthorized"})
                return
            except LookupError:
                self._reply(404, {"error": "peer not online"})
                return
            except OverflowError as exc:
                self._reply(429, {"error": str(exc)})
                return
            self._reply(202, {"queued": True, "expires_in": int(SIGNAL_TTL)})
            return

        self._reply(404, {"error": "not found"})


class SignalHTTPServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True

    def __init__(self, address: tuple[str, int]) -> None:
        super().__init__(address, Handler)
        self.registry = Rendezvous()


def make_server(host: str = "127.0.0.1", port: int = 8765) -> SignalHTTPServer:
    return SignalHTTPServer((host, port))


def main() -> int:
    parser = argparse.ArgumentParser(description="P2P rendezvous only; chat is never relayed or stored.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8765)
    parser.add_argument("--cert", help="TLS certificate (for HTTPS deployments)")
    parser.add_argument("--key", help="TLS private key (for HTTPS deployments)")
    parser.add_argument("--dev-http", action="store_true", help="allow cleartext on a private test network only")
    args = parser.parse_args()
    if bool(args.cert) != bool(args.key):
        parser.error("--cert and --key must be supplied together")
    local = args.host in ("127.0.0.1", "localhost", "::1")
    if not local and not (args.cert and args.key) and not args.dev_http:
        parser.error("non-local bind requires TLS; --dev-http is only for an isolated test LAN")
    server = make_server(args.host, args.port)
    scheme = "http"
    if args.cert and args.key:
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.minimum_version = ssl.TLSVersion.TLSv1_2
        context.load_cert_chain(args.cert, args.key)
        server.socket = context.wrap_socket(server.socket, server_side=True)
        scheme = "https"
    print(f"Rendezvous listening on {scheme}://{args.host}:{args.port}")
    print("No chat-message, account, or contact database exists in this process.")
    try:
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt:
        print("Stopping rendezvous")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
