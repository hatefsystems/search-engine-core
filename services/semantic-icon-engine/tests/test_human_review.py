import copy
import json
from pathlib import Path
from types import SimpleNamespace

import pytest

from app.ingest import atomic_json, digest
from app.knowledge import read_esco
from scripts import human_review as review
from scripts import translate_esco as translation
from scripts.correct_fa_drafts import apply_corrections
from scripts.benchmark import benchmark, load_cases


@pytest.fixture
def study(catalog):
    # Synthetic fixtures exercise the protocol, not real acceptance evidence.
    packet = review.new_study("a" * 40, catalog.manifest["version"])
    for number, row in enumerate(packet["cases"]):
        row.update(
            text=f"عبارت ساختگی آزمون واحد شماره {number}",
            queryAuthor="fixture-query-author",
            humanAuthored=True,
            queryAuthorIndependentFromDevelopment=True,
        )
    return review.seal_queries(packet)


def labeled(study, name, ids=None):
    packet = review.annotation_packet(study, name)
    packet.update(
        isHuman=True, independentFromDevelopment=True, blindToEngineResults=True
    )
    for row in packet["cases"]:
        row.update(
            relevant=ids if ids is not None else ["lucide:cloud"],
            noSuitableIcon=ids == [],
            reviewStatus="reviewed",
        )
    return packet


def test_empty_human_collection_cannot_be_sealed(catalog):
    packet = review.new_study("a" * 40, catalog.manifest["version"])
    assert len(packet["cases"]) == 500
    assert not any(r["text"] or r["humanAuthored"] for r in packet["cases"])
    with pytest.raises(ValueError, match="Persian phrases"):
        review.seal_queries(packet)


def test_runtime_overlap_reference_matches_frozen_source_without_labels():
    reference = review.development_reference()
    original = json.loads(review.DEVELOPMENT.read_text())
    assert reference["fileSha256"] == digest(review.DEVELOPMENT)
    assert reference["normalizedQuerySha256"] == sorted(
        review.phrase_hash(row["text"]) for row in original
    )
    assert not any(
        key in reference for key in ("relevant", "cases", "predictions", "texts")
    )
    assert reference["domains"] == sorted({row["domain"] for row in original})
    candidates = review.DEVELOPMENT.with_name("evaluation-fa-holdout-review.json")
    assert reference["assistantCandidateFileSha256"] == digest(candidates)
    assert reference["candidateQuerySha256"] == sorted(
        review.phrase_hash(row["text"]) for row in json.loads(candidates.read_text())
    )


def test_reject_development_overlap_and_changed_sealed_queries(study, catalog):
    packet = copy.deepcopy(study)
    packet["state"] = "collecting"
    packet["cases"][0]["text"] = json.loads(review.DEVELOPMENT.read_text())[0]["text"]
    with pytest.raises(ValueError, match="unseen Persian"):
        review.seal_queries(packet)
    a, b = labeled(study, "fixture-annotator-a"), labeled(study, "fixture-annotator-b")
    a["cases"][0]["text"] = "دستکاری عبارت"
    with pytest.raises(ValueError, match="changed a sealed"):
        review.freeze(study, a, b, catalog)


def test_pending_same_person_author_and_unblinded_reviews_are_rejected(study, catalog):
    a, b = labeled(study, "fixture-annotator-a"), labeled(study, "fixture-annotator-b")
    for change, message in [
        ({"reviewer": "fixture-annotator-a"}, "distinct"),
        ({"reviewer": "fixture-query-author"}, "authors"),
        ({"isHuman": False}, "attestations"),
        ({"blindToEngineResults": False}, "attestations"),
        ({"independentFromDevelopment": False}, "attestations"),
    ]:
        with pytest.raises(ValueError, match=message):
            review.freeze(study, a, {**b, **change}, catalog)
    b["cases"][0]["reviewStatus"] = "pending"
    with pytest.raises(ValueError, match="reviewed"):
        review.freeze(study, a, b, catalog)


