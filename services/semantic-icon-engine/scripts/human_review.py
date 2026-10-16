#!/usr/bin/env python3
"""Offline human review packets and a blind, two-rater acceptance protocol.

Names and attestations document a process; this tool cannot authenticate humans.
No search-engine predictions are exported to annotators.
"""

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from app.catalog import Catalog
from app.ingest import atomic_json, digest
from app.knowledge import read_esco, validate_overrides
from app.text import CONFIG, normalize
from scripts.translate_esco import load_drafts, source_hash

PROTOCOL = "hatef-blind-two-rater-v1"
DEVELOPMENT = Path(__file__).resolve().parents[1] / "tests/evaluation-fa-500.json"


def development_reference():
    return json.loads((CONFIG / "development-reference-fa.json").read_text())


def phrase_hash(text):
    return hashlib.sha256(normalize(text).encode()).hexdigest()


def identity(value):
    return hashlib.sha256(
        json.dumps(value, ensure_ascii=False, sort_keys=True).encode()
    ).hexdigest()


def write_new(path, value):
    path = Path(path)
    if path.exists():
        raise ValueError("refusing to overwrite an existing review artifact")
    path.parent.mkdir(parents=True, exist_ok=True)
    atomic_json(path, value)


def new_study(engine_revision, catalog_version):
    if (
        not re.fullmatch(r"[a-f0-9]{40}", engine_revision)
        or not catalog_version.strip()
    ):
        raise ValueError("pin an exact engine commit and nonempty catalog version")
    domains = development_reference()["domains"]
    return {
        "schema": 1,
        "protocol": PROTOCOL,
        "mode": "authoring",
        "state": "collecting",
        "engineRevision": engine_revision,
        "catalogVersion": catalog_version,
        "developmentSha256": development_reference()["fileSha256"],
        "exclusionReferenceSha256": digest(CONFIG / "development-reference-fa.json"),
        "notice": "Humans must supply unseen phrases. Do not copy assistant templates or engine suggestions. Do not send a sealed holdout to developers before freeze.",
        "cases": [
            {
                "id": f"holdout-{i + 1:03d}",
                "domain": domain,
                "text": "",
                "queryAuthor": "",
                "querySource": "human-authored",
                "humanAuthored": False,
                "queryAuthorIndependentFromDevelopment": False,
            }
            for i, domain in enumerate(domain for domain in domains for _ in range(20))
        ],
    }


def seal_queries(packet):
    if packet.get("protocol") != PROTOCOL or packet.get("state") != "collecting":
        raise ValueError("expected an unsealed human query collection")
    if packet.get("developmentSha256") != development_reference()["fileSha256"]:
        raise ValueError("development reference changed after query collection started")
    if packet.get("exclusionReferenceSha256") != digest(
        CONFIG / "development-reference-fa.json"
    ):
        raise ValueError("query exclusion reference changed after collection started")
    if not re.fullmatch(
        r"[a-f0-9]{40}", packet.get("engineRevision", "")
    ) or not packet.get("catalogVersion"):
        raise ValueError("pin the exact engine and catalog before collecting queries")
    rows = packet["cases"]
    domains = set(development_reference()["domains"])
    if (
        len(rows) != 500
        or len({r["id"] for r in rows}) != 500
        or Counter(r["domain"] for r in rows) != Counter({d: 20 for d in domains})
    ):
        raise ValueError(
            "acceptance requires exactly 500 cases, 25 domains, 20 per domain"
        )
    development = set(
        development_reference()["normalizedQuerySha256"]
        + development_reference()["candidateQuerySha256"]
    )
    texts = set()
    for row in rows:
        text = normalize(row["text"])
        if (
            not text
            or not re.search(r"[\u0621-\u06cc]", text)
            or text in texts
            or phrase_hash(text) in development
        ):
            raise ValueError("queries must be distinct, unseen Persian phrases")
        if (
            not row.get("queryAuthor", "").strip()
            or row.get("humanAuthored") is not True
            or row.get("queryAuthorIndependentFromDevelopment") is not True
            or row.get("querySource") not in ("human-authored", "consented-user-query")
        ):
            raise ValueError("every phrase needs human authorship/source attribution")
        if any(
            key in row
            for key in ("relevant", "top3", "proposedRelevant", "predictions")
        ):
            raise ValueError(
                "query collection must not contain relevance labels or predictions"
            )
        texts.add(text)
    sealed = {
        key: packet[key]
        for key in (
            "schema",
            "protocol",
            "engineRevision",
            "catalogVersion",
            "developmentSha256",
            "exclusionReferenceSha256",
            "cases",
        )
    }
    sealed.update(state="queries-sealed", mode="sealed", querySetSha256=identity(rows))
    return sealed


