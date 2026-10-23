#!/usr/bin/env python3
"""Serves build/web/ on 0.0.0.0:8080 for interactive 60 FPS WebGL2 + Firebase preview."""
from __future__ import annotations

import http.server
import os
import socketserver
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "build" / "web"
REPO = Path(__file__).resolve().parent.parent
PORT = int(sys.argv[1]) if len(sys.argv) > 1 else 8080


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(ROOT), **kwargs)

    def do_GET(self):
        if self.path in ("/", "/index.html") or self.path.startswith("/?"):
            self.path = "/play.html"
        return super().do_GET()

    def end_headers(self):
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()


class ThreadingHTTPServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


if __name__ == "__main__":
    if not (ROOT / "cb4.wasm").is_file():
        import subprocess
        subprocess.run([sys.executable, str(REPO / "tools" / "build_web.py")], cwd=REPO, check=True)
    os.makedirs(ROOT, exist_ok=True)
    with ThreadingHTTPServer(("0.0.0.0", PORT), Handler) as httpd:
        print(f"Cubic Battle 4 — 3D Minecraft Live Preview listening on http://0.0.0.0:{PORT}", flush=True)
        httpd.serve_forever()
