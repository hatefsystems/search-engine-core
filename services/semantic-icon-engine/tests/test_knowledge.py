import json
import numpy as np
import pytest

from app import knowledge
from app.ingest import atomic_json
from app.matching import Matcher
from scripts.knowledge import export_review, import_review


@pytest.fixture
def bank(tmp_path, catalog, knowledge_fixture):
    root = tmp_path / "knowledge"
    report = knowledge_fixture(root, None, catalog)
    assert report["escoSkills"] == report["escoOccupations"] == 1
    assert report["escoGroups"] == 2 and report["relationships"] == 3
    return knowledge.KnowledgeBank(root)


def test_import_deduplicates_and_preserves_stable_ids(bank, knowledge_fixture):
    id_ = knowledge.concept_id(knowledge_fixture.skill)
    item = bank.get(id_)
    assert item["uri"] == knowledge_fixture.skill
    assert set(item["aliasesEn"]) == {"cloud", "cloud systems", "cloud computing"}
    assert item["translationStatus"] == "untranslated" and not item["labelFa"]
    assert bank.search("cloud engineer")[0]
    assert knowledge.concept_id(knowledge_fixture.skill) == id_


def test_persian_mapping_works_without_model(bank, catalog):
    result = Matcher(catalog, knowledge=bank).suggest("رایانش ابری")
    assert result["suggestions"][0]["id"] == "lucide:cloud"
    assert not result["lowConfidence"]


def test_review_packet_requires_explicit_review(
    bank, catalog, knowledge_fixture, tmp_path
):
    packet = export_review(bank)
    original = json.loads((knowledge_fixture.config / "knowledge-fa.json").read_text())
    with pytest.raises(ValueError, match="no explicitly reviewed"):
        import_review(bank, catalog, packet, original)
    row = packet["concepts"][0]
    row.update(
        labelFa="مهارت ابری",
        aliasesFa=["سامانه ابری"],
        descriptionFa="",
        icons=["lucide:cloud"],
        translationStatus="reviewed",
        reviewer="fixture-reviewer",
    )
    updated = import_review(bank, catalog, packet, original)
    atomic_json(knowledge_fixture.config / "knowledge-fa.json", updated)
    root = tmp_path / "reviewed-bank"
    knowledge_fixture(root, None, catalog)
    rebuilt = knowledge.KnowledgeBank(root)
    assert rebuilt.get(row["id"])["labelFa"] == "مهارت ابری"
    assert row["id"] in rebuilt.direct("سامانه ابری")
    assert (
        rebuilt.icon_candidates("سامانه ابری")["lucide:cloud"]["evidence"] == "curated"
    )


def test_rejects_unverified_or_unknown_overlay(bank, catalog):
    id_ = next(r["id"] for r in export_review(bank)["concepts"])
    row = dict(
        id=id_, labelFa="ابر", translationStatus="proposed", reviewer="", icons=[]
    )
    with pytest.raises(ValueError, match="reviewer"):
        knowledge.validate_overrides([row], {id_: True}, catalog)
    row.update(translationStatus="reviewed", reviewer="fixture")
    row["icons"] = ["lucide:missing"]
    with pytest.raises(ValueError, match="unknown"):
        knowledge.validate_overrides([row], {id_: True}, catalog)


def test_archive_and_database_checksums(bank, knowledge_fixture):
    spec = json.loads((knowledge_fixture.config / "knowledge.lock.json").read_text())[
        "esco"
    ]
    with pytest.raises(ValueError, match="checksum"):
        knowledge.read_esco(knowledge_fixture.archive, {**spec, "sha256": "bad"})
    bank.path.write_bytes(bank.path.read_bytes() + b"tamper")
    with pytest.raises(ValueError, match="integrity"):
        knowledge.KnowledgeBank(bank.root)


def test_semantic_proposals_cannot_authorize_auto_selection(bank, catalog):
    class Model:
        def query(self, text):
            return np.zeros(384)

    class Vectors:
        def nearest(self, vector, limit):
            return [("hatef:concept:cloud", 0.99)]

    bank.model, bank.vectors = Model(), Vectors()
    result = Matcher(catalog, model=bank.model, knowledge=bank).suggest(
        "عبارت ناشناخته"
    )
    assert result["suggestions"][0]["id"] == "lucide:cloud"
    assert result["suggestions"][0]["evidence"] == "semantic-concept"
    assert result["lowConfidence"]
    fallback = Matcher(catalog, knowledge=bank).suggest("عبارت ناشناخته")
    assert fallback["suggestions"][0]["evidence"] == "fallback"


def test_ambiguous_curated_concepts_require_manual_selection(catalog):
    class AmbiguousBank:
        def icon_candidates(self, text, model):
            return {
                "lucide:cloud": {
                    "score": 2.7,
                    "evidence": "curated",
                    "conceptId": "cloud",
                },
                "lucide:briefcase": {
                    "score": 2.7,
                    "evidence": "curated",
                    "conceptId": "office",
                },
            }

    assert Matcher(catalog, knowledge=AmbiguousBank()).suggest(
        "عبارت چندمعنا", limit=1
    )["lowConfidence"]