def annotation_packet(study, reviewer):
    if study.get("state") != "queries-sealed" or study["querySetSha256"] != identity(
        study["cases"]
    ):
        raise ValueError("queries must be sealed and unmodified")
    if not reviewer.strip():
        raise ValueError("annotator name required")
    return {
        "schema": 1,
        "protocol": PROTOCOL,
        "mode": "annotation",
        "querySetSha256": study["querySetSha256"],
        "engineRevision": study["engineRevision"],
        "catalogVersion": study["catalogVersion"],
        "reviewer": reviewer,
        "isHuman": False,
        "independentFromDevelopment": False,
        "blindToEngineResults": False,
        "notice": "Judge the complete catalog by artwork and meaning. Icon browsing is canonical-ID ordered and is not engine ranking. Do not inspect development labels or other annotators' choices. Empty suitability is a scored miss, never an excluded case.",
        "cases": [
            {
                "id": r["id"],
                "domain": r["domain"],
                "text": r["text"],
                "relevant": [],
                "noSuitableIcon": False,
                "reviewStatus": "pending",
                "notes": "",
            }
            for r in study["cases"]
        ],
    }


def validate_annotation(study, packet, catalog, required_ids=None):
    if packet.get("protocol") != PROTOCOL or packet.get("mode") not in (
        "annotation",
        "adjudication",
    ):
        raise ValueError("invalid annotation protocol")
    for key in ("querySetSha256", "engineRevision", "catalogVersion"):
        if packet.get(key) != study[key]:
            raise ValueError("annotation identity mismatch: " + key)
    name = str(packet.get("reviewer", "")).strip()
    if not name or not all(
        packet.get(key) is True
        for key in ("isHuman", "independentFromDevelopment", "blindToEngineResults")
    ):
        raise ValueError(
            "explicit human, independence and blinding attestations required"
        )
    if name.casefold() in {r["queryAuthor"].strip().casefold() for r in study["cases"]}:
        raise ValueError("query authors cannot annotate their own acceptance set")
    originals = {r["id"]: r for r in study["cases"]}
    rows = packet["cases"]
    required_ids = set(originals) if required_ids is None else required_ids
    if len(rows) != len(required_ids) or {r["id"] for r in rows} != required_ids:
        raise ValueError(
            "annotation must cover every required sealed case exactly once"
        )
    found = {}
    for row in rows:
        original = originals[row["id"]]
        if row["text"] != original["text"] or row["domain"] != original["domain"]:
            raise ValueError("annotator changed a sealed query")
        ids = row.get("relevant")
        if (
            row.get("reviewStatus") != "reviewed"
            or not isinstance(ids, list)
            or any(not isinstance(v, str) or not catalog.get(v) for v in ids)
            or len(set(ids)) != len(ids)
        ):
            raise ValueError("reviewed, valid unique icon labels required")
        if bool(ids) == (row.get("noSuitableIcon") is True):
            raise ValueError(
                "choose acceptable artwork or explicitly mark no suitable icon"
            )
        if any(key in row for key in ("top3", "proposedRelevant", "predictions")):
            raise ValueError("engine predictions must not enter blind annotations")
        found[row["id"]] = row
    return name, found


def agreement_key(row):
    return (tuple(sorted(row["relevant"])), bool(row.get("noSuitableIcon")))


def adjudication_packet(study, first, second, catalog, reviewer):
    a, aa = validate_annotation(study, first, catalog)
    b, bb = validate_annotation(study, second, catalog)
    if a.casefold() == b.casefold() or reviewer.strip().casefold() in (
        a.casefold(),
        b.casefold(),
    ):
        raise ValueError("distinct annotators and a third adjudicator required")
    packet = annotation_packet(study, reviewer)
    packet["mode"] = "adjudication"
    packet["notice"] = (
        "Resolve only these two-rater disagreements. Previous human labels are visible for adjudication; engine predictions remain hidden."
    )
    packet["cases"] = [
        {**r, "raterChoices": {a: aa[r["id"]]["relevant"], b: bb[r["id"]]["relevant"]}}
        for r in packet["cases"]
        if agreement_key(aa[r["id"]]) != agreement_key(bb[r["id"]])
    ]
    if not packet["cases"]:
        raise ValueError("no disagreements require adjudication")
    return packet


