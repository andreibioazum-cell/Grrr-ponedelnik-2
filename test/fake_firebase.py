#!/usr/bin/env python3
"""Подставной Firebase для веб-панели (index.html) и сетевого кода игры.

Зачем он нужен. Веб-панель админа — это обычный HTML+JS, который ходит в
Firebase REST. Без подставной базы её нельзя ни проверить, ни просто открыть и
понажимать: боевой проект закрыт правилами, а ключ — ограничениями по referrer.
Поэтому панель умеет «тестовый режим» (кнопка в настройках проекта): адрес базы
= адрес этой страницы, вход любой парой «ник + пароль», аккаунт создаётся сам.

Сервер раздаёт и саму панель, и базу — одним портом, чтобы не было проблем с
CORS и смешанным контентом:

    python3 test/fake_firebase.py 8080
    → http://localhost:8080/          (index.html)
    → http://localhost:8080/.json     (корень базы)

Что реализовано из REST Realtime Database:
  GET    <path>.json            (+ X-Firebase-ETag -> ETag; пустой путь = корень)
  PUT    <path>.json            (if-match: "null" или etag -> 200/412)
  PATCH  <path>.json            (глубокое слияние плоских ключей "a/b": value)
  DELETE <path>.json
  POST   <path>.json            (push-ключ -> {"name": "-KEY"})
  GET    .../chat.json?orderBy="$key"&limitToLast=32

Из Firebase Auth:
  POST   /v1/accounts:signUp?key=K
  POST   /v1/accounts:signInWithPassword?key=K
  POST   /v1/token?key=K        (refresh_token -> id_token)
  GET    /__stats.json          (счётчики для тестов)

Правила доступа повторяют firebase.rules.json настолько, чтобы панель можно
было проверить в обоих режимах:
  * корень и «чужие» узлы читает/пишет только админ;
  * event, banner, bans, AzumBuff, FlashSale, rooms/*/meta пишет только админ;
  * users/<ник> — сам игрок или админ;
  * слоты и чат — любой вошедший игрок;
  * AzumBuff и FlashSale читаются всеми (в правилах ".read": true);
  * без токена — 401, под токеном, но без прав — 403 (именно на эти коды
    панель реагирует переходом на «снимок по узлам» и удалением по детям).
"""
import base64
import hashlib
import json
import os
import re
import sys
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PANEL = os.path.join(ROOT, "index.html")

ADMINS = ("dimasi4ek229@cb4.game", "qwertyuiopaj1234@cb4.game")
SECRET = "BMAhzk26iwVJAK3KpPxoFCzkt08FL0Hc"  # из firebase.rules.json (поле "w")
PUBLIC_ROOT = ("AzumBuff", "FlashSale")      # ".read": true в правилах
GAME_ROOT = ("users", "rooms", "bans", "event", "banner")
KNOWN_ROOT = GAME_ROOT + PUBLIC_ROOT
ADMIN_ONLY_WRITE = ("event", "banner", "bans", "AzumBuff", "FlashSale")

LOCK = threading.Lock()
AUTH = {}       # email -> password
REFRESH = {}    # refresh_token -> email
TOKENS = {}     # id_token -> email
COUNTER = [0]

STATS = {"sign_up": 0, "sign_in": 0, "token_refresh": 0,
         "rtdb_auth": 0, "rtdb_no_auth": 0, "rtdb_denied": 0, "static": 0}


