#!/usr/bin/env python3
"""Validate a complete artifact set and stage only runtime files for an image."""

import argparse
import json
from pathlib import Path
import shutil
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import numpy as np
from app.catalog import Catalog
from app.embeddings import E5, Vectors
from app.ingest import atomic_json, digest
from app.text import CONFIG


def package(artifacts, output):
    root, destination = Path(artifacts), Path(output)
    if destination.exists():
        raise ValueError("package requires a new output directory")
    catalog = Catalog(root / "data")
    model = E5(root / "models", 1)
    vectors = Vectors(root / "vectors", catalog, model)
    with catalog.db() as db:
        expected = {r[0] for r in db.execute("SELECT id FROM icons")}
    if not expected or vectors.count != len(expected):
        raise ValueError("release requires a complete vector index")
    files = ["data/current.json", "data/" + catalog.path.name]
    files += ["models/manifest.json"] + [
        "models/" + name for name in model.spec["checksums"]
    ]
    files += ["vectors/vectors.json"]
    seen = set()
    for shard in vectors.shards:
        ids = json.loads((root / "vectors" / shard["ids"]).read_text())
        matrix = np.load(
            root / "vectors" / shard["vectors"], mmap_mode="r", allow_pickle=False
        )
        if (
            matrix.shape != (len(ids), 384)
            or matrix.dtype != np.float32
            or not np.isfinite(matrix).all()
            or not np.allclose(np.linalg.norm(matrix, axis=1), 1, atol=1e-5)
            or len(set(ids)) != len(ids)
            or seen.intersection(ids)
        ):
            raise ValueError("invalid release vector shard")
        seen.update(ids)
        for key in ("vectors", "ids", "texts"):
            if shard.get(key):
                if Path(shard[key]).name != shard[key]:
                    raise ValueError("unsafe release shard path")
                files.append("vectors/" + shard[key])
    if seen != expected:
        raise ValueError("release vector IDs do not match the catalog")
    checks = {}
    # Reject symlinks even when their target has an otherwise valid checksum.
    for name in files:
        source = root / name
        if source.is_symlink() or source.parent.is_symlink() or not source.is_file():
            raise ValueError("unsafe or missing release file: " + name)
        checks[name] = digest(source)
    destination.mkdir(parents=True)
    try:
        for name in files:
            target = destination / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(root / name, target)
            target.chmod(0o644)
        manifest = {
            "schema": 1,
            "catalogVersion": catalog.manifest["version"],
            "embeddingVersion": model.version,
            "approvedIcons": catalog.count,
            "indexedIcons": vectors.count,
            "sourceLockSha256": digest(CONFIG / "sources.lock.json"),
            "files": checks,
        }
        atomic_json(destination / "release.json", manifest)
        return manifest
    except BaseException:
        shutil.rmtree(destination)
        raise


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--artifacts", default="/artifacts")
    parser.add_argument("--output", default="/release/package")
    args = parser.parse_args()
    result = package(args.artifacts, args.output)
    print(json.dumps({k: v for k, v in result.items() if k != "files"}, indent=2))
