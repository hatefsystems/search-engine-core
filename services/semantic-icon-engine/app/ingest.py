"""Explicit administrative ingestion with per-icon checkpoints and atomic activation."""

import collections
import fcntl
import hashlib
import json
import os
import re
import subprocess
import unicodedata
from pathlib import Path
from .catalog import connect, initialize
from .svg import sanitize, valid_id, iconify_svg
from .text import CONFIG, VERSION, enrich, expand


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1024 * 1024), b""):
            h.update(b)
    return h.hexdigest()


def atomic_json(path, value):
    path = Path(path)
    tmp = path.with_suffix(path.suffix + ".tmp")
    with tmp.open("w") as f:
        json.dump(value, f, ensure_ascii=False, sort_keys=True, indent=2)
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)


def simple_slug(item):
    if item.get("slug"):
        return item["slug"]
    value = item["title"].lower()
    for old, new in {
        "+": "plus",
        ".": "dot",
        "&": "and",
        "đ": "d",
        "ħ": "h",
        "ı": "i",
        "ĸ": "k",
        "ŀ": "l",
        "ł": "l",
        "ß": "ss",
        "ŧ": "t",
    }.items():
        value = value.replace(old, new)
    return re.sub("[^a-z0-9]", "", unicodedata.normalize("NFD", value))


def records(source, root):
    if source == "lucide":
        for path in sorted((root / "icons").glob("*.svg")):
            data = (
                json.loads(path.with_suffix(".json").read_text())
                if path.with_suffix(".json").exists()
                else {}
            )
            yield path.stem, path.read_text(), data.get("tags", []) + data.get(
                "categories", []
            ), data
    else:
        items = json.loads((root / "_data/simple-icons.json").read_text())
        for item in items.get("icons", []) if isinstance(items, dict) else items:
            name = simple_slug(item)
            path = root / "icons" / f"{name}.svg"
            yield name, path.read_text() if path.exists() else "", [item["title"]], item