# ---------------------------------------------------------------------------
# Наполнение базы: живые данные ровно той формы, что пускают правила,
# плюс мусор, на котором видно работу уборщика панели.
# ---------------------------------------------------------------------------
def profile(nick, **over):
    p = {
        "nick": nick, "w": SECRET,
        "cups": 120, "candies": 7, "cls": 0,
        "azum": 1, "santa": 0, "ebuc": 1, "astra": 0,
        "level": 2, "levels": 3,
        "ordinary_level": 1, "ordinary_levels": 2,
        "azum_level": 2, "azum_levels": 3,
        "santa_level": 0, "santa_levels": 1,
        "ebuc_level": 1, "ebuc_levels": 2,
        "astra_level": 0, "astra_levels": 0,
        "codecracker_level": 1, "codecracker_levels": 2,
        "bp": 3, "bpx": 420, "azum_skin": 2,
        "ach_flags": 5, "ach_revives": 1,
        "lang": 0, "hitboxes": 1, "musicvol": 70, "winter": 0,
        "showfps": 0, "classic": 0, "playtime": 5400,
        "promo": "2027", "promo_used": 1,
        "pd_consent_ts": 1767225600, "pd_consent_ver": 2,
        "gifts": {"poop": 2, "cake": 1, "bear": 0, "rocket": 3,
                  "emerald": 1, "emerald_hue": 4, "rose": 0},
    }
    p.update(over)
    return p


def slot(nick, seq, **over):
    s = {
        "uid": "%016x" % (abs(hash(nick)) % (16 ** 16)),
        "nick": nick, "w": SECRET,
        "x": 0.4, "y": 0.6, "angle": 1.1, "hp": 100, "alive": 1, "seq": seq,
        "px": 0.4, "py": 0.6, "pdx": 1, "pdy": 0, "punch": 0,
        "sx": 0, "sy": 0, "sdx": 0, "sdy": 0, "snow": 0,
        "stx": 0, "sty": 0, "sthp": 0, "station": 0,
        "s2x": 0, "s2y": 0, "s2hp": 0, "s3x": 0, "s3y": 0, "s3hp": 0,
        "dax": 0, "day": 0, "dadx": 0, "dady": 0, "dash": 0,
        "gbx": 0, "gby": 0, "gbdx": 0, "gbdy": 0, "grab": 0,
        "uzx": 0, "uzy": 0, "universe": 0, "thud": 0, "blk": 0, "ccu": 0,
        "cls": 0, "level": 2, "skin": 1,
    }
    s.update(over)
    return s


def msg(nick, text, key):
    return {"uid": "%016x" % (abs(hash(nick + key)) % (16 ** 16)),
            "nick": nick, "text": text, "w": SECRET}


DB = {
    "users": {
        "Dimasi4ek229": profile("Dimasi4ek229", cups=4200, candies=31),
        "Player2": profile("Player2", cls=1, azum=1, cups=10),
        # Мусорные «игроки»: ник не проходит правила, пароль в базе,
        # неизвестное поле и накрутка сверх лимита правил.
        "xX_hack_Xx": {"nick": "xX_hack_Xx", "cups": 99999999, "pass": "123456",
                       "freeCups": True},
        "Spammer": {"nick": "Spammer", "cups": 5, "candies": 0, "cls": 0,
                    "azum": 0, "santa": 0, "level": 0, "levels": 0,
                    "gifts": {"poop": 1000}},
    },
    "rooms": {
        "main": {
            "players": {"0": slot("Dimasi4ek229", 18), "1": slot("Player2", 7),
                        # заявка без движения (нулевой слот-заглушка: игра
                        # пишет нули при захвате слота, room_sync.inc)
                        "2": slot("Player3", 0, hp=0, alive=0, x=0, y=0, angle=0)},
            "chat": {
                "-M0001": msg("Player2", "привет", "-M0001"),
                "-M0002": msg("Spammer", "взлом кубков пиши в тг @freecups", "-M0002"),
                "-M0003": {"nick": "Broken"},   # битое сообщение: нет uid и text
            },
        },
        "private1": {
            "meta": {"owner": "player2", "ph": "1234", "w": SECRET},
            "players": {},
            "chat": {"-N0001": msg("Player2", "комната с паролем", "-N0001")},
        },
        # комната с мусорным узлом и битой meta
        "junk": {"whatever": 1, "meta": {"owner": "x"}},
    },
    "bans": {"Spammer": True, "OldBan": False, "not a nick!": True},
    "event": 0,
    "banner": {"ts": int(time.time() * 1000), "text": "Новый год в кубиках!",
               "color": "white"},
    "AzumBuff": 0,
    "FlashSale": 0,
    # корневой мусор: узлы, которых нет в игре
    "freeCupsHack": {"cups": 999999},
    "testtest": "junk",
}

