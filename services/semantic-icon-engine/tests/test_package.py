import json
from pathlib import Path
import shutil

import numpy as np
import pytest

from app.embeddings import build
from app.ingest import digest
from app.knowledge import KnowledgeBank, ConceptDocuments
from scripts import package as release


@pytest.fixture
def artifacts(tmp_path, catalog, monkeypatch, knowledge_fixture):
    root = tmp_path / "artifacts"
    root.mkdir()
    shutil.copytree(catalog.root, root / "data")
    models = root / "models"
    models.mkdir()
    for name in ("model.onnx", "tokenizer.json", "README.md", "manifest.json"):
        (models / name).write_text("fixture")

    class Model:
        version = "release-fixture"
        spec = {
            "checksums": {
                name: digest(models / name)
                for name in ("model.onnx", "tokenizer.json", "README.md")
            }
        }

        def __init__(self, *args):
            pass

        def encode(self, texts):
            return np.full((len(texts), 384), 1 / np.sqrt(384), dtype=np.float32)

    monkeypatch.setattr(release, "E5", Model)
    build(root / "vectors", catalog, Model())
    knowledge_fixture(root / "knowledge", None, catalog)
    bank = KnowledgeBank(root / "knowledge")
    build(root / "knowledge-vectors", ConceptDocuments(bank), Model())
    (root / "upstream").mkdir()
    (root / "upstream/private-extra").write_text("do not package source clones")
    return root


def test_package_complete_and_excludes_administrative_files(tmp_path, artifacts):
    target = tmp_path / "release"
    manifest = release.package(artifacts, target)
    assert manifest["approvedIcons"] == manifest["indexedIcons"] == 12
    assert manifest["concepts"] == manifest["indexedConcepts"] == 5
    assert not (target / "upstream").exists()
    assert not (target / "vectors/checkpoint.sqlite").exists()
    for name, expected in manifest["files"].items():
        assert digest(target / name) == expected
        assert (target / name).stat().st_mode & 0o004
    assert (target / "models/README.md").is_file()
    with pytest.raises(ValueError, match="new output"):
        release.package(artifacts, target)


@pytest.mark.parametrize("corruption", ["partial", "wrong_id", "nonfinite", "symlink"])
def test_package_rejects_corrupt_or_partial_release(tmp_path, artifacts, corruption):
    vectors = artifacts / "vectors"
    if corruption == "partial":
        p = vectors / "vectors.json"
        spec = json.loads(p.read_text())
        spec["count"] = 11
        p.write_text(json.dumps(spec))
    elif corruption == "wrong_id":
        p = vectors / "shard-000000.json"
        ids = json.loads(p.read_text())
        ids[0] = "lucide:not-in-catalog"
        p.write_text(json.dumps(ids))
    elif corruption == "nonfinite":
        p = vectors / "shard-000000.npy"
        matrix = np.load(p)
        matrix[0, 0] = np.nan
        np.save(p, matrix)
    else:
        p = artifacts / "models/README.md"
        original = tmp_path / "model-card"
        p.rename(original)
        p.symlink_to(original)
    with pytest.raises(ValueError):
        release.package(artifacts, tmp_path / "release")
    assert not (tmp_path / "release").exists()


def test_package_rejects_incomplete_concept_index(tmp_path, artifacts):
    path = artifacts / "knowledge-vectors/vectors.json"
    spec = json.loads(path.read_text())
    spec["count"] -= 1
    path.write_text(json.dumps(spec))
    with pytest.raises(ValueError, match="complete concept"):
        release.package(artifacts, tmp_path / "release")
    assert not (tmp_path / "release").exists()
