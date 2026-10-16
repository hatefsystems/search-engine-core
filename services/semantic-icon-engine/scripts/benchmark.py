#!/usr/bin/env python3
"""Frozen-label Hit@k benchmark. This is not an independent human quality audit."""
import argparse
import collections
import hashlib
import json
import math
from pathlib import Path
import resource
import statistics
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from app.catalog import Catalog
from app.ingest import atomic_json
from app.matching import Matcher


def load_cases(path):
    data = json.loads(Path(path).read_text())
    if not data or len({row["id"] for row in data}) != len(data):
        raise ValueError("benchmark needs nonempty cases with unique IDs")
    for row in data:
        if not row.get("text") or not row.get("domain") or not row.get("relevant"):
            raise ValueError("missing benchmark text, domain or relevance labels")
    return data


def benchmark(data, queries, models=None, vectors=None, route="search"):
    start = time.perf_counter()
    cat = Catalog(data)
    model = index = None
    if bool(models) != bool(vectors):
        raise ValueError("models and vectors must be supplied together")
    if models:
        from app.embeddings import E5, Vectors

        model = E5(models)
        index = Vectors(vectors, cat, model)
        if index.count != cat.count:
            raise ValueError("benchmark requires a complete vector index")
    matcher = Matcher(cat, model, index)
    cases = load_cases(queries)
    # Validate availability against the real catalog, without changing any label.
    with cat.db() as db:
        available = {row[0] for row in db.execute("SELECT id FROM icons")}
    rows = []
    for case in cases:
        before = time.perf_counter()
        response = getattr(matcher, route)(case["text"], limit=3)
        elapsed = (time.perf_counter() - before) * 1000
        items = response["items" if route == "search" else "suggestions"]
        ids = [item["id"] for item in items]
        relevant = set(case["relevant"])
        rows.append(
            {
                **case,
                "top3": ids,
                "labels": [item["labelFa"] or item["labelEn"] for item in items],
                "engine": response["engine"],
                "latencyMs": round(elapsed, 3),
                "hitAt1": bool(set(ids[:1]) & relevant),
                "hitAt3": bool(set(ids[:3]) & relevant),
                "empty": not ids,
                "catalogCovered": bool(relevant & available),
                "missingRelevant": sorted(relevant - available),
            }
        )
        if len(rows) % 50 == 0:
            print(
                json.dumps(
                    {"processed": len(rows), "total": len(cases)}, ensure_ascii=False
                ),
                flush=True,
            )
    by_domain = collections.defaultdict(list)
    for row in rows:
        by_domain[row["domain"]].append(row)

    def summary(group):
        passed = sum(row["hitAt3"] for row in group)
        return {
            "queries": len(group),
            "passedAt3": passed,
            "failedAt3": len(group) - passed,
            "hitAt1": statistics.mean(row["hitAt1"] for row in group),
            "hitAt3": passed / len(group),
            "emptyResults": sum(row["empty"] for row in group),
            "catalogCovered": sum(row["catalogCovered"] for row in group),
        }

    latency = sorted(row["latencyMs"] for row in rows)
    return {
        "schema": 1,
        "dataset": str(Path(queries).name),
        "datasetSha256": hashlib.sha256(Path(queries).read_bytes()).hexdigest(),
        "labeling": "Agent-curated frozen expected icon IDs; not an independent human audit or random production sample.",
        "scope": "Direct ranking function, unfiltered, no HTTP rate limits/timeouts; semantic-only retrieval threshold remains active.",
        "route": route,
        "requestedEngine": "hybrid" if model else "lexical",
        "catalogVersion": cat.manifest["version"],
        "catalogIcons": cat.count,
        "embeddingVersion": model.version if model else None,
        "targetHitAt3": 0.9,
        "targetMet": sum(row["hitAt3"] for row in rows) >= math.ceil(0.9 * len(rows)),
        **summary(rows),
        "domains": {domain: summary(group) for domain, group in by_domain.items()},
        "meanMs": statistics.mean(latency),
        "p95Ms": latency[math.ceil(0.95 * len(rows)) - 1],
        "wallSeconds": time.perf_counter() - start,
        "peakRssMiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
        "engineCounts": dict(collections.Counter(row["engine"] for row in rows)),
        "results": rows,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--data", required=True)
    parser.add_argument(
        "--queries",
        default=str(
            Path(__file__).resolve().parents[1] / "tests/evaluation-fa-500.json"
        ),
    )
    parser.add_argument("--models")
    parser.add_argument("--vectors")
    parser.add_argument("--route", choices=["search", "suggest"], default="search")
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    result = benchmark(args.data, args.queries, args.models, args.vectors, args.route)
    Path(args.output).parent.mkdir(parents=True, exist_ok=True)
    atomic_json(args.output, result)
    print(
        json.dumps(
            {k: v for k, v in result.items() if k not in ("results", "domains")},
            ensure_ascii=False,
            indent=2,
        )
    )