# Админские аккаунты создаются сразу: вход ником Dimasi4ek229 с паролем «admin»
# (панель сама добавит суффикс |cb4v1). Любой другой ник создаётся по signUp.
for _admin in ADMINS:
    AUTH[_admin] = "admin|cb4v1"


# ---------------------------------------------------------------------------
# Токены: настоящий на вид JWT, чтобы jwt() в панели разбирал payload.
# ---------------------------------------------------------------------------
def _b64(obj):
    raw = json.dumps(obj, separators=(",", ":")).encode()
    return base64.urlsafe_b64encode(raw).decode().rstrip("=")


def _issue_tokens(email):
    with LOCK:
        COUNTER[0] += 1
        n = COUNTER[0]
    payload = {"email": email, "iat": int(time.time()),
               "exp": int(time.time()) + 3600,
               "firebase": {"sign_in_provider": "password"}}
    id_token = "%s.%s.tok%d" % (_b64({"alg": "none", "typ": "JWT"}), _b64(payload), n)
    refresh = "refreshtok-%d" % n
    with LOCK:
        REFRESH[refresh] = email
        TOKENS[id_token] = email
    return id_token, refresh


def token_email(token):
    with LOCK:
        return TOKENS.get(token or "")


def node_etag(node):
    return '"etag-%s"' % hashlib.sha1(repr(node).encode()).hexdigest()[:16]


def deep_get(node, parts):
    for p in parts:
        if not isinstance(node, dict) or p not in node:
            return None
        node = node[p]
    return node


def deep_set(node, parts, value):
    if not parts:
        return False
    for p in parts[:-1]:
        nxt = node.get(p)
        if not isinstance(nxt, dict):
            nxt = {}
            node[p] = nxt
        node = nxt
    node[parts[-1]] = value
    return True


def deep_del(node, parts):
    if not parts:
        return False
    for p in parts[:-1]:
        if not isinstance(node, dict) or p not in node:
            return False
        node = node[p]
    if isinstance(node, dict) and parts[-1] in node:
        del node[parts[-1]]
        return True
    return False


# ---------------------------------------------------------------------------
# Права: урезанный firebase.rules.json
# ---------------------------------------------------------------------------
def can_read(parts, email):
    if not parts:                                   # корень
        return email in ADMINS
    head = parts[0]
    if head in PUBLIC_ROOT:
        return True
    if head in KNOWN_ROOT:
        return bool(email)                          # любой вошедший игрок
    return email in ADMINS                          # «чужие» узлы видит админ