def test_disagreements_need_third_person_and_complete_catalog_is_unranked(
    study, catalog
):
    a, b = labeled(study, "fixture-annotator-a"), labeled(
        study, "fixture-annotator-b", ["lucide:wrench"]
    )
    with pytest.raises(ValueError, match="500 annotation disagreements"):
        review.freeze(study, a, b, catalog)
    rows, report = review.freeze(
        study, a, b, catalog, labeled(study, "fixture-adjudicator", [])
    )
    assert len(rows) == 500 and all(
        r["noSuitableIcon"] and not r["relevant"] for r in rows
    )
    assert report["disagreements"] == 500 and report["identityAuthenticated"] is False
    review.validate_independent_cases(rows, catalog.manifest["version"], "a" * 40)
    exported = review.export_catalog(catalog)
    assert len(exported["icons"]) == catalog.count
    assert exported["sources"][0]["licenseText"] == "ISC License fixture"
    assert [r["id"] for r in exported["icons"]] == sorted(
        r["id"] for r in exported["icons"]
    )
    assert all("score" not in r and "rank" not in r for r in exported["icons"])


def test_independent_run_requires_frozen_evidence_versions_and_counts_all_misses(
    study, catalog, tmp_path
):
    rows, report = review.freeze(
        study,
        labeled(study, "fixture-annotator-a", []),
        labeled(study, "fixture-annotator-b", []),
        catalog,
    )
    path = tmp_path / "independent-fixture.json"
    atomic_json(path, rows)
    manifest = tmp_path / "freeze-report.json"
    atomic_json(manifest, report)
    result = benchmark(
        catalog.root,
        path,
        require_independent=True,
        engine_revision="a" * 40,
        acceptance_manifest=manifest,
    )
    assert result["queries"] == result["failedAt3"] == 500
    assert (
        result["independentAcceptanceEvaluated"] and result["acceptancePassed"] is False
    )
    with pytest.raises(ValueError, match="engine revision mismatch"):
        benchmark(
            catalog.root, path, require_independent=True, engine_revision="b" * 40
        )
    rows[0]["relevant"] = ["lucide:cloud"]
    rows[0]["noSuitableIcon"] = False
    atomic_json(path, rows)
    with pytest.raises(ValueError, match="manifest or labels mismatch"):
        benchmark(
            catalog.root,
            path,
            require_independent=True,
            engine_revision="a" * 40,
            acceptance_manifest=manifest,
        )
    rows[0]["text"] = "عبارت دستکاری شده"
    atomic_json(path, rows)
    with pytest.raises(ValueError, match="digest mismatch"):
        load_cases(path, require_independent=True)


def test_translator_preserves_symbols_and_separates_structural_warnings():
    assert translation.clean_source("<p>beekeeping</p>", True) == "Beekeeping."
    assert translation.clean_fa("كاربرد يک ابزار.", True) == "کاربرد یک ابزار"
    flags = translation.quality_flags(
        {"labelEn": "AJAX", "descriptionEn": "a technique"},
        {"labelFa": "AJAX", "aliasesFa": [], "descriptionFa": "a technique"},
    )
    assert set(flags) == {
        "title-needs-persian",
        "unchanged-title",
        "description-needs-persian",
    }


def test_decoder_retry_keeps_every_source_fragment_instead_of_truncated_output():
    translator = object.__new__(translation.DraftTranslator)
    translator.source = SimpleNamespace(decode=lambda tokens: " ".join(tokens))
    translator.target = SimpleNamespace(decode=lambda tokens: " ".join(tokens))
    translator.beam, translator.no_repeat = 5, 8

    def decode(batch, **options):
        tokens = batch[0][:-1]
        # Only single-token pieces terminate. Larger chunks simulate truncation.
        return [
            SimpleNamespace(
                hypotheses=[tokens + (["</s>"] if len(tokens) == 1 else [])]
            )
        ]

    translator.translator = SimpleNamespace(translate_batch=decode)
    text, literal = translator.recover(["بخش‌اول", "بخش‌دوم", "بخش‌سوم", "بخش‌چهارم"])
    assert text == "بخش‌اول بخش‌دوم بخش‌سوم بخش‌چهارم"
    assert literal is False
    translator.translator = SimpleNamespace(
        translate_batch=lambda *a, **k: [SimpleNamespace(hypotheses=[["unfinished"]])]
    )
    text, literal = translator.recover(["original-name"])
    assert text == "original-name" and literal is True


