import json
import sqlite3
import uuid
from contextlib import contextmanager
from pathlib import Path


class Store:
    def __init__(self, path):
        self.path = path
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        with self.connect() as db:
            db.execute("PRAGMA journal_mode=WAL")
            db.executescript("""
                CREATE TABLE IF NOT EXISTS sessions (
                    id TEXT PRIMARY KEY, system TEXT NOT NULL,
                    summary TEXT NOT NULL DEFAULT '', through INTEGER NOT NULL DEFAULT 0
                );
                CREATE TABLE IF NOT EXISTS turns (
                    seq INTEGER PRIMARY KEY AUTOINCREMENT,
                    session TEXT NOT NULL REFERENCES sessions(id) ON DELETE CASCADE,
                    request TEXT NOT NULL, user TEXT NOT NULL, assistant TEXT NOT NULL,
                    result TEXT NOT NULL, UNIQUE(session, request)
                );
                CREATE INDEX IF NOT EXISTS turns_session ON turns(session, seq);
                CREATE TABLE IF NOT EXISTS notes (
                    session TEXT PRIMARY KEY REFERENCES sessions(id) ON DELETE CASCADE,
                    summary TEXT NOT NULL, through INTEGER NOT NULL
                );
            """)

    @contextmanager
    def connect(self):
        db = sqlite3.connect(self.path, timeout=10)
        db.row_factory = sqlite3.Row
        db.execute("PRAGMA foreign_keys=ON")
        try:
            with db:
                yield db
        finally:
            db.close()

    def create(self, system):
        sid = str(uuid.uuid4())
        with self.connect() as db:
            db.execute("INSERT INTO sessions(id, system) VALUES (?,?)", (sid, system))
        return sid

    def load(self, sid):
        with self.connect() as db:
            session = db.execute("SELECT * FROM sessions WHERE id=?", (sid,)).fetchone()
            if session is None:
                return None
            turns = db.execute("SELECT seq,user,assistant FROM turns WHERE session=? AND seq>? ORDER BY seq",
                               (sid, session["through"])).fetchall()
            return dict(session), [dict(t) for t in turns]

    def session(self, sid):
        with self.connect() as db:
            row = db.execute("SELECT * FROM sessions WHERE id=?", (sid,)).fetchone()
            return dict(row) if row else None

    def note(self, sid):
        with self.connect() as db:
            row = db.execute("SELECT summary,through FROM notes WHERE session=?", (sid,)).fetchone()
            return dict(row) if row else {"summary": "", "through": 0}

    def save_note(self, sid, summary, through):
        with self.connect() as db:
            db.execute("INSERT INTO notes(session,summary,through) VALUES (?,?,?) ON CONFLICT(session) DO UPDATE SET summary=excluded.summary,through=excluded.through", (sid, summary, through))

    def checkpoint(self):
        with self.connect() as db:
            db.execute("PRAGMA wal_checkpoint(TRUNCATE)")

    def cached(self, sid, request):
        with self.connect() as db:
            row = db.execute("SELECT user,result FROM turns WHERE session=? AND request=?", (sid, request)).fetchone()
            return (row["user"], json.loads(row["result"])) if row else None

    def commit(self, sid, request, user, answer, summary, through, result):
        with self.connect() as db:
            db.execute("INSERT INTO turns(session,request,user,assistant,result) VALUES (?,?,?,?,?)",
                       (sid, request, user, answer, json.dumps(result)))
            db.execute("UPDATE sessions SET summary=?,through=? WHERE id=?", (summary, through, sid))

    def history(self, sid, after, limit):
        with self.connect() as db:
            return [dict(r) for r in db.execute(
                "SELECT seq,user,assistant FROM turns WHERE session=? AND seq>? ORDER BY seq LIMIT ?",
                (sid, after, limit))]

    def delete(self, sid):
        with self.connect() as db:
            return db.execute("DELETE FROM sessions WHERE id=?", (sid,)).rowcount > 0