def freeze(study, first, second, catalog, adjudication=None):
    if (
        study.get("state") != "queries-sealed"
        or identity(study["cases"]) != study["querySetSha256"]
    ):
        raise ValueError("sealed query digest mismatch")
    if catalog.manifest["version"] != study["catalogVersion"]:
        raise ValueError("catalog differs from the sealed study")
    a, aa = validate_annotation(study, first, catalog)
    b, bb = validate_annotation(study, second, catalog)
    if a.casefold() == b.casefold():
        raise ValueError("two distinct human annotators required")
    disagreements = [
        r["id"]
        for r in study["cases"]
        if agreement_key(aa[r["id"]]) != agreement_key(bb[r["id"]])
    ]
    judge = jj = None
    if adjudication:
        judge, jj = validate_annotation(
            study, adjudication, catalog, set(disagreements)
        )
        if judge.casefold() in (a.casefold(), b.casefold()):
            raise ValueError("adjudicator must be a third person")
    if disagreements and not jj:
        raise ValueError(
            f"{len(disagreements)} annotation disagreements need a third-person adjudication packet"
        )
    rows = []
    for original in study["cases"]:
        id_ = original["id"]
        selected = jj[id_] if id_ in disagreements else aa[id_]
        rows.append(
            {
                **original,
                "relevant": sorted(selected["relevant"]),
                "noSuitableIcon": bool(selected.get("noSuitableIcon")),
                "reviewStatus": "reviewed",
                "reviewer": f"{a}; {b}"
                + (f"; {judge}" if id_ in disagreements else ""),
                "acceptanceEvidence": {
                    "protocol": PROTOCOL,
                    "querySetSha256": study["querySetSha256"],
                    "exclusionReferenceSha256": study["exclusionReferenceSha256"],
                    "engineRevision": study["engineRevision"],
                    "catalogVersion": study["catalogVersion"],
                    "annotators": [a, b],
                    "adjudicator": judge if id_ in disagreements else None,
                    "humanAndIndependentAttested": True,
                    "blindToEngineResultsAttested": True,
                },
            }
        )
    return rows, {
        "protocol": PROTOCOL,
        "querySetSha256": study["querySetSha256"],
        "exclusionReferenceSha256": study["exclusionReferenceSha256"],
        "engineRevision": study["engineRevision"],
        "catalogVersion": study["catalogVersion"],
        "queries": len(rows),
        "annotators": [a, b],
        "adjudicator": judge,
        "disagreements": len(disagreements),
        "exactSetAgreement": 1 - len(disagreements) / len(rows),
        "firstPacketSha256": identity(first),
        "secondPacketSha256": identity(second),
        "adjudicationPacketSha256": identity(adjudication) if adjudication else None,
        "frozenLabelsSha256": identity(rows),
        "identityAuthenticated": False,
        "notice": "Human attribution and independence are process attestations, not automatically authenticated identities. Keep these labels hidden from development until the acceptance run.",
    }


