"""Offline occupation/skill bank with explicit provenance and mapping states."""

import fcntl
import csv
import functools
import hashlib
import io
import json
import os
from pathlib import Path
import sqlite3
import uuid
import zipfile

from .catalog import connect
from .ingest import atomic_json, digest
from .text import CONFIG, normalize


def concept_id(uri):
    return "hatef:concept:" + str(uuid.uuid5(uuid.NAMESPACE_URL, uri))


def read_esco(archive, spec):
    if digest(archive) != spec["sha256"]:
        raise ValueError("ESCO archive checksum mismatch")
    with zipfile.ZipFile(archive) as z:

        def rows(name):
            info = z.getinfo(name)
            if info.file_size > 100 * 1024**2:
                raise ValueError("ESCO CSV exceeds size limit")
            return list(csv.DictReader(io.StringIO(z.read(info).decode("utf-8-sig"))))

        records = {}
        for kind, filename, countkey in [
            ("skill", "skills_en.csv", "expectedSkills"),
            ("occupation", "occupations_en.csv", "expectedOccupations"),
            ("group", "ISCOGroups_en.csv", "expectedISCOGroups"),
            ("group", "skillGroups_en.csv", "expectedSkillGroups"),
        ]:
            unique = {}
            for row in rows(filename):
                uri = row["conceptUri"]
                if not uri.startswith("http://data.europa.eu/esco/") or (
                    kind != "group"
                    and not uri.startswith("http://data.europa.eu/esco/" + kind + "/")
                ):
                    raise ValueError("invalid ESCO concept URI")
                if row["status"] != "released":
                    continue
                aliases = set(row.get("altLabels", "").splitlines()) - {""}
                if uri in unique:
                    # The official CSV repeats some URIs. Merge aliases, not
                    # duplicate concepts.
                    old = unique[uri]
                    old["aliasesEn"] = sorted(set(old["aliasesEn"]) | aliases)
                    continue
                unique[uri] = {
                    "id": concept_id(uri),
                    "uri": uri,
                    "kind": kind,
                    "labelEn": row["preferredLabel"],
                    "labelFa": "",
                    "aliasesEn": sorted(aliases),
                    "aliasesFa": [],
                    "descriptionEn": row.get("description", ""),
                    "descriptionFa": "",
                    "translationStatus": "untranslated",
                    "source": "esco",
                    "sourceVersion": spec["version"],
                }
            if len(unique) != spec[countkey]:
                raise ValueError("ESCO concept count mismatch: " + kind)
            records.update(unique)
        links = []
        for row in rows("occupationSkillRelations_en.csv"):
            a, b = row["occupationUri"], row["skillUri"]
            if a in records and b in records:
                links.append((concept_id(a), row["relationType"], concept_id(b)))
        for row in rows("skillSkillRelations_en.csv"):
            a, b = row["originalSkillUri"], row["relatedSkillUri"]
            if a in records and b in records:
                links.append(
                    (concept_id(a), "skill-" + row["relationType"], concept_id(b))
                )
        for filename in [
            "broaderRelationsSkillPillar_en.csv",
            "broaderRelationsOccPillar_en.csv",
        ]:
            for row in rows(filename):
                a, b = row["conceptUri"], row["broaderUri"]
                if a in records and b in records:
                    links.append((concept_id(a), "broader", concept_id(b)))
        return list(records.values()), sorted(set(links))


