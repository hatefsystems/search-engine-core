#!/usr/bin/env python3
"""Explicit one-shot preparation. The API never invokes this script."""

import argparse
import fcntl
import json
import os
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.manage import prepare
from app.ingest import ingest, atomic_json
from app.catalog import Catalog
from app.knowledge import build_bank, KnowledgeBank, ConceptDocuments, prepare_proposals


def bootstrap(artifacts, upstream, skip_index=False, batch=8):
    if not 1 <= batch <= 32:
        raise ValueError("batch must be 1..32")
    # Artifacts must remain readable by the non-root runtime on a read-only mount.
    os.umask(0o022)
    root = Path(artifacts)
    root.mkdir(parents=True, exist_ok=True)
    with (root / ".setup.lock").open("w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        print("Preparing pinned sources and model", flush=True)
        prepare(upstream, root / "models")
        print("Importing approved icons", flush=True)
        report = ingest(root / "data", upstream)
        atomic_json(root / "import-report.json", report)
        catalog = Catalog(root / "data")
        result = {
            "catalogVersion": report["version"],
            "approvedIcons": catalog.count,
            "indexedIcons": None,
            "indexSkipped": skip_index,
        }
        knowledge_report = build_bank(
            root / "knowledge", Path(upstream) / "esco/dataset.zip", catalog
        )
        result["knowledgeConcepts"] = (
            knowledge_report["escoSkills"]
            + knowledge_report["escoOccupations"]
            + knowledge_report["editorialPersianConcepts"]
            + knowledge_report["escoGroups"]
        )
        if not skip_index:
            from app.embeddings import E5, Vectors, build

            print("Building or resuming vectors", flush=True)
            model = E5(root / "models", int(os.getenv("ICON_THREADS", "1")))
            spec = build(root / "vectors", catalog, model, batch)
            if spec["count"] != catalog.count:
                raise ValueError("index is incomplete")
            result["indexedIcons"] = spec["count"]
            bank = KnowledgeBank(root / "knowledge")
            docs = ConceptDocuments(bank)
            build(root / "knowledge-vectors", docs, model, batch)
            result["knowledgeMappings"] = prepare_proposals(
                bank,
                catalog,
                model,
                Vectors(root / "vectors", catalog, model),
                Vectors(root / "knowledge-vectors", docs, model),
            )
        print(json.dumps(result, ensure_ascii=False, indent=2), flush=True)
        return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifacts", default="/artifacts")
    parser.add_argument("--upstream", default="/upstream")
    parser.add_argument("--skip-index", action="store_true")
    parser.add_argument("--batch", type=int, default=8)
    args = parser.parse_args()
    bootstrap(args.artifacts, args.upstream, args.skip_index, args.batch)
