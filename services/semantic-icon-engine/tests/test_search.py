"""User-facing multilingual search, relevance, catalog and inference regressions."""

import json
import time

import pytest
from fastapi.testclient import TestClient
from app.api import create_app
from app.catalog import Catalog, connect
from app.ingest import atomic_json, digest
from app.matching import Matcher
from app.svg import sanitize
from app.text import enrich, expand, category_label, document_text


def test_svg_scale_tag_does_not_translate_resize_as_legal_scales():
    categories, fa = enrich("expand", ["scale", "resize"])
    assert fa != "ترازو" and "law" not in categories
    assert "ترازو" not in document_text("expand", "expand", fa, ["scale"])


def test_strong_semantics_can_outrank_a_weak_lexical_candidate(catalog, monkeypatch):
    monkeypatch.setattr(
        catalog, "search", lambda *args, **kwargs: [catalog.get("lucide:briefcase")]
    )

    class Model:
        def query(self, text):
            return text

    class Vectors:
        count = 1

        def nearest(self, vector, limit):
            return [("lucide:cloud", 0.96)]

    matcher = Matcher(catalog, Model(), Vectors())
    assert (
        matcher.search("عبارتی با تطبیق متنی ضعیف")["items"][0]["id"] == "lucide:cloud"
    )
    assert matcher.suggest("عبارتی با تطبیق متنی ضعیف")["lowConfidence"]


def test_lucide_mirror_uses_canonical_id_and_preserves_explicit_source(
    catalog, monkeypatch
):
    original = catalog.get
    mirror = {
        **original("lucide:cloud"),
        "id": "iconify:lucide:cloud",
        "source": "iconify",
    }
    monkeypatch.setattr(
        catalog, "get", lambda id_: mirror if id_ == mirror["id"] else original(id_)
    )

    class Model:
        def query(self, text):
            return text

    class Vectors:
        def nearest(self, vector, limit):
            return [(mirror["id"], 0.99)]

    matcher = Matcher(catalog, Model(), Vectors())
    assert matcher.search("عبارت معنایی")["items"][0]["id"] == "lucide:cloud"
    assert (
        matcher.search("عبارت معنایی", source="iconify")["items"][0]["id"]
        == mirror["id"]
    )


@pytest.fixture
def multilingual_catalog(catalog):
    db = connect(catalog.path)
    for index, (icon_id, name, brand) in enumerate(
        [
            ("lucide:heart", "heart", False),
            ("lucide:heart-pulse", "heart-pulse", False),
            ("simple-icons:terraform", "terraform", True),
            ("iconify:devicon:terraform", "terraform", True),
            ("iconify:devicon:amazonwebservices", "amazonwebservices", True),
        ]
    ):
        cats, fa = enrich(name, [], brand=brand)
        svg, hash_ = sanitize(
            f'<svg viewBox="0 0 24 24"><path fill="#5c4ee5" d="M{index+15} 1h3"/></svg>'
        )
        metadata = {
            "labelEn": name.replace("-", " "),
            "labelFa": fa,
            "categories": cats,
            "keywords": [],
            "brand": brand,
        }
        text = expand(name.replace("-", " ") + " " + fa)
        db.execute("INSERT INTO assets VALUES(?,?)", (hash_, svg))
        db.execute(
            "INSERT INTO icons VALUES(?,?,?,?,?,?,?,?,?)",
            (
                icon_id,
                icon_id.split(":")[0],
                name,
                cats[0],
                "brand" if brand else "outline",
                int(brand),
                hash_,
                json.dumps(metadata),
                text,
            ),
        )
        db.execute("INSERT INTO search VALUES(?,?)", (icon_id, text))
    db.commit()
    db.close()
    atomic_json(
        catalog.root / "current.json",
        {**catalog.manifest, "sha256": digest(catalog.path)},
    )
    return Catalog(catalog.root)