def build_bank(output, archive, icon_catalog):
    root = Path(output)
    root.mkdir(parents=True, exist_ok=True)
    with (root / ".setup.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        return _build_bank(root, archive, icon_catalog)


def _build_bank(output, archive, icon_catalog):
    root = Path(output)
    root.mkdir(parents=True, exist_ok=True)
    lock = json.loads((CONFIG / "knowledge.lock.json").read_text())
    editorial = json.loads((CONFIG / "knowledge-fa.json").read_text())
    identity = hashlib.sha256(
        json.dumps(
            [
                lock,
                editorial,
                icon_catalog.manifest["version"],
                digest(Path(__file__)),
                json.loads((CONFIG / "sources.lock.json").read_text())["model"],
                1,
            ],
            sort_keys=True,
        ).encode()
    ).hexdigest()[:24]
    target = root / f"knowledge-{identity}.sqlite"
    manifest = root / "knowledge.json"
    if target.exists():
        payload = json.loads((root / f"report-{identity}.json").read_text())
        atomic_json(
            manifest,
            {"version": identity, "database": target.name, "sha256": digest(target)},
        )
        return payload
    rows, links = read_esco(archive, lock["esco"])
    overrides = editorial.get("escoOverrides", [])
    validate_overrides(overrides, {r["id"]: r for r in rows}, icon_catalog)
    by_id = {r["id"]: r for r in rows}
    for override in overrides:
        by_id[override["id"]].update(override)
    path = target.with_suffix(".building.sqlite")
    if path.exists():
        path.unlink()
    db = connect(path)
    db.executescript(
        """
        CREATE TABLE concepts(id TEXT PRIMARY KEY,uri TEXT UNIQUE,kind TEXT NOT NULL,category TEXT NOT NULL,metadata TEXT NOT NULL,search_text TEXT NOT NULL);
        CREATE VIRTUAL TABLE concept_search USING fts5(id UNINDEXED,text,tokenize='unicode61');
        CREATE TABLE concept_links(source TEXT,relation TEXT,target TEXT,PRIMARY KEY(source,relation,target));
        CREATE TABLE concept_icons(concept_id TEXT,icon_id TEXT,rank INTEGER,state TEXT,score REAL,PRIMARY KEY(concept_id,icon_id));
        CREATE TABLE state(key TEXT PRIMARY KEY,value TEXT);
    """
    )
    approved = 0
    missing = []
    for item in overrides:
        for rank, icon_id in enumerate(item.get("icons", [])):
            db.execute(
                "INSERT INTO concept_icons VALUES(?,?,?,?,?)",
                (item["id"], icon_id, rank, "curated", 1.0),
            )
            approved += 1
    local_ids = set()
    for item in editorial["concepts"]:
        if item["id"] in local_ids:
            raise ValueError("duplicate editorial concept ID")
        local_ids.add(item["id"])
        rows.append(
            {
                **item,
                "uri": None,
                "kind": "local",
                "source": "hatef",
                "sourceVersion": editorial["version"],
                "descriptionEn": item.get("descriptionEn", item["labelEn"]),
                "brand": bool(item.get("brand")),
            }
        )
        rank = 0
        for name in item["icons"]:
            ids = (
                [name]
                if ":" in name
                else ["lucide:" + name, "iconify:lucide:" + name, "iconify:mdi:" + name]
            )
            accepted = False
            for icon_id in ids:
                if icon_catalog.get(icon_id):
                    db.execute(
                        "INSERT OR IGNORE INTO concept_icons VALUES(?,?,?,?,?)",
                        (item["id"], icon_id, rank, "curated", 1.0),
                    )
                    approved += 1
                    accepted = True
            if not accepted:
                missing.append({"concept": item["id"], "icon": name})
            rank += 1
    for row in rows:
        text = " ".join(
            [
                row["labelEn"],
                row.get("labelFa", ""),
                *row.get("aliasesEn", []),
                *row.get("aliasesFa", []),
                row.get("descriptionEn", "")[:500],
                row.get("descriptionFa", ""),
            ]
        )
        db.execute(
            "INSERT INTO concepts VALUES(?,?,?,?,?,?)",
            (
                row["id"],
                row["uri"],
                row["kind"],
                row["kind"],
                json.dumps(row, ensure_ascii=False),
                normalize(text),
            ),
        )
        db.execute(
            "INSERT INTO concept_search VALUES(?,?)", (row["id"], normalize(text))
        )
    db.executemany("INSERT OR IGNORE INTO concept_links VALUES(?,?,?)", links)
    db.execute("INSERT INTO state VALUES(?,?)", ("provenance", json.dumps(lock)))
    db.commit()
    db.close()
    os.replace(path, target)
    report = {
        "version": identity,
        "escoOccupations": sum(r["kind"] == "occupation" for r in rows),
        "escoSkills": sum(r["kind"] == "skill" for r in rows),
        "escoGroups": sum(r["kind"] == "group" for r in rows),
        "editorialPersianConcepts": len(editorial["concepts"]),
        "relationships": len(links),
        "curatedIconMappings": approved,
        "missingEditorialIcons": missing,
        "translationStatusCounts": {
            "editorial": len(editorial["concepts"]),
            "reviewed": len(overrides),
            "untranslated": len(rows) - len(editorial["concepts"]) - len(overrides),
        },
        "provenance": lock,
    }
    atomic_json(root / f"report-{identity}.json", report)
    atomic_json(
        manifest,
        {"version": identity, "database": target.name, "sha256": digest(target)},
    )
    return report


def validate_overrides(overrides, concepts, icon_catalog):
    """Only explicitly reviewed Persian overlays may become trusted aliases."""
    seen = set()
    allowed = {
        "id",
        "labelFa",
        "aliasesFa",
        "descriptionFa",
        "translationStatus",
        "reviewer",
        "icons",
    }
    for row in overrides:
        if set(row) - allowed or row.get("id") not in concepts or row["id"] in seen:
            raise ValueError("unknown, duplicate or unsafe ESCO override")
        seen.add(row["id"])
        if (
            row.get("translationStatus") != "reviewed"
            or not str(row.get("reviewer", "")).strip()
        ):
            raise ValueError(
                "ESCO overlay requires an explicit reviewer and reviewed status"
            )
        if not isinstance(row.get("labelFa"), str) or not normalize(row["labelFa"]):
            raise ValueError("ESCO overlay needs a Persian title")
        if not any("\u0600" <= c <= "\u06ff" for c in row["labelFa"]):
            raise ValueError("ESCO overlay title must contain Persian text")
        if not isinstance(row.get("aliasesFa", []), list) or not all(
            isinstance(v, str) for v in row.get("aliasesFa", [])
        ):
            raise ValueError("invalid Persian aliases")
        if not isinstance(row.get("descriptionFa", ""), str):
            raise ValueError("invalid Persian description")
        icons = row.get("icons", [])
        if (
            not isinstance(icons, list)
            or not all(isinstance(v, str) for v in icons)
            or len(set(icons)) != len(icons)
            or any(not icon_catalog.get(v) for v in icons)
        ):
            raise ValueError("unknown or duplicate reviewed icon")


class ConceptDocuments:
    embedding_table = "concepts"

    def __init__(self, bank):
        self.bank = bank
        self.manifest = {"version": bank.manifest["version"]}
        with bank.db() as db:
            self.count = db.execute("SELECT count(*) FROM concepts").fetchone()[0]

    def db(self):
        return self.bank.db()


class KnowledgeBank:
    def __init__(self, root):
        self.root = Path(root)
        self.manifest = json.loads((self.root / "knowledge.json").read_text())
        name = self.manifest["database"]
        if Path(name).name != name:
            raise ValueError("unsafe knowledge path")
        self.path = self.root / name
        if digest(self.path) != self.manifest["sha256"]:
            raise ValueError("knowledge integrity mismatch")
        self.model = self.vectors = None
        with self.db() as db:
            self.editorial = [
                json.loads(r[0])
                for r in db.execute("SELECT metadata FROM concepts ORDER BY id")
            ]
        self.editorial = [r for r in self.editorial if r.get("labelFa")]
        self.brands = {r["id"] for r in self.editorial if r.get("brand")}
        self.generic = {
            "hatef:concept:" + key
            for key in [
                "teaching",
                "programming",
                "sports",
                "construction",
                "environment",
                "marketing",
                "communications",
            ]
        }
        self.aliases = []
        for row in self.editorial:
            for alias in [
                row["labelFa"],
                row["labelEn"],
                *row.get("aliasesFa", []),
                *row.get("aliasesEn", []),
            ]:
                if value := normalize(alias):
                    self.aliases.append((value, row["id"]))

    def db(self):
        from contextlib import contextmanager

        @contextmanager
        def opened():
            db = connect(self.path, True)
            try:
                yield db
            finally:
                db.close()

        return opened()

    def attach_vectors(self, model, directory):
        from .embeddings import Vectors

        self.model = model
        self.vectors = Vectors(directory, ConceptDocuments(self), model)

    def get(self, id_):
        with self.db() as db:
            row = db.execute(
                "SELECT metadata FROM concepts WHERE id=?", (id_,)
            ).fetchone()
            return json.loads(row[0]) if row else None

    def search(self, text, limit=10):
        tokens = normalize(text).split()[:48]
        if not tokens:
            return []
        with self.db() as db:
            rows = db.execute(
                "SELECT c.metadata FROM concepts c JOIN concept_search s ON s.id=c.id WHERE concept_search MATCH ? ORDER BY bm25(concept_search),c.id LIMIT ?",
                (" OR ".join('"' + t + '"' for t in tokens), limit),
            ).fetchall()
        lexical = [json.loads(row[0]) for row in rows]
        scores = {r["id"]: 1 / (60 + rank) for rank, r in enumerate(lexical)}
        found = {r["id"]: r for r in lexical}
        if self.model and self.vectors:
            for rank, (id_, similarity) in enumerate(
                self.vectors.nearest(self.model.query(text), limit)
            ):
                if similarity < 0.75:
                    continue
                scores[id_] = scores.get(id_, 0) + 1 / (60 + rank)
                if id_ not in found:
                    found[id_] = self.get(id_)
        for id_ in self.direct(text):
            scores[id_] = scores.get(id_, 0) + 1.0
            found[id_] = self.get(id_)
        return [
            found[id_]
            for id_ in sorted(scores, key=lambda id_: (-scores[id_], id_))[:limit]
        ]

    def direct(self, text):
        value = " " + normalize(text) + " "
        found = {}
        for alias, id_ in self.aliases:
            if " " + alias + " " in value:
                found[id_] = max(
                    found.get(id_, 0), len(alias.split()) + len(alias) / 100
                )
        if not found:
            return {}
        if any(id_ in self.brands for id_ in found):
            found = {id_: score for id_, score in found.items() if id_ in self.brands}
        elif any(id_ not in self.generic for id_ in found):
            found = {
                id_: score for id_, score in found.items() if id_ not in self.generic
            }
        longest = max(found.values())
        return {id_: score for id_, score in found.items() if score >= longest - 0.5}

    @functools.lru_cache(maxsize=128)
    def icon_candidates(self, text, model=None):
        direct = self.direct(text)
        out = {}
        with self.db() as db:
            for id_, specificity in direct.items():
                for row in db.execute(
                    "SELECT icon_id,rank FROM concept_icons WHERE concept_id=? AND state='curated' ORDER BY rank,icon_id",
                    (id_,),
                ):
                    value = {
                        "score": 2.5
                        + min(specificity, 3.0) * 0.15
                        - 0.13 * row["rank"],
                        "evidence": "curated",
                        "conceptId": id_,
                    }
                    if value["score"] > out.get(row["icon_id"], {}).get("score", 0):
                        out[row["icon_id"]] = value
        # Unreviewed concept mappings are suggestions, never an exact match.
        if not direct and model and self.model and self.vectors:
            near = self.vectors.nearest(self.model.query(text), 5)
            with self.db() as db:
                for id_, similarity in near:
                    if similarity < 0.78:
                        continue
                    for row in db.execute(
                        "SELECT icon_id,rank,state FROM concept_icons WHERE concept_id=? ORDER BY rank LIMIT 3",
                        (id_,),
                    ):
                        score = 0.35 * similarity - 0.02 * row["rank"]
                        if score > out.get(row["icon_id"], {}).get("score", 0):
                            out[row["icon_id"]] = {
                                "score": score,
                                "evidence": "semantic-concept",
                                "conceptId": id_,
                            }
        return out


def prepare_proposals(bank, icon_catalog, model, icon_vectors, concept_vectors):
    """Precompute suggestions. Machine proposals never authorize auto-selection."""
    import shutil
    import numpy as np

    if bank.path.stem.endswith("-mapped"):
        raise ValueError("mapping preparation requires the original bank")
    target = bank.root / (bank.path.stem + "-mapped.sqlite")
    report_path = bank.root / (bank.path.stem + "-mappings.json")
    if target.exists() and report_path.exists():
        report = json.loads(report_path.read_text())
        atomic_json(
            bank.root / "knowledge.json",
            {**bank.manifest, "database": target.name, "sha256": digest(target)},
        )
        return report
    matrix = []
    icon_ids = []
    for shard in icon_vectors.shards:
        matrix.append(np.load(icon_vectors.root / shard["vectors"], allow_pickle=False))
        icon_ids.extend(json.loads((icon_vectors.root / shard["ids"]).read_text()))
    matrix = np.concatenate(matrix, axis=0)
    icon_data = {id_: icon_catalog.get(id_) for id_ in icon_ids}
    temp = target.with_suffix(".building.sqlite")
    shutil.copyfile(bank.path, temp)
    db = connect(temp)
    count = 0
    total = 0
    for shard in concept_vectors.shards:
        ids = json.loads((concept_vectors.root / shard["ids"]).read_text())
        vectors = np.load(
            concept_vectors.root / shard["vectors"], mmap_mode="r", allow_pickle=False
        )
        for start in range(0, len(ids), 32):
            similarities = vectors[start : start + 32] @ matrix.T
            for offset, scores in enumerate(similarities):
                id_ = ids[start + offset]
                item = bank.get(id_)
                if item["kind"] == "local":
                    continue
                if db.execute(
                    "SELECT 1 FROM concept_icons WHERE concept_id=? AND state='curated' LIMIT 1",
                    (id_,),
                ).fetchone():
                    count += 1
                    total += 1
                    continue
                # Explicit source-title aliases may reuse an editorial family;
                # this does not manufacture a Persian translation of the title.
                exact = bank.icon_candidates(item["labelEn"])
                ranked = []
                if exact:
                    ranked = [
                        (icon_id, v["score"], "curated-rule")
                        for icon_id, v in exact.items()
                    ]
                else:
                    from .text import brand_matches

                    for pos in np.argsort(-scores, kind="stable")[:60]:
                        icon_id = icon_ids[pos]
                        if scores[pos] < 0.70:
                            continue
                        if not brand_matches(icon_data[icon_id], item["labelEn"]):
                            continue
                        ranked.append((icon_id, float(scores[pos]), "proposed"))
                seen = set()
                rank = 0
                for icon_id, score, state in ranked:
                    icon = icon_data.get(icon_id)
                    if not icon or icon["hash"] in seen:
                        continue
                    seen.add(icon["hash"])
                    db.execute(
                        "INSERT OR IGNORE INTO concept_icons VALUES(?,?,?,?,?)",
                        (id_, icon_id, rank, state, score),
                    )
                    rank += 1
                    if rank == 3:
                        break
                count += bool(rank)
                total += 1
            db.commit()
    report = {
        "conceptsConsidered": total,
        "conceptsWithMappings": count,
        "automaticSelectionAuthorized": False,
        "method": "editorial-alias rules or E5 cosine proposals; not human-reviewed",
    }
    db.commit()
    db.close()
    os.replace(temp, target)
    atomic_json(report_path, report)
    atomic_json(
        bank.root / "knowledge.json",
        {**bank.manifest, "database": target.name, "sha256": digest(target)},
    )
    return report
