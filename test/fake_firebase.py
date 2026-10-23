#!/usr/bin/env python3
"""Local open-rules Firebase Realtime Database HTTP emulator for Geometrium E2E tests.
Supports GET, PUT, PATCH, POST, DELETE, X-HTTP-Method-Override: PATCH, and {".sv":"timestamp"}."""

import json
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse


def expand_sv(val):
    if isinstance(val, dict):
        if val.get(".sv") == "timestamp":
            return int(time.time() * 1000)
        return {k: expand_sv(v) for k, v in val.items()}
    if isinstance(val, list):
        return [expand_sv(v) for v in val]
    return val


class FakeFirebaseDB:
    def __init__(self, initial=None):
        self.lock = threading.Lock()
        self.root = initial if initial is not None else {}

    def _parts(self, path):
        if path.endswith(".json"):
            path = path[:-5]
        return [p for p in path.strip("/").split("/") if p]

    def get(self, path):
        with self.lock:
            cur = self.root
            for p in self._parts(path):
                if not isinstance(cur, dict) or p not in cur:
                    return None
                cur = cur[p]
            return json.loads(json.dumps(cur))

    def put(self, path, val):
        val = expand_sv(val)
        parts = self._parts(path)
        with self.lock:
            if not parts:
                self.root = val if val is not None else {}
                return val
            cur = self.root
            for p in parts[:-1]:
                if p not in cur or not isinstance(cur[p], dict):
                    cur[p] = {}
                cur = cur[p]
            if val is None:
                cur.pop(parts[-1], None)
            else:
                cur[parts[-1]] = val
            return val

    def patch(self, path, val):
        if not isinstance(val, dict):
            return self.put(path, val)
        val = expand_sv(val)
        parts = self._parts(path)
        with self.lock:
            cur = self.root
            for p in parts:
                if p not in cur or not isinstance(cur[p], dict):
                    cur[p] = {}
                cur = cur[p]
            for k, v in val.items():
                if v is None:
                    cur.pop(k, None)
                else:
                    cur[k] = v
            return json.loads(json.dumps(cur))


def make_handler(db):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, fmt, *args):
            pass

        def _send_json(self, code, obj):
            raw = json.dumps(obj, ensure_ascii=False).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, PUT, PATCH, POST, DELETE, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type, X-HTTP-Method-Override")
            self.send_header("Content-Length", str(len(raw)))
            self.end_headers()
            self.wfile.write(raw)

        def _read_json(self):
            ln = int(self.headers.get("Content-Length", "0") or "0")
            if ln <= 0:
                return None
            return json.loads(self.rfile.read(ln).decode("utf-8"))

        def do_OPTIONS(self):
            self._send_json(200, {})

        def do_GET(self):
            self._send_json(200, db.get(urlparse(self.path).path))

        def do_PUT(self):
            self._send_json(200, db.put(urlparse(self.path).path, self._read_json()))

        def do_PATCH(self):
            self._send_json(200, db.patch(urlparse(self.path).path, self._read_json()))

        def do_DELETE(self):
            self._send_json(200, db.put(urlparse(self.path).path, None))

        def do_POST(self):
            override = (self.headers.get("X-HTTP-Method-Override") or "").upper()
            if override == "PATCH":
                self._send_json(200, db.patch(urlparse(self.path).path, self._read_json()))
                return
            key = f"k_{int(time.time() * 1000000)}"
            db.put(urlparse(self.path).path.rstrip(".json") + "/" + key + ".json", self._read_json())
            self._send_json(200, {"name": key})

    return Handler


class RunningFakeFirebase:
    def __init__(self, initial=None):
        self.db = FakeFirebaseDB(initial)
        self.httpd = ThreadingHTTPServer(("127.0.0.1", 0), make_handler(self.db))
        self.port = self.httpd.server_address[1]
        self.url = f"http://127.0.0.1:{self.port}"
        self.thread = threading.Thread(target=self.httpd.serve_forever, daemon=True)

    def __enter__(self):
        self.thread.start()
        return self

    def __exit__(self, *exc):
        self.httpd.shutdown()
        self.httpd.server_close()


if __name__ == "__main__":
    import sys
    srv = RunningFakeFirebase()
    srv.thread.start()
    print(srv.url, flush=True)
    try:
        while True:
            time.sleep(1.0)
    except KeyboardInterrupt:
        sys.exit(0)
