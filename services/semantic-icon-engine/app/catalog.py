import json
import hashlib
import sqlite3
from contextlib import contextmanager
from pathlib import Path
from .text import expand


def connect(path, readonly=False):
    db = sqlite3.connect(
        f"file:{Path(path).resolve()}?mode=ro" if readonly else str(path),
        uri=readonly,
        timeout=5,
    )
    db.row_factory = sqlite3.Row
    db.execute("PRAGMA cache_size=-8192")
    if readonly:
        db.execute("PRAGMA query_only=ON")
    return db


def initialize(db):
    db.executescript(
        """
    CREATE TABLE IF NOT EXISTS assets(hash TEXT PRIMARY KEY,svg TEXT NOT NULL);
    CREATE TABLE IF NOT EXISTS icons(id TEXT PRIMARY KEY,source TEXT,name TEXT,category TEXT,style TEXT,brand INTEGER,hash TEXT,metadata TEXT,search_text TEXT);
    CREATE INDEX IF NOT EXISTS icons_category ON icons(category,id);
    CREATE INDEX IF NOT EXISTS icons_source ON icons(source,id);
    CREATE INDEX IF NOT EXISTS icons_hash ON icons(hash);
    CREATE VIRTUAL TABLE IF NOT EXISTS search USING fts5(id UNINDEXED,text,tokenize='unicode61');
    CREATE TABLE IF NOT EXISTS decisions(id TEXT PRIMARY KEY,status TEXT,reason TEXT,source TEXT);
    CREATE TABLE IF NOT EXISTS sources(id TEXT PRIMARY KEY,metadata TEXT);
    CREATE TABLE IF NOT EXISTS state(key TEXT PRIMARY KEY,value TEXT);
    """
    )


def metadata(row):
    data = json.loads(row["metadata"])
    data.update(
        id=row["id"],
        source=row["source"],
        name=row["name"],
        hash=row["hash"],
        category=row["category"],
        style=row["style"],
    )
    data["iconUrl"] = "/assets/icons/" + row["id"].replace(":", "/") + ".svg"
    return data


class Catalog:
    def __init__(self, root):
        self.root = Path(root)
        self.manifest = json.loads((self.root / "current.json").read_text())
        name = self.manifest["catalog"]
        if Path(name).name != name:
            raise ValueError("unsafe catalog manifest")
        self.path = self.root / name
        with self.path.open("rb") as asset:
            if (
                hashlib.file_digest(asset, "sha256").hexdigest()
                != self.manifest["sha256"]
            ):
                raise ValueError("catalog integrity mismatch")
        with self.db() as db:
            self.count = db.execute("SELECT count(*) FROM icons").fetchone()[0]

    @contextmanager
    def db(self):
        db = connect(self.path, True)
        try:
            yield db
        finally:
            db.close()

    def get(self, icon_id):
        with self.db() as db:
            row = db.execute("SELECT * FROM icons WHERE id=?", (icon_id,)).fetchone()
            return metadata(row) if row else None

    def search(self, text="", category="", source="", style="", limit=20, offset=0):
        args, where, join, order = [], [], "", "i.id"
        tokens = list(dict.fromkeys(expand(text).split()))[:48]
        if tokens:
            join = " JOIN search s ON s.id=i.id"
            where.append("search MATCH ?")
            args.append(" OR ".join('"' + t + '"' for t in tokens))
            order = "bm25(search),i.id"
        for key, val in [("category", category), ("source", source), ("style", style)]:
            if val:
                where.append("i." + key + "=?")
                args.append(val)
        sql = (
            "SELECT i.* FROM icons i"
            + join
            + (" WHERE " + " AND ".join(where) if where else "")
            + " ORDER BY "
            + order
            + " LIMIT ? OFFSET ?"
        )
        with self.db() as db:
            return [metadata(r) for r in db.execute(sql, (*args, limit, offset))]
