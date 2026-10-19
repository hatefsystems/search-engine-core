"""Tiny synthetic container fixture, not production data."""

import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from app.catalog import connect, initialize
from app.ingest import atomic_json, digest
from app.svg import sanitize
from app.text import expand

root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
path = root / "smoke.sqlite"
db = connect(path)
initialize(db)
for i, name in enumerate(["cloud", "briefcase"]):
    svg, hash_ = sanitize(f'<svg viewBox="0 0 24 24"><path d="M0 0h{i+1}"/></svg>')
    id_ = "lucide:" + name
    data = {
        "labelEn": name,
        "labelFa": "ابر" if name == "cloud" else "خدمات",
        "keywords": [name],
        "categories": ["cloud-infrastructure"] if name == "cloud" else ["general"],
        "brand": False,
        "raw": {},
    }
    search = expand(name)
    db.execute("INSERT INTO assets VALUES(?,?)", (hash_, svg))
    db.execute(
        "INSERT INTO icons VALUES(?,?,?,?,?,?,?,?,?)",
        (
            id_,
            "lucide",
            name,
            data["categories"][0],
            "outline",
            0,
            hash_,
            json.dumps(data),
            search,
        ),
    )
    db.execute("INSERT INTO search VALUES(?,?)", (id_, search))
db.commit()
db.close()
atomic_json(
    root / "current.json",
    {"version": "smoke-test", "catalog": path.name, "sha256": digest(path)},
)
