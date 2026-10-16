import hashlib
import json
from pathlib import Path

import pytest
from app.svg import valid_id
from app.text import normalize
from scripts.benchmark import benchmark, load_cases


def test_frozen_500_query_dataset():
    path = Path(__file__).with_name("evaluation-fa-500.json")
    assert (
        hashlib.sha256(path.read_bytes()).hexdigest()
        == "9a47d346814a068f492fe4a9bf6655e2d0c43fc68c44c8094229814ca9b2bb6d"
    )
    rows = load_cases(path)
    assert len(rows) == len({normalize(row["text"]) for row in rows}) == 500
    domains = {row["domain"] for row in rows}
    assert len(domains) == 25
    assert all(sum(row["domain"] == domain for row in rows) == 20 for domain in domains)
    assert all(
        any("\u0600" <= char <= "\u06ff" for char in row["text"]) for row in rows
    )
    assert all(valid_id(id_) for row in rows for id_ in row["relevant"])


def test_missing_artwork_and_empty_results_stay_in_denominator(catalog, tmp_path):
    path = tmp_path / "cases.json"
    path.write_text(
        json.dumps(
            [
                {
                    "id": "one",
                    "domain": "known",
                    "text": "cloud",
                    "relevant": ["lucide:cloud"],
                },
                {
                    "id": "two",
                    "domain": "unknown",
                    "text": "zzxxqq",
                    "relevant": ["lucide:not-present"],
                },
            ]
        )
    )
    result = benchmark(catalog.root, path)
    assert result["queries"] == 2
    assert result["passedAt3"] == result["failedAt3"] == 1
    assert result["hitAt3"] == 0.5
    assert result["emptyResults"] == 1
    assert result["catalogCovered"] == 1
    assert result["targetMet"] is False
    assert result["results"][1]["missingRelevant"] == ["lucide:not-present"]
    assert result["domains"]["unknown"]["hitAt3"] == 0
    assert result["engineCounts"] == {"lexical": 2}


def test_benchmark_rejects_ambiguous_labels_and_partial_model_arguments(
    catalog, tmp_path
):
    path = tmp_path / "cases.json"
    path.write_text('[{"id":"one","text":"cloud"}]')
    with pytest.raises(ValueError, match="missing benchmark"):
        load_cases(path)
    with pytest.raises(ValueError, match="supplied together"):
        benchmark(catalog.root, path, models="missing")