def can_write(parts, email):
    if not email:
        return False
    if not parts:
        return email in ADMINS
    head = parts[0]
    if head in ADMIN_ONLY_WRITE:
        if head == "bans" and len(parts) > 1:
            # своих админов банить нельзя — как в правилах
            if parts[1].lower() in ("dimasi4ek229", "qwertyuiopaj1234"):
                return False
        return email in ADMINS
    if head == "rooms":
        if len(parts) > 2 and parts[2] == "meta":
            return email in ADMINS
        if len(parts) > 2 and parts[2] == "players" and len(parts) > 3:
            if not re.match(r"^[0-3]$", parts[3]):
                return False
        return True                                 # слоты и чат — любому игроку
    if head == "users":
        if len(parts) < 2:
            return email in ADMINS                  # список игроков целиком
        if email in ADMINS:
            return True
        return parts[1].lower() == email.split("@")[0].lower()
    return email in ADMINS


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "FakeFirebase/2"

    def log_message(self, fmt, *args):
        if os.environ.get("FAKE_FB_QUIET"):
            return
        sys.stderr.write("SVR %s\n" % (fmt % args))

    # --- служебное ---------------------------------------------------------
    def _read_body(self):
        n = int(self.headers.get("Content-Length") or 0)
        return self.rfile.read(n) if n > 0 else b""

    def _cors(self):
        # Панель открывается из браузера (в том числе через https-прокси
        # предпросмотра), поэтому разрешаем любой источник: база подставная.
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET,PUT,PATCH,DELETE,POST,OPTIONS")
        self.send_header("Access-Control-Allow-Headers",
                         "Content-Type,If-Match,X-Firebase-ETag,Authorization")
        self.send_header("Access-Control-Expose-Headers", "ETag")

    def _send(self, code, body=b"", ctype="application/json", etag=None):
        self.send_response(code)
        self._cors()
        if etag is not None:
            self.send_header("ETag", etag)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        if body:
            self.wfile.write(body)

    def do_OPTIONS(self):
        self._read_body()
        self._send(204, b"")

    # --- роутинг -----------------------------------------------------------
    def _handle(self):
        path, _, query = self.path.partition("?")
        if path == "/__stats.json":
            with LOCK:
                self._send(200, json.dumps(STATS, separators=(",", ":")).encode())
            return
        parts_probe = [p for p in path.split("/") if p]
        if parts_probe and parts_probe[0] == "v1":
            self._auth(parts_probe, query, self._read_body())
            return
        if path.endswith(".json"):
            self._rtdb(path[:-5], query)
            return
        self._static(path)

    do_GET = _handle
    do_PUT = _handle
    do_PATCH = _handle
    do_DELETE = _handle
    do_POST = _handle

    # --- раздача панели ----------------------------------------------------
    def _static(self, path):
        if path not in ("/", "/index.html", "/favicon.ico"):
            self._send(404, b'{"error":"not found"}')
            return
        if path == "/favicon.ico":
            self._send(204, b"")
            return
        try:
            with open(PANEL, "rb") as fh:
                body = fh.read()
        except OSError as exc:
            self._send(404, json.dumps({"error": "index.html: %s" % exc}).encode())
            return
        with LOCK:
            STATS["static"] += 1
        self._send(200, body, ctype="text/html; charset=utf-8")

    # --- Realtime Database -------------------------------------------------
    def _rtdb(self, path, query):
        parts = [p for p in path.split("/") if p]
        qs = parse_qs(query)
        token = (qs.get("auth", [""]) or [""])[0]
        email = token_email(token)
        method = self.command
        body = self._read_body()

        with LOCK:
            if token:
                STATS["rtdb_auth"] += 1
            else:
                STATS["rtdb_no_auth"] += 1

        if token and not email:
            self._send(401, b'{"error":"Auth token is invalid or malformed."}')
            return
        if not can_read(parts, email) and method in ("GET",):
            with LOCK:
                STATS["rtdb_denied"] += 1
            self._send(403, b'{"error":"Permission denied"}')
            return
        if method != "GET" and not can_write(parts, email):
            with LOCK:
                STATS["rtdb_denied"] += 1
            self._send(403, b'{"error":"Permission denied"}')
            return

        if method == "GET":
            node = deep_get(DB, parts) if parts else DB
            raw = b"null" if node is None else json.dumps(node, separators=(",", ":")).encode()
            self._send(200, raw, etag=node_etag(node))
        elif method == "DELETE":
            deep_del(DB, parts)
            self._send(200, b"null")
        elif method == "PUT":
            if_match = self.headers.get("If-Match")
            node = deep_get(DB, parts) if parts else DB
            if if_match is not None:
                want = if_match.strip()
                if want == "null":
                    if node is not None:
                        self._send(412, b'{"error":"condition failed"}')
                        return
                elif want != node_etag(node):
                    self._send(412, b'{"error":"condition failed"}')
                    return
            try:
                value = json.loads(body.decode()) if body else None
            except Exception:
                self._send(400, b'{"error":"bad json"}')
                return
            if not parts:
                if not isinstance(value, dict):
                    self._send(400, b'{"error":"root must be an object"}')
                    return
                DB.clear()
                DB.update(value)
            else:
                deep_set(DB, parts, value)
            raw = b"null" if value is None else json.dumps(value, separators=(",", ":")).encode()
            self._send(200, raw, etag=node_etag(value))
        elif method == "PATCH":
            try:
                patch = json.loads(body.decode()) if body else {}
            except Exception:
                self._send(400, b'{"error":"bad json"}')
                return
            if not isinstance(patch, dict):
                self._send(400, b'{"error":"bad patch"}')
                return
            for key, value in patch.items():
                deep_set(DB, parts + key.split("/"), value)
            node = deep_get(DB, parts) if parts else DB
            raw = b"null" if node is None else json.dumps(node, separators=(",", ":")).encode()
            self._send(200, raw, etag=node_etag(node))
        elif method == "POST":
            key = "-TESTKEY%06d" % int(time.time() * 1000 % 1000000)
            try:
                value = json.loads(body.decode()) if body else {}
            except Exception:
                self._send(400, b'{"error":"bad json"}')
                return
            node = deep_get(DB, parts) if parts else DB
            if not isinstance(node, dict):
                node = {}
                deep_set(DB, parts, node)
            node[key] = value
            self._send(200, json.dumps({"name": key}).encode())
        else:
            self._send(405, b'{"error":"method"}')

    # --- Firebase Auth -----------------------------------------------------
    def _fb_error(self, message, code=400):
        self._send(code, json.dumps({"error": {"code": code, "message": message}}).encode())

    def _auth(self, parts, query, body):
        endpoint = "/".join(parts)
        qs = parse_qs(query)
        if not qs.get("key", [""])[0]:
            self._fb_error("API_KEY_INVALID")
            return
        try:
            data = json.loads(body.decode()) if body else {}
        except Exception:
            data = {k: v[0] for k, v in parse_qs(body.decode()).items()} if body else {}
        if not isinstance(data, dict):
            data = {}

        if endpoint in ("v1/accounts:signUp", "v1/accounts:signInWithPassword"):
            email = str(data.get("email", "")).lower()
            password = data.get("password", "")
            if not email or not password:
                self._fb_error("MISSING_EMAIL_OR_PASSWORD")
                return
            if len(password) < 6:
                self._fb_error("WEAK_PASSWORD : Password should be at least 6 characters")
                return
            with LOCK:
                exists = email in AUTH
            if endpoint == "v1/accounts:signUp":
                if exists:
                    self._fb_error("EMAIL_EXISTS")
                    return
                with LOCK:
                    AUTH[email] = password
                    STATS["sign_up"] += 1
                idt, rt = _issue_tokens(email)
            else:
                if not exists:
                    self._fb_error("EMAIL_NOT_FOUND")
                    return
                with LOCK:
                    good = AUTH[email] == password
                    if good:
                        STATS["sign_in"] += 1
                if not good:
                    self._fb_error("INVALID_PASSWORD")
                    return
                idt, rt = _issue_tokens(email)
            self._send(200, json.dumps({
                "idToken": idt, "refreshToken": rt, "expiresIn": "3600",
                "localId": email.split("@")[0], "email": email,
            }).encode())
            return

        if endpoint == "v1/token":
            rt = data.get("refresh_token", "")
            with LOCK:
                email = REFRESH.get(rt)
                if email:
                    STATS["token_refresh"] += 1
            if not email:
                self._fb_error("INVALID_REFRESH_TOKEN")
                return
            idt, nrt = _issue_tokens(email)
            self._send(200, json.dumps({
                "id_token": idt, "refresh_token": nrt, "expires_in": "3600",
                "user_id": email.split("@")[0],
            }).encode())
            return

        self._fb_error("OPERATION_NOT_ALLOWED")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    port = int(args[0]) if args else 18765
    host = "0.0.0.0"          # предпросмотр Arena проксирует порт извне
    if "--local" in sys.argv:
        host = "127.0.0.1"
    srv = ThreadingHTTPServer((host, port), Handler)
    srv.daemon_threads = True
    print("fake firebase: panel + database on http://%s:%d/" % (host, port), flush=True)
    print("  admin login: Dimasi4ek229 / admin (any other nick signs up on its own)", flush=True)
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
