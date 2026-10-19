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
def knowledge_fixture(tmp_path, monkeypatch):
    """Exercise the real ESCO importer with a small pinned archive."""
    import csv
    import io
    import shutil
    import zipfile
    from app import knowledge

    config = tmp_path / "knowledge-config"
    shutil.copytree(knowledge.CONFIG, config)
    monkeypatch.setattr(knowledge, "CONFIG", config)
    archive = tmp_path / "esco-fixture.zip"
    skill = "http://data.europa.eu/esco/skill/cloud"
    occupation = "http://data.europa.eu/esco/occupation/cloud-engineer"
    group = "http://data.europa.eu/esco/skillgroup/cloud"
    isco = "http://data.europa.eu/esco/isco/C1"
    tables = {
        "skills_en.csv": [
            dict(
                conceptUri=skill,
                preferredLabel="cloud infrastructure",
                status="released",
                altLabels="cloud\ncloud systems",
                description="manage infrastructure",
            ),
            dict(
                conceptUri=skill,
                preferredLabel="cloud infrastructure",
                status="released",
                altLabels="cloud computing",
                description="manage infrastructure",
            ),
        ],
        "occupations_en.csv": [
            dict(
                conceptUri=occupation,
                preferredLabel="cloud engineer",
                status="released",
            )
        ],
        "ISCOGroups_en.csv": [
            dict(conceptUri=isco, preferredLabel="engineers", status="released")
        ],
        "skillGroups_en.csv": [
            dict(conceptUri=group, preferredLabel="cloud skills", status="released")
        ],
        "occupationSkillRelations_en.csv": [
            dict(occupationUri=occupation, skillUri=skill, relationType="essential")
        ],
        "skillSkillRelations_en.csv": [],
        "broaderRelationsSkillPillar_en.csv": [
            dict(conceptUri=skill, broaderUri=group)
        ],
        "broaderRelationsOccPillar_en.csv": [
            dict(conceptUri=occupation, broaderUri=isco)
        ],
    }
    with zipfile.ZipFile(archive, "w") as z:
        for name, rows in tables.items():
            stream = io.StringIO()
            writer = csv.DictWriter(
                stream,
                fieldnames=sorted({key for row in rows for key in row}) or ["unused"],
            )
            writer.writeheader()
            writer.writerows(rows)
            z.writestr(name, stream.getvalue())
    lock = json.loads((config / "knowledge.lock.json").read_text())
    lock["esco"].update(
        sha256=digest(archive),
        expectedOccupations=1,
        expectedSkills=1,
        expectedISCOGroups=1,
        expectedSkillGroups=1,
    )
    atomic_json(config / "knowledge.lock.json", lock)
    atomic_json(
        config / "knowledge-fa.json",
        {
            "version": 1,
            "concepts": [
                {
                    "id": "hatef:concept:cloud",
                    "labelEn": "cloud",
                    "labelFa": "زیرساخت ابری",
                    "aliasesFa": ["رایانش ابری", "ابر"],
                    "aliasesEn": ["cloud infrastructure"],
                    "icons": ["cloud"],
                }
            ],
            "escoOverrides": [],
        },
    )

    def make(root, unused_archive, catalog):
        return knowledge.build_bank(root, archive, catalog)

    make.archive, make.config, make.skill = archive, config, skill
    return make


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