def validate_independent_cases(cases, catalog_version=None, engine_revision=None):
    if len(cases) != 500:
        raise ValueError("independent acceptance requires all 500 cases")
    authors = []
    signatures = set()
    for row in cases:
        evidence = row.get("acceptanceEvidence", {})
        if evidence.get("exclusionReferenceSha256") != digest(
            CONFIG / "development-reference-fa.json"
        ):
            raise ValueError("independent acceptance exclusion reference mismatch")
        raters = evidence.get("annotators", [])
        if (
            evidence.get("protocol") != PROTOCOL
            or evidence.get("humanAndIndependentAttested") is not True
            or evidence.get("blindToEngineResultsAttested") is not True
            or len(raters) != 2
            or len({v.strip().casefold() for v in raters}) != 2
        ):
            raise ValueError("missing independent two-rater acceptance evidence")
        if any(not v.strip() for v in raters) or row.get(
            "queryAuthor", ""
        ).strip().casefold() in {v.strip().casefold() for v in raters}:
            raise ValueError("independent annotators must differ from query authors")
        judge = evidence.get("adjudicator")
        if judge and judge.strip().casefold() in {v.strip().casefold() for v in raters}:
            raise ValueError("adjudicator must be a third person")
        if catalog_version and evidence.get("catalogVersion") != catalog_version:
            raise ValueError("independent acceptance catalog mismatch")
        if engine_revision and evidence.get("engineRevision") != engine_revision:
            raise ValueError("independent acceptance engine revision mismatch")
        signatures.add(
            (
                evidence.get("querySetSha256"),
                evidence.get("engineRevision"),
                evidence.get("catalogVersion"),
            )
        )
        authors.append(
            {
                key: row[key]
                for key in (
                    "id",
                    "domain",
                    "text",
                    "queryAuthor",
                    "querySource",
                    "humanAuthored",
                    "queryAuthorIndependentFromDevelopment",
                )
            }
        )
    if (
        len(signatures) != 1
        or identity(authors) != cases[0]["acceptanceEvidence"]["querySetSha256"]
    ):
        raise ValueError("independent acceptance query digest mismatch")
    # Re-run collection checks to reject development overlap and broken quotas.
    sealed = seal_queries(
        {
            "schema": 1,
            "protocol": PROTOCOL,
            "state": "collecting",
            "engineRevision": next(iter(signatures))[1],
            "catalogVersion": next(iter(signatures))[2],
            "developmentSha256": development_reference()["fileSha256"],
            "exclusionReferenceSha256": digest(
                CONFIG / "development-reference-fa.json"
            ),
            "cases": authors,
        }
    )
    if sealed["querySetSha256"] != next(iter(signatures))[0]:
        raise ValueError("independent acceptance query identity mismatch")


def translation_packets(archive, output):
    spec = json.loads((CONFIG / "knowledge.lock.json").read_text())["esco"]
    rows, _ = read_esco(archive, spec)
    manifest, drafts = load_drafts(CONFIG / "esco-fa-drafts/manifest.json", rows, spec)
    rows.sort(key=lambda row: (not bool(drafts[row["id"]]["qualityFlags"]), row["id"]))
    out = Path(output)
    out.mkdir(parents=True, exist_ok=True)
    packets = []
    for start in range(0, len(rows), 100):
        batch = [
            {
                "id": row["id"],
                "sourceUri": row["uri"],
                "sourceSha256": source_hash(row),
                "labelEn": row["labelEn"],
                "aliasesEn": row["aliasesEn"],
                "descriptionEn": row["descriptionEn"],
                **{
                    key: drafts[row["id"]][key]
                    for key in ("labelFa", "aliasesFa", "descriptionFa", "qualityFlags")
                },
                "icons": [],
                "translationStatus": "pending",
                "reviewer": "",
                "humanReviewed": False,
            }
            for row in rows[start : start + 100]
        ]
        filename = f"translation-review-{start // 100:03d}.json"
        write_new(
            out / filename,
            {
                "schema": 1,
                "mode": "translation",
                "sourceSha256": spec["sha256"],
                "draftManifestSha256": digest(CONFIG / "esco-fa-drafts/manifest.json"),
                "notice": manifest["notice"],
                "concepts": batch,
            },
        )
        packets.append(filename)
    shutil.copyfile(Path(__file__).with_name("review.html"), out / "review.html")
    atomic_json(
        out / "index.json",
        {"records": len(rows), "packets": packets, "humanReviewed": 0},
    )
    return {"packets": len(packets), "records": len(rows), "humanReviewed": 0}


def import_translations(archive, packet, catalog, existing):
    spec = json.loads((CONFIG / "knowledge.lock.json").read_text())["esco"]
    rows, _ = read_esco(archive, spec)
    originals = {row["id"]: row for row in rows}
    if (
        packet.get("mode") != "translation"
        or packet.get("sourceSha256") != spec["sha256"]
    ):
        raise ValueError("translation packet source mismatch")
    manifest = CONFIG / "esco-fa-drafts/manifest.json"
    if packet.get("draftManifestSha256") != digest(manifest):
        raise ValueError("translation draft snapshot changed since review export")
    reviewed, seen = [], set()
    for row in packet["concepts"]:
        if row["id"] not in originals or row["id"] in seen:
            raise ValueError("unknown or duplicate translation concept")
        seen.add(row["id"])
        original = originals[row["id"]]
        if (
            row.get("sourceSha256") != source_hash(original)
            or any(
                row.get(k) != original[k]
                for k in ("labelEn", "aliasesEn", "descriptionEn")
            )
            or row.get("sourceUri") != original["uri"]
        ):
            raise ValueError("reviewer changed an immutable source record")
        if row.get("translationStatus") != "reviewed":
            continue
        if row.get("humanReviewed") is not True:
            raise ValueError("explicit personal human review required")
        if original["descriptionEn"] and not str(row.get("descriptionFa", "")).strip():
            raise ValueError(
                "reviewed description cannot be empty for a defined source"
            )
        reviewed.append(
            {
                k: row[k]
                for k in (
                    "id",
                    "labelFa",
                    "aliasesFa",
                    "descriptionFa",
                    "translationStatus",
                    "reviewer",
                    "icons",
                )
            }
        )
    if not reviewed:
        raise ValueError("no human-reviewed translations to import")
    validate_overrides(reviewed, originals, catalog)
    merged = {row["id"]: row for row in existing.get("escoOverrides", [])}
    merged.update({row["id"]: row for row in reviewed})
    return {**existing, "escoOverrides": [merged[key] for key in sorted(merged)]}


