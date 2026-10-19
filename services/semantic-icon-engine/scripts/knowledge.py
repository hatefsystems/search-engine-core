#!/usr/bin/env python3
"""Export review packets and merge explicitly reviewed Persian ESCO overlays."""

import argparse
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from app.catalog import Catalog
from app.ingest import atomic_json
from app.knowledge import KnowledgeBank, validate_overrides
from app.text import CONFIG


def export_review(bank, limit=100, offset=0):
    with bank.db() as db:
        rows = db.execute(
            "SELECT metadata FROM concepts WHERE kind!='local' ORDER BY id LIMIT ? OFFSET ?",
            (limit, offset),
        ).fetchall()
        items = []
        for raw in rows:
            item = json.loads(raw[0])
            proposals = [
                dict(r)
                for r in db.execute(
                    "SELECT icon_id,state,score FROM concept_icons WHERE concept_id=? ORDER BY rank",
                    (item["id"],),
                )
            ]
            items.append(
                {
                    "id": item["id"],
                    "sourceUri": item["uri"],
                    "labelEn": item["labelEn"],
                    "descriptionEn": item["descriptionEn"],
                    "proposals": proposals,
                    "labelFa": item.get("labelFa", ""),
                    "aliasesFa": item.get("aliasesFa", []),
                    "descriptionFa": item.get("descriptionFa", ""),
                    "icons": [],
                    "translationStatus": "pending",
                    "reviewer": "",
                }
            )
    return {
        "bankVersion": bank.manifest["version"],
        "notice": "Machine proposals require human review. Keep only the final approved icon IDs in icons.",
        "concepts": items,
    }


def import_review(bank, catalog, packet, existing):
    if packet.get("bankVersion") != bank.manifest["version"]:
        raise ValueError("review packet bank version mismatch")
    reviewed = []
    for row in packet["concepts"]:
        if row.get("translationStatus") != "reviewed":
            continue
        reviewed.append(
            {
                key: row[key]
                for key in [
                    "id",
                    "labelFa",
                    "aliasesFa",
                    "descriptionFa",
                    "icons",
                    "translationStatus",
                    "reviewer",
                ]
            }
        )
    if not reviewed:
        raise ValueError("packet has no explicitly reviewed entries")
    with bank.db() as db:
        concepts = {
            r[0]: True
            for r in db.execute("SELECT id FROM concepts WHERE kind!='local'")
        }
    validate_overrides(reviewed, concepts, catalog)
    merged = {r["id"]: r for r in existing.get("escoOverrides", [])}
    merged.update({r["id"]: r for r in reviewed})
    return {**existing, "escoOverrides": [merged[key] for key in sorted(merged)]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=["export-review", "import-reviewed"])
    parser.add_argument("--bank", required=True)
    parser.add_argument("--data")
    parser.add_argument("--input")
    parser.add_argument("--config", default=str(CONFIG / "knowledge-fa.json"))
    parser.add_argument("--limit", type=int, default=100)
    parser.add_argument("--offset", type=int, default=0)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    bank = KnowledgeBank(args.bank)
    if args.action == "export-review":
        if not 1 <= args.limit <= 1000 or args.offset < 0:
            parser.error("limit must be 1..1000 and offset nonnegative")
        result = export_review(bank, args.limit, args.offset)
    else:
        if not args.data or not args.input:
            parser.error("import-reviewed requires --data and --input")
        result = import_review(
            bank,
            Catalog(args.data),
            json.loads(Path(args.input).read_text()),
            json.loads(Path(args.config).read_text()),
        )
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    atomic_json(args.output, result)
