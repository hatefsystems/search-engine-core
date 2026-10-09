import json
import sys
from pathlib import Path
import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from app.catalog import connect, initialize, Catalog
from app.ingest import atomic_json, digest
from app.svg import sanitize
from app.text import enrich, expand


@pytest.fixture
def catalog(tmp_path):
    root = tmp_path / "data"
    root.mkdir()
    path = root / "test.sqlite"
    db = connect(path)
    initialize(db)
    names = [
        "cloud",
        "cloud-cog",
        "workflow",
        "scale",
        "stethoscope",
        "graduation-cap",
        "building",
        "palette",
        "sprout",
        "shopping-cart",
        "wrench",
        "briefcase",
    ]
    for i, name in enumerate(names):
        svg, hash_ = sanitize(
            f'<svg viewBox="0 0 24 24"><path d="M0 0 L{i+1} 20"/></svg>'
        )
        cats, fa = enrich(name, [])
        id_ = "lucide:" + name
        metadata = {
            "labelEn": name.replace("-", " "),
            "labelFa": fa,
            "categories": cats,
            "keywords": [],
            "brand": False,
            "raw": {},
        }
        search = expand(name.replace("-", " ") + " " + fa)
        db.execute("INSERT INTO assets VALUES(?,?)", (hash_, svg))
        db.execute(
            "INSERT INTO icons VALUES(?,?,?,?,?,?,?,?,?)",
            (
                id_,
                "lucide",
                name,
                cats[0] if cats else "",
                "outline",
                0,
                hash_,
                json.dumps(metadata),
                search,
            ),
        )
        db.execute("INSERT INTO search VALUES(?,?)", (id_, search))
    db.execute(
        "INSERT INTO sources VALUES(?,?)",
        (
            "lucide",
            json.dumps({"license": "ISC", "licenseText": "ISC License fixture"}),
        ),
    )
    db.commit()
    db.close()
    atomic_json(
        root / "current.json",
        {"version": "fixture-v1", "catalog": path.name, "sha256": digest(path)},
    )
    return Catalog(root)