def export_catalog(catalog):
    # All approved artwork in canonical order; no matcher/model/scores involved.
    with catalog.db() as db:
        return {
            "mode": "catalog",
            "catalogVersion": catalog.manifest["version"],
            "sources": [
                {"id": row["id"], **json.loads(row["metadata"])}
                for row in db.execute("SELECT id,metadata FROM sources ORDER BY id")
            ],
            "icons": [
                {
                    "id": row["id"],
                    "labelEn": json.loads(row["metadata"])["labelEn"],
                    "labelFa": json.loads(row["metadata"]).get("labelFa", ""),
                    "svg": row["svg"],
                }
                for row in db.execute(
                    "SELECT i.id,i.metadata,a.svg FROM icons i JOIN assets a ON a.hash=i.hash ORDER BY i.id"
                )
            ],
        }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "action",
        choices=[
            "new-study",
            "seal-queries",
            "export-labels",
            "export-adjudication",
            "freeze",
            "translations",
            "import-translations",
            "export-catalog",
        ],
    )
    parser.add_argument("--input")
    parser.add_argument("--first")
    parser.add_argument("--second")
    parser.add_argument("--adjudication")
    parser.add_argument("--reviewer")
    parser.add_argument("--engine-revision")
    parser.add_argument("--catalog-version")
    parser.add_argument("--archive")
    parser.add_argument("--data")
    parser.add_argument("--config", default=str(CONFIG / "knowledge-fa.json"))
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    required = {
        "seal-queries": ["input"],
        "export-labels": ["input", "reviewer"],
        "export-adjudication": ["input", "first", "second", "data", "reviewer"],
        "freeze": ["input", "first", "second", "data"],
        "translations": ["archive"],
        "import-translations": ["archive", "input", "data"],
        "export-catalog": ["data"],
    }
    missing = [
        "--" + name.replace("_", "-")
        for name in required.get(args.action, [])
        if not getattr(args, name)
    ]
    if missing:
        parser.error(args.action + " requires " + ", ".join(missing))
    read = lambda path: json.loads(Path(path).read_text())
    if args.action == "new-study":
        result = new_study(args.engine_revision or "", args.catalog_version or "")
    elif args.action == "translations":
        print(json.dumps(translation_packets(args.archive, args.output)))
        return
    elif args.action == "export-catalog":
        result = export_catalog(Catalog(args.data))
    elif args.action == "import-translations":
        result = import_translations(
            args.archive, read(args.input), Catalog(args.data), read(args.config)
        )
    elif args.action == "seal-queries":
        result = seal_queries(read(args.input))
    elif args.action == "export-labels":
        result = annotation_packet(read(args.input), args.reviewer or "")
    elif args.action == "export-adjudication":
        result = adjudication_packet(
            read(args.input),
            read(args.first),
            read(args.second),
            Catalog(args.data),
            args.reviewer or "",
        )
    else:
        if (
            Path(args.output).exists()
            or Path(str(args.output) + ".report.json").exists()
        ):
            parser.error("freeze requires new output and report paths")
        result, report = freeze(
            read(args.input),
            read(args.first),
            read(args.second),
            Catalog(args.data),
            read(args.adjudication) if args.adjudication else None,
        )
        write_new(str(args.output) + ".report.json", report)
    write_new(args.output, result)
    shutil.copyfile(
        Path(__file__).with_name("review.html"),
        Path(args.output).parent / "review.html",
    )
    print(json.dumps({"output": args.output, "humanWorkPerformedByThisCommand": False}))


if __name__ == "__main__":
    main()
