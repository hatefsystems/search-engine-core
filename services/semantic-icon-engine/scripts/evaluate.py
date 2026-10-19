#!/usr/bin/env python3
"""Measure a labeled development set; scores are not production accuracy claims."""
import argparse
import json
import math
from pathlib import Path
import resource
import statistics
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from app.catalog import Catalog
from app.matching import Matcher
from app.ingest import atomic_json


def evaluate(data, queries, models=None, vectors=None):
    start = time.perf_counter()
    cpu = time.process_time()
    cat = Catalog(data)
    model = index = None
    if models:
        from app.embeddings import E5, Vectors

        model = E5(models)
        index = Vectors(vectors, cat, model)
    matcher = Matcher(cat, model, index)
    cases = json.loads(Path(queries).read_text())
    rows = []
    startup = time.perf_counter() - start
    matcher.suggest("cloud infrastructure")
    for case in cases:
        begin = time.perf_counter()
        result = matcher.suggest(case["text"], limit=5)
        elapsed = (time.perf_counter() - begin) * 1000
        ids = [i["id"] for i in result["suggestions"]]
        relevant = set(case["relevant"])
        rows.append(
            {
                **case,
                "ids": ids,
                "latencyMs": elapsed,
                "recallAt3": len(set(ids[:3]) & relevant) / len(relevant),
                "recallAt5": len(set(ids[:5]) & relevant) / len(relevant),
                "hitAt3": bool(set(ids[:3]) & relevant),
                "hitAt5": bool(set(ids[:5]) & relevant),
                "lowConfidence": result["lowConfidence"],
            }
        )
    latency = sorted(r["latencyMs"] for r in rows)
    wall = time.perf_counter() - start
    cpu = time.process_time() - cpu
    return {
        "dataset": "development, dictionary-aware labels; not held-out",
        "queries": len(rows),
        "engine": "hybrid" if model else "lexical",
        "catalogVersion": cat.manifest["version"],
        "catalogIcons": cat.count,
        "embeddingVersion": model.version if model else None,
        "startupSeconds": startup,
        "meanMs": statistics.mean(latency),
        "p95Ms": latency[max(0, math.ceil(0.95 * len(latency)) - 1)],
        "cpuSeconds": cpu,
        "wallSeconds": wall,
        "averageCpuCores": cpu / wall,
        "peakRssMiB": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024,
        **{
            k: statistics.mean(r[k] for r in rows)
            for k in ["recallAt3", "recallAt5", "hitAt3", "hitAt5"]
        },
        "results": rows,
    }


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("--data", required=True)
    p.add_argument(
        "--queries",
        default=str(Path(__file__).resolve().parents[1] / "tests/evaluation.json"),
    )
    p.add_argument("--models")
    p.add_argument("--vectors")
    p.add_argument("--output", required=True)
    a = p.parse_args()
    result = evaluate(a.data, a.queries, a.models, a.vectors)
    atomic_json(a.output, result)
    print(json.dumps({k: v for k, v in result.items() if k != "results"}, indent=2))