def test_only_disagreements_go_to_third_person(study, catalog):
    a, b = labeled(study, "fixture-annotator-a"), labeled(study, "fixture-annotator-b")
    b["cases"][0]["relevant"] = ["lucide:wrench"]
    third = review.adjudication_packet(study, a, b, catalog, "fixture-third-person")
    assert len(third["cases"]) == 1 and third["mode"] == "adjudication"
    assert set(third["cases"][0]["raterChoices"]) == {a["reviewer"], b["reviewer"]}
    third.update(
        isHuman=True, independentFromDevelopment=True, blindToEngineResults=True
    )
    third["cases"][0].update(relevant=["lucide:wrench"], reviewStatus="reviewed")
    rows, report = review.freeze(study, a, b, catalog, third)
    assert len(rows) == 500 and report["disagreements"] == 1
    assert rows[0]["relevant"] == ["lucide:wrench"]


def fixture_drafts(knowledge_fixture, tmp_path):
    spec = json.loads((knowledge_fixture.config / "knowledge.lock.json").read_text())[
        "esco"
    ]
    rows, _ = read_esco(knowledge_fixture.archive, spec)
    drafts = [
        {
            "id": row["id"],
            "sourceSha256": translation.source_hash(row),
            "labelFa": "عنوان پیش‌نویس فارسی",
            "descriptionFa": "توضیح پیش‌نویس فارسی" if row["descriptionEn"] else "",
            "aliasesFa": [
                f"نام جایگزین فارسی {i}" for i, _ in enumerate(row["aliasesEn"])
            ],
            "translationStatus": "machine-draft",
            "reviewer": "",
            "qualityFlags": [],
        }
        for row in rows
    ]
    root = tmp_path / "drafts"
    root.mkdir()
    path = root / "drafts-000.jsonl"
    path.write_text(
        "".join(json.dumps(row, ensure_ascii=False) + "\n" for row in drafts)
    )
    manifest = {
        "schema": 1,
        "status": "machine-draft",
        "humanReviewed": 0,
        "sourceSha256": spec["sha256"],
        "sourceVersion": spec["version"],
        "records": len(rows),
        "shards": [{"file": path.name, "records": len(rows), "sha256": digest(path)}],
    }
    atomic_json(root / "manifest.json", manifest)
    return root, rows, drafts, manifest, spec


def test_draft_integrity_source_coverage_and_false_review_are_rejected(
    knowledge_fixture, tmp_path
):
    root, rows, drafts, manifest, spec = fixture_drafts(knowledge_fixture, tmp_path)
    assert len(translation.load_drafts(root / "manifest.json", rows, spec)[1]) == 4
    with pytest.raises(ValueError, match="all source IDs"):
        translation.load_drafts(root / "manifest.json", rows[:-1], spec)
    changed = copy.deepcopy(rows)
    changed[0]["labelEn"] = "different source"
    with pytest.raises(ValueError, match="source record"):
        translation.load_drafts(root / "manifest.json", changed, spec)
    drafts[0]["translationStatus"] = "reviewed"
    shard = root / manifest["shards"][0]["file"]
    shard.write_text("".join(json.dumps(row) + "\n" for row in drafts))
    with pytest.raises(ValueError, match="checksum"):
        translation.load_drafts(root / "manifest.json", rows, spec)
    manifest["shards"][0]["sha256"] = digest(shard)
    atomic_json(root / "manifest.json", manifest)
    with pytest.raises(ValueError, match="falsely reviewed"):
        translation.load_drafts(root / "manifest.json", rows, spec)


def test_full_checked_in_drafts_are_machine_only():
    root = Path(__file__).resolve().parents[1] / "config"
    manifest, rows = translation.load_drafts(
        root / "esco-fa-drafts/manifest.json",
        source_spec=json.loads((root / "knowledge.lock.json").read_text())["esco"],
    )
    assert len(rows) == manifest["titleCoverage"] == 18237
    assert manifest["descriptionCoverage"] == manifest["nonemptySourceDescriptions"]
    assert manifest["humanReviewed"] == 0
    assert manifest["persianScriptTitles"] == 18237
    assert all(translation.has_persian(row["labelFa"]) for row in rows.values())


