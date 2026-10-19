#!/usr/bin/env python3
"""Apply source-bound assistant edits without manufacturing human approval."""
import argparse
import copy
import json
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from app.ingest import digest
from app.knowledge import read_esco
from app.text import CONFIG, normalize
from scripts.translate_esco import (
    load_drafts,
    write_drafts,
    source_hash,
    quality_flags,
    has_persian,
)


def apply_corrections(originals, drafts, corrections, source_spec):
    if (
        corrections.get("schema") != 1
        or corrections.get("sourceSha256") != source_spec["sha256"]
        or corrections.get("translationStatus") != "machine-draft"
        or corrections.get("humanReviewed") != 0
    ):
        raise ValueError("assistant correction source or review status mismatch")
    originals = {r["id"]: r for r in originals}
    result, seen = copy.deepcopy(drafts), set()
    for change in corrections["corrections"]:
        id_ = change["id"]
        if (
            id_ not in originals
            or id_ in seen
            or set(change)
            - {"id", "labelEn", "sourceSha256", "labelFa", "descriptionFa"}
        ):
            raise ValueError("unknown, duplicate or unsafe assistant correction")
        seen.add(id_)
        source, draft = originals[id_], result[id_]
        if (
            change.get("sourceSha256") != source_hash(source)
            or change.get("labelEn") != source["labelEn"]
        ):
            raise ValueError("assistant correction source record mismatch")
        if not isinstance(change.get("labelFa"), str) or not has_persian(
            change["labelFa"]
        ):
            raise ValueError("assistant title correction needs Persian text")
        draft["labelFa"] = change["labelFa"]
        if "descriptionFa" in change:
            if not isinstance(change["descriptionFa"], str) or not has_persian(
                change["descriptionFa"]
            ):
                raise ValueError("assistant description correction needs Persian text")
            draft["descriptionFa"] = change["descriptionFa"]
        # The original English alias remains immutable. Replace only its
        # corresponding draft when it is the same phrase as this source title.
        for i, alias in enumerate(source["aliasesEn"]):
            if normalize(alias) == normalize(source["labelEn"]):
                draft["aliasesFa"][i] = change["labelFa"]
        retained = [
            f
            for f in draft["qualityFlags"]
            if f in ("adaptive-segmentation", "untranslated-fragment")
        ]
        draft["qualityFlags"] = quality_flags(source, draft) + retained
        draft["translationStatus"], draft["reviewer"] = "machine-draft", ""
    return result


def correct(archive, drafts, corrections, output):
    source_spec = json.loads((CONFIG / "knowledge.lock.json").read_text())["esco"]
    originals, _ = read_esco(archive, source_spec)
    originals.sort(key=lambda r: r["id"])
    base_path = Path(drafts) / "manifest.json"
    base, rows = load_drafts(base_path, originals, source_spec)
    if (Path(output) / "manifest.json").exists():
        raise ValueError("assistant corrections require a new output directory")
    payload = json.loads(Path(corrections).read_text())
    edited = apply_corrections(originals, rows, payload, source_spec)
    manifest = write_drafts(
        output,
        originals,
        [edited[r["id"]] for r in originals],
        source_spec,
        base["generator"],
        base["glossaryTitleMatches"],
        provenance={
            "method": "pinned neural drafts plus source-bound assistant editorial corrections; not human reviewed",
            "baseManifestSha256": digest(base_path),
            "basePreparation": base["preparation"],
            "correctionsSha256": digest(corrections),
            "correctionsScriptSha256": digest(Path(__file__)),
            "assistantCorrectedRecords": len(payload["corrections"]),
            "humanReviewed": 0,
        },
    )
    print(
        json.dumps(
            {
                "records": manifest["records"],
                "persianScriptTitles": manifest["persianScriptTitles"],
                "assistantCorrectedRecords": len(payload["corrections"]),
                "humanReviewed": 0,
            }
        ),
        flush=True,
    )
    return manifest


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", required=True)
    parser.add_argument("--drafts", required=True)
    parser.add_argument(
        "--corrections", default=str(CONFIG / "esco-fa-corrections.json")
    )
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    correct(args.archive, args.drafts, args.corrections, args.output)
