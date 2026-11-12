#!/usr/bin/env python3
"""PetWings shared high-score API (Python standard library only).

  GET  /api/ranking?limit=8   -> {"entries": [{"name", "potions", "episode", "level", "cleared", "date"}]}
  POST /api/ranking           <- {"name", "potions", "episode", "level", "cleared"}
                              -> {"rank": n, "entries": [... top 8 ...]}
  GET  /api/health            -> {"ok": true}

Environment:
  PETWINGS_DB      SQLite file (default /data/ranking.sqlite3)
  PETWINGS_PORT    listen port (default 8000)
  PETWINGS_STATIC  optional directory to also serve static files from (local testing only)
"""
import json
import mimetypes
import os
import re
import sqlite3
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

DB_PATH = os.environ.get("PETWINGS_DB", "/data/ranking.sqlite3")
PORT = int(os.environ.get("PETWINGS_PORT", "8000"))
STATIC = os.environ.get("PETWINGS_STATIC")
NAME_RE = re.compile(r"^[A-Z0-9 .\-]{0,15}$")
KEEP_ENTRIES = 1000
RATE_WINDOW, RATE_MAX = 600, 8      # submissions per client IP per 10 minutes
MAX_BODY = 512

_lock = threading.Lock()
_recent = {}  # ip -> [timestamps]


def connect():
    conn = sqlite3.connect(DB_PATH, timeout=10)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    os.makedirs(os.path.dirname(DB_PATH) or ".", exist_ok=True)
    with connect() as c:
        c.execute("""CREATE TABLE IF NOT EXISTS scores (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL, potions INTEGER NOT NULL,
            episode INTEGER NOT NULL, level INTEGER NOT NULL, cleared INTEGER NOT NULL,
            created REAL NOT NULL, ip TEXT NOT NULL)""")
        if c.execute("SELECT COUNT(*) FROM scores").fetchone()[0] == 0:
            # the original game's default table
            now = time.time()
            for i in range(8):
                c.execute("INSERT INTO scores (name, potions, episode, level, cleared, created, ip) "
                          "VALUES ('SAITO GAMES', ?, 0, 0, 0, ?, '')", (100 - i * 10, now + i * 1e-3))


def top(c, limit):
    rows = c.execute("SELECT id, name, potions, episode, level, cleared, created FROM scores "
                     "ORDER BY potions DESC, created ASC LIMIT ?", (limit,)).fetchall()
    return [{"name": r["name"], "potions": r["potions"], "episode": r["episode"], "level": r["level"],
             "cleared": bool(r["cleared"]), "date": time.strftime("%Y-%m-%d", time.gmtime(r["created"]))}
            for r in rows]


def client_ip(handler):
    h = handler.headers
    ip = h.get("CF-Connecting-IP") or (h.get("X-Forwarded-For") or "").split(",")[0].strip() \
        or h.get("X-Real-IP") or handler.client_address[0]
    return ip[:64]


def rate_limited(ip):
    now = time.time()
    with _lock:
        hits = [t for t in _recent.get(ip, []) if now - t < RATE_WINDOW]
        if len(hits) >= RATE_MAX:
            _recent[ip] = hits
            return True
        hits.append(now)
        _recent[ip] = hits
        return False


def validate(d):
    try:
        name = str(d.get("name", "")).upper().strip()
        potions, episode, level = int(d["potions"]), int(d["episode"]), int(d["level"])
        cleared = bool(d.get("cleared", False))
    except (KeyError, TypeError, ValueError):
        return None, "bad fields"
    if not NAME_RE.match(name):
        return None, "bad name"
    if not (0 <= potions <= 9999) or not (1 <= episode <= 4):
        return None, "out of range"
    if not (1 <= level <= (5 if episode == 4 else 16)):
        return None, "out of range"
    return (name, potions, episode, level, int(cleared)), None


class Handler(BaseHTTPRequestHandler):
    server_version = "petwings-api"

    def log_message(self, fmt, *args):
        print("%s %s" % (client_ip(self), fmt % args), flush=True)

    def send_json(self, code, obj):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        url = urlparse(self.path)
        if url.path == "/api/ranking":
            try:
                limit = max(1, min(100, int(parse_qs(url.query).get("limit", ["8"])[0])))
            except ValueError:
                limit = 8
            with connect() as c:
                return self.send_json(200, {"entries": top(c, limit)})
        if url.path == "/api/health":
            return self.send_json(200, {"ok": True})
        if STATIC and not url.path.startswith("/api/"):
            return self.serve_static(url.path)
        self.send_json(404, {"error": "not found"})

    def do_POST(self):
        if urlparse(self.path).path != "/api/ranking":
            return self.send_json(404, {"error": "not found"})
        length = int(self.headers.get("Content-Length") or 0)
        if length <= 0 or length > MAX_BODY:
            return self.send_json(413, {"error": "bad size"})
        try:
            data = json.loads(self.rfile.read(length))
        except ValueError:
            return self.send_json(400, {"error": "bad json"})
        entry, err = validate(data if isinstance(data, dict) else {})
        if err:
            return self.send_json(400, {"error": err})
        ip = client_ip(self)
        if rate_limited(ip):
            return self.send_json(429, {"error": "too many submissions"})
        with _lock, connect() as c:
            now = time.time()
            cur = c.execute("INSERT INTO scores (name, potions, episode, level, cleared, created, ip) "
                            "VALUES (?, ?, ?, ?, ?, ?, ?)", entry + (now, ip))
            rank = c.execute("SELECT COUNT(*) FROM scores WHERE potions > ? OR (potions = ? AND created < ?)",
                             (entry[1], entry[1], now)).fetchone()[0] + 1
            c.execute("DELETE FROM scores WHERE id NOT IN (SELECT id FROM scores "
                      "ORDER BY potions DESC, created ASC LIMIT ?)", (KEEP_ENTRIES,))
            entries = top(c, 8)
        print("score %r %d ep%d-%d -> rank %d (id %d)" % (entry[0], entry[1], entry[2], entry[3], rank, cur.lastrowid), flush=True)
        self.send_json(200, {"rank": rank, "entries": entries})

    def serve_static(self, path):
        rel = os.path.normpath(path.lstrip("/") or "index.html")
        full = os.path.join(STATIC, rel)
        if rel.startswith("..") or not os.path.isfile(full):
            return self.send_json(404, {"error": "not found"})
        data = open(full, "rb").read()
        self.send_response(200)
        self.send_header("Content-Type", mimetypes.guess_type(full)[0] or "application/octet-stream")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


if __name__ == "__main__":
    mimetypes.add_type("application/wasm", ".wasm")
    init_db()
    print("petwings-api on :%d, db %s" % (PORT, DB_PATH), flush=True)
    ThreadingHTTPServer(("0.0.0.0", PORT), Handler).serve_forever()