def test_translation_import_requires_real_review_attestation_and_immutable_source(
    knowledge_fixture, catalog, tmp_path, monkeypatch
):
    monkeypatch.setattr(review, "CONFIG", knowledge_fixture.config)
    spec = json.loads((review.CONFIG / "knowledge.lock.json").read_text())["esco"]
    source, _ = read_esco(knowledge_fixture.archive, spec)
    original = source[0]
    draft_manifest = review.CONFIG / "esco-fa-drafts/manifest.json"
    draft_manifest.parent.mkdir(exist_ok=True)
    atomic_json(draft_manifest, {"fixture": "snapshot only"})
    row = {
        "id": original["id"],
        "sourceUri": original["uri"],
        "sourceSha256": translation.source_hash(original),
        "labelEn": original["labelEn"],
        "aliasesEn": original["aliasesEn"],
        "descriptionEn": original["descriptionEn"],
        "labelFa": "مدیریت زیرساخت ابری",
        "aliasesFa": ["سامانه‌های ابری"],
        "descriptionFa": "مدیریت و نگهداری زیرساخت‌های ابری.",
        "icons": ["lucide:cloud"],
        "translationStatus": "pending",
        "humanReviewed": False,
        "reviewer": "fixture-reviewer",
    }
    packet = {
        "mode": "translation",
        "sourceSha256": spec["sha256"],
        "draftManifestSha256": digest(draft_manifest),
        "concepts": [row],
    }
    with pytest.raises(ValueError, match="no human-reviewed"):
        review.import_translations(knowledge_fixture.archive, packet, catalog, {})
    row["translationStatus"] = "reviewed"
    with pytest.raises(ValueError, match="personal human review"):
        review.import_translations(knowledge_fixture.archive, packet, catalog, {})
    row["humanReviewed"] = True
    result = review.import_translations(knowledge_fixture.archive, packet, catalog, {})
    assert result["escoOverrides"][0]["labelFa"] == row["labelFa"]
    row["labelEn"] = "changed source title"
    with pytest.raises(ValueError, match="immutable source"):
        review.import_translations(knowledge_fixture.archive, packet, catalog, {})


def test_partition_generation_and_merge_require_exact_source_coverage(
    knowledge_fixture, tmp_path, monkeypatch
):
    monkeypatch.setattr(translation, "CONFIG", knowledge_fixture.config)

    class FixtureTranslator:
        def __init__(self, *args):
            pass

        def prepare(self, *args):
            pass

        def translate(self, value, title=False):
            return "ترجمهٔ آزمایشی " + value if value else ""

    monkeypatch.setattr(translation, "DraftTranslator", FixtureTranslator)
    parts = tmp_path / "parts"
    for index in range(2):
        result = translation.generate(
            knowledge_fixture.archive,
            "unused-fixture",
            "unused-fixture",
            parts / str(index),
            partition_index=index,
            partition_count=2,
        )
        assert result["records"] == 2
    manifest = translation.merge_drafts(
        knowledge_fixture.archive, parts, tmp_path / "merged"
    )
    assert manifest["records"] == 4 and manifest["humanReviewed"] == 0
    (parts / "1/manifest.json").unlink()
    with pytest.raises(ValueError, match="partitions"):
        translation.merge_drafts(
            knowledge_fixture.archive, parts, tmp_path / "missing-part"
        )


def test_assistant_corrections_preserve_source_binding_and_cannot_be_human_approval(
    knowledge_fixture, tmp_path
):
    root, originals, drafts, manifest, spec = fixture_drafts(
        knowledge_fixture, tmp_path
    )
    source = originals[0]
    change = {
        "id": source["id"],
        "labelEn": source["labelEn"],
        "sourceSha256": translation.source_hash(source),
        "labelFa": "زیرساخت ابری",
    }
    corrections = {
        "schema": 1,
        "sourceSha256": spec["sha256"],
        "translationStatus": "machine-draft",
        "humanReviewed": 0,
        "corrections": [change],
    }
    original_drafts = {r["id"]: r for r in drafts}
    result = apply_corrections(originals, original_drafts, corrections, spec)
    assert result[source["id"]]["labelFa"] == "زیرساخت ابری"
    assert all(
        r["translationStatus"] == "machine-draft" and r["reviewer"] == ""
        for r in result.values()
    )
    assert original_drafts[source["id"]]["labelFa"] != result[source["id"]]["labelFa"]
    change["reviewer"] = "fabricated-reviewer"
    with pytest.raises(ValueError, match="unsafe"):
        apply_corrections(originals, original_drafts, corrections, spec)
    change.pop("reviewer")
    change["sourceSha256"] = "wrong"
    with pytest.raises(ValueError, match="source record"):
        apply_corrections(originals, original_drafts, corrections, spec)