def ingest(root, upstream, policy_path=None, stop_after=None):
    root = Path(root)
    root.mkdir(parents=True, exist_ok=True)
    with (root / ".ingest.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        return _ingest(root, Path(upstream), policy_path, stop_after)


def _ingest(root, upstream, policy_path, stop_after):
    lock = json.loads((CONFIG / "sources.lock.json").read_text())
    policy = json.loads(Path(policy_path or CONFIG / "policy.json").read_text())
    fingerprint = hashlib.sha256(
        json.dumps(
            [
                policy,
                VERSION,
                (CONFIG / "synonyms.json").read_text(),
                *[
                    digest(Path(__file__).with_name(n))
                    for n in ("ingest.py", "text.py", "svg.py")
                ],
            ],
            sort_keys=True,
        ).encode()
    ).hexdigest()
    sources = {s: lock[s] for s in ("lucide", "simple-icons", "iconify")}
    identity = hashlib.sha256(
        json.dumps([sources, fingerprint], sort_keys=True).encode()
    ).hexdigest()[:24]
    target = root / f"catalog-{identity}.sqlite"
    building = root / f"building-{identity}.sqlite"
    report_path = root / f"report-{identity}.json"
    if target.exists() and report_path.exists():
        atomic_json(
            root / "current.json",
            {"version": identity, "catalog": target.name, "sha256": digest(target)},
        )
        return json.loads(report_path.read_text())
    db = connect(building)
    initialize(db)
    count = 0
    reused = set()
    # Copy unchanged sources only when normalization, policy and parser match.
    if (root / "current.json").exists():
        old = json.loads((root / "current.json").read_text())
        prior = root / f"report-{old['version']}.json"
        if prior.exists():
            previous = json.loads(prior.read_text())
            if previous.get("metadataFingerprint") == fingerprint:
                if Path(old["catalog"]).name != old["catalog"]:
                    raise ValueError("unsafe catalog manifest")
                db.execute(
                    "ATTACH DATABASE ? AS previous", (str(root / old["catalog"]),)
                )
                for source in sources:
                    if previous["sourceLock"][source] != lock[source]:
                        continue
                    if not db.execute(
                        "SELECT 1 FROM state WHERE key=?", ("copied:" + source,)
                    ).fetchone():
                        with db:
                            db.execute(
                                "INSERT OR IGNORE INTO icons SELECT * FROM previous.icons WHERE source=?",
                                (source,),
                            )
                            db.execute(
                                "INSERT OR IGNORE INTO assets SELECT a.* FROM previous.assets a WHERE a.hash IN (SELECT hash FROM previous.icons WHERE source=?)",
                                (source,),
                            )
                            db.execute(
                                "INSERT INTO search SELECT id,search_text FROM previous.icons WHERE source=?",
                                (source,),
                            )
                            db.execute(
                                "INSERT OR REPLACE INTO decisions SELECT * FROM previous.decisions WHERE source=?",
                                (source,),
                            )
                            db.execute(
                                "INSERT OR REPLACE INTO sources SELECT * FROM previous.sources WHERE id=? OR id LIKE ?",
                                (source, source + ":%"),
                            )
                            db.execute(
                                "INSERT INTO state VALUES(?,?)",
                                ("copied:" + source, "1"),
                            )
                    reused.add(source)
                db.execute("DETACH DATABASE previous")

    def decision(icon_id, status, reason, source):
        db.execute(
            "INSERT OR REPLACE INTO decisions VALUES(?,?,?,?)",
            (icon_id, status, reason, source),
        )

    def add(icon_id, svg, tags, raw, source, provenance, style, brand=False):
        nonlocal count
        if db.execute("SELECT 1 FROM decisions WHERE id=?", (icon_id,)).fetchone():
            return
        try:
            if not valid_id(icon_id):
                raise ValueError("invalid ID")
            individual = raw.get("license", {}).get("type", "CC0-1.0")
            if brand and individual not in policy["allowedLicenses"]:
                decision(
                    icon_id, "excluded", "individual icon license disallowed", source
                )
                return
            review = policy.get("iconLicenseReviews", {}).get(icon_id, {})
            if (
                brand
                and individual != "CC0-1.0"
                and not (
                    review.get("license") == individual and review.get("licenseText")
                )
            ):
                decision(
                    icon_id,
                    "excluded",
                    "individual license notice needs review",
                    source,
                )
                return
            if brand and icon_id not in policy["publicBrands"]:
                decision(icon_id, "excluded", "brand exposure requires review", source)
                return
            safe, hash_ = sanitize(svg)
            editorial = policy.get("metadataOverrides", {}).get(icon_id, {})
            extra = editorial.get("keywordsFa", []) + editorial.get("keywordsEn", [])
            if not isinstance(extra, list) or any(
                not isinstance(t, str) or len(t) > 200 for t in extra
            ):
                raise ValueError("invalid editorial keywords")
            tags = list(tags) + extra
            cats, fa = enrich(icon_id.split(":")[-1], tags)
            fa = editorial.get("labelFa", fa)
            if not isinstance(fa, str) or len(fa) > 500:
                raise ValueError("invalid editorial label")
            origin = {
                "sourceRecord": source
                + (
                    ":" + provenance["collection"] if "collection" in provenance else ""
                ),
                "revision": provenance["revision"],
                "license": provenance["license"],
            }
            metadata = {
                "labelEn": raw.get("title", icon_id.split(":")[-1].replace("-", " ")),
                "labelFa": fa,
                "keywords": tags,
                "categories": cats,
                "metadataVersion": 1,
                "persianProvenance": (
                    "editorial override"
                    if editorial
                    else "curated dictionary; automatic association"
                ),
                "origin": origin,
                "raw": raw,
                "brand": brand,
            }
            duplicate = (
                db.execute("SELECT 1 FROM assets WHERE hash=?", (hash_,)).fetchone()
                is not None
            )
            db.execute("INSERT OR IGNORE INTO assets VALUES(?,?)", (hash_, safe))
            text = expand(
                " ".join(
                    [
                        icon_id.split(":")[-1].replace("-", " "),
                        metadata["labelEn"],
                        fa,
                        *tags,
                    ]
                )
            )
            db.execute(
                "INSERT INTO icons VALUES(?,?,?,?,?,?,?,?,?)",
                (
                    icon_id,
                    source,
                    icon_id.split(":")[-1],
                    cats[0] if cats else "",
                    style,
                    int(brand),
                    hash_,
                    json.dumps(metadata, ensure_ascii=False),
                    text,
                ),
            )
            db.execute("INSERT INTO search VALUES(?,?)", (icon_id, text))
            decision(icon_id, "duplicated" if duplicate else "imported", "", source)
        except (ValueError, KeyError, TypeError) as e:
            decision(icon_id, "rejected", str(e)[:300], source)
        count += 1
        if count % 100 == 0:
            db.commit()
        if stop_after and count >= stop_after:
            db.commit()
            db.close()
            raise InterruptedError("checkpoint interruption")

    for source in sources:
        if source in reused:
            continue
        directory = upstream / source
        revision = subprocess.check_output(
            ["git", "-C", str(directory), "rev-parse", "HEAD"], text=True
        ).strip()
        if revision != lock[source]["revision"]:
            raise ValueError("source revision mismatch: " + source)
        if subprocess.check_output(
            [
                "git",
                "-C",
                str(directory),
                "status",
                "--porcelain",
                "--untracked-files=no",
            ],
            text=True,
        ).strip():
            raise ValueError("dirty source: " + source)
        if source != "iconify":
            license_path = next(
                (
                    p
                    for p in (directory / "LICENSE", directory / "LICENSE.md")
                    if p.exists()
                ),
                None,
            )
            notice = license_path.read_text() if license_path else ""
            license_ = (
                ("ISC" if "ISC License" in notice else "")
                if source == "lucide"
                else ("CC0-1.0" if "CC0 1.0 Universal" in notice else "")
            )
            if not license_ or license_ not in policy["allowedLicenses"]:
                raise ValueError("source license unapproved")
            provenance = {**lock[source], "license": license_, "licenseText": notice}
            db.execute(
                "INSERT OR REPLACE INTO sources VALUES(?,?)",
                (source, json.dumps(provenance)),
            )
            for name, svg, tags, raw in records(source, directory):
                add(
                    source + ":" + name,
                    svg,
                    tags,
                    raw,
                    source,
                    provenance,
                    "outline" if source == "lucide" else "brand",
                    source == "simple-icons",
                )
        else:
            for path in sorted((directory / "json").glob("*.json")):
                pack = json.loads(path.read_text())
                prefix = pack.get("prefix", path.stem)
                info = pack.get("info", {})
                license_ = info.get("license", {})
                spdx = license_.get("spdx", "")
                review = policy["reviewedCollections"].get(prefix, {})
                reason = ""
                if spdx not in policy["allowedLicenses"]:
                    reason = "unknown or disallowed license"
                elif not review.get("licenseText"):
                    reason = "original license notice needs review"
                elif (
                    review.get("revision") != revision or review.get("license") != spdx
                ):
                    reason = "collection review mismatch"
                provenance = {
                    **lock[source],
                    "collection": prefix,
                    "info": info,
                    "license": license_,
                    "licenseText": review.get("licenseText", ""),
                    "themes": pack.get("themes", {}),
                    "prefixes": pack.get("prefixes", {}),
                    "suffixes": pack.get("suffixes", {}),
                    "sha256": digest(path),
                }
                db.execute(
                    "INSERT OR REPLACE INTO sources VALUES(?,?)",
                    ("iconify:" + prefix, json.dumps(provenance)),
                )
                tags = collections.defaultdict(list)
                for category, names in pack.get("categories", {}).items():
                    for name in names:
                        tags[name].append(category)
                for name in sorted(
                    set(pack.get("icons", {})) | set(pack.get("aliases", {}))
                ):
                    icon_id = f"iconify:{prefix}:{name}"
                    if reason:
                        decision(icon_id, "excluded", reason, source)
                        continue
                    try:
                        svg = iconify_svg(pack, name)
                    except (ValueError, KeyError, TypeError) as e:
                        decision(icon_id, "rejected", str(e)[:300], source)
                        continue
                    add(
                        icon_id,
                        svg,
                        tags[name],
                        {
                            "title": name.replace("-", " "),
                            "alias": pack.get("aliases", {}).get(name),
                        },
                        source,
                        provenance,
                        review.get("style", "mixed"),
                        review.get("brands", True),
                    )
                db.commit()
        db.commit()
    reports = {
        s: {
            "counts": {
                r[0]: r[1]
                for r in db.execute(
                    "SELECT status,count(*) FROM decisions WHERE source=? GROUP BY status",
                    (s,),
                )
            },
            "reasons": {
                r[0]: r[1]
                for r in db.execute(
                    "SELECT reason,count(*) FROM decisions WHERE source=? AND reason!='' GROUP BY reason",
                    (s,),
                )
            },
        }
        for s in sources
    }
    total = db.execute("SELECT count(*) FROM icons").fetchone()[0]
    db.execute(
        "INSERT OR REPLACE INTO state VALUES(?,?)", ("policy", json.dumps(policy))
    )
    db.commit()
    db.close()
    os.replace(building, target)
    report = {
        "version": identity,
        "sources": reports,
        "total": total,
        "diskBytes": target.stat().st_size,
        "completeDiscovery": True,
        "sourceLock": sources,
        "metadataFingerprint": fingerprint,
        "reusedSources": sorted(reused),
        "policy": policy,
    }
    atomic_json(report_path, report)
    atomic_json(
        root / "current.json",
        {"version": identity, "catalog": target.name, "sha256": digest(target)},
    )
    return report