@pytest.mark.parametrize(
    "query", ["heart", "قلب", "دل", "عشق", "علاقه", "محبت", "دوست داشتن"]
)
def test_manual_heart_exact_before_related(multilingual_catalog, query):
    items = Matcher(multilingual_catalog).search(query)["items"]
    ids = [item["id"] for item in items]
    assert ids[0] == "lucide:heart"
    assert "lucide:heart-pulse" in ids
    if query in ("heart", "قلب", "دل"):
        assert (
            Matcher(multilingual_catalog).suggest(query)["suggestions"][0]["id"]
            == "lucide:heart"
        )


@pytest.mark.parametrize(
    "query,target",
    [
        ("Terraform", "terraform"),
        ("ترافورم", "terraform"),
        ("ترافرم", "terraform"),
        ("AWS", "amazonwebservices"),
        ("آمازون وب سرویسز", "amazonwebservices"),
    ],
)
def test_brand_names_and_persian_aliases(multilingual_catalog, query, target):
    matcher = Matcher(multilingual_catalog)
    brands = [item for item in matcher.search(query)["items"] if item["brand"]]
    assert brands and all(target in item["id"] for item in brands)
    assert any(target in item["id"] for item in matcher.suggest(query)["suggestions"])


def test_both_brand_collections_and_secondary_categories(multilingual_catalog):
    matcher = Matcher(multilingual_catalog)
    assert {i["source"] for i in matcher.search("ترافورم")["items"]} == {
        "simple-icons",
        "iconify",
    }
    assert (
        matcher.search("ترافورم", source="simple-icons")["items"][0]["id"]
        == "simple-icons:terraform"
    )
    items = matcher.search("ترافورم", category="technology")["items"]
    assert len(items) == 2
    categories = multilingual_catalog.categories()
    assert all(c["labelFa"] and not c["labelFa"].isascii() for c in categories)
    assert category_label("unknown") == "سایر"
    assert next(c for c in categories if c["category"] == "technology")["count"] >= 3


def test_hybrid_search_filters_threshold_and_stable_pagination(multilingual_catalog):
    class Model:
        def query(self, text):
            return text

    class Vectors:
        def nearest(self, vector, limit):
            return [
                ("lucide:heart-pulse", 0.95),
                ("simple-icons:terraform", 0.99),
                ("lucide:scale", 0.2),
            ]

    matcher = Matcher(multilingual_catalog, Model(), Vectors())
    response = matcher.search("مراقبت ویژه")
    assert response["engine"] == "hybrid"
    assert [item["id"] for item in response["items"]] == ["lucide:heart-pulse"]
    assert matcher.search("مراقبت ویژه", category="law")["items"] == []
    assert matcher.search("مراقبت ویژه", source="simple-icons")["items"] == []
    first = matcher.search("قلب", limit=1)
    second = matcher.search("قلب", limit=1, offset=first["nextOffset"])
    assert first["items"][0]["id"] == "lucide:heart"
    assert first["items"][0]["id"] != second["items"][0]["id"]
    assert matcher.search("قلب", limit=1) == first


def test_manual_api_falls_back_on_model_timeout_and_busy(
    multilingual_catalog, monkeypatch
):
    monkeypatch.setenv("ICON_TIMEOUT_SECONDS", ".01")
    app = create_app(multilingual_catalog.root)
    with TestClient(app) as client:

        class Model:
            def query(self, text):
                time.sleep(0.15)
                return text

        class Vectors:
            def nearest(self, *args):
                return []

        app.state.matcher.model, app.state.matcher.vectors = Model(), Vectors()
        response = client.get("/v1/icons/search", params={"q": "قلب"}).json()
        assert response["reason"] == "semantic_timeout"
        assert response["items"][0]["id"] == "lucide:heart"
        second = client.get("/v1/icons/search", params={"q": "قلب"}).json()
        assert second["reason"] == "inference_busy"
        assert second["items"][0]["id"] == "lucide:heart"
        time.sleep(0.2)
        app.state.matcher.model = None
        assert (
            client.get("/v1/icons/search", params={"q": "قلب"}).json()["engine"]
            == "lexical"
        )
