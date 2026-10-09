import json
import os
import time
from pathlib import Path
import numpy as np
import pytest
from fastapi.testclient import TestClient
from app.api import create_app
from app.svg import sanitize, valid_id, iconify_svg
from app.text import normalize
from app.matching import Matcher
from app.embeddings import E5, Vectors, build


@pytest.mark.parametrize(
    "body",
    [
        "<script>alert(1)</script>",
        "<foreignObject/>",
        '<image href="https://example.org/a"/>',
        '<path onclick="alert(1)"/>',
        '<path style="fill:red"/>',
        '<use href="#a"/>',
        '<path fill="url(https://example.org/a)"/>',
        '<path fill="url(#missing)"/>',
        '<g xmlns="https://example.org"/>',
    ],
)
def test_reject_active_svg(body):
    with pytest.raises(ValueError):
        sanitize('<svg viewBox="0 0 24 24">' + body + "</svg>")


def test_svg_limits_and_determinism():
    for svg in [
        '<!DOCTYPE svg [<!ENTITY a "b">]><svg viewBox="0 0 24 24"/>',
        '<svg viewBox="0 0 nan 24"/>',
        '<svg viewBox="0 0 24 24">' + "<g>" * 26 + "</g>" * 26 + "</svg>",
    ]:
        with pytest.raises(ValueError):
            sanitize(svg)
    text = '<svg viewBox="0 0 24 24"><path fill="#ff0000" d="M0 0h1"/><path fill="#00ff00" d="M0 1h2"/></svg>'
    safe, hash_ = sanitize(text)
    assert "#ff0000" in safe and "#00ff00" in safe
    assert sanitize(safe) == (safe, hash_)


def test_alias_rotation_and_cycles():
    pack = {
        "width": 16,
        "height": 24,
        "icons": {"a": {"body": '<path d="M0 0h1"/>'}},
        "aliases": {"b": {"parent": "a", "rotate": 1, "hFlip": True}},
    }
    svg, _ = sanitize(iconify_svg(pack, "b"))
    assert 'viewBox="0 0 24 16"' in svg
    assert "rotate(90)" in svg
    pack["aliases"]["a"] = {"parent": "b"}
    with pytest.raises(ValueError):
        iconify_svg(pack, "b")


@pytest.mark.parametrize(
    "id_",
    [
        "../etc/passwd",
        "lucide:../../x",
        "https://example.org/x",
        "lucide:<script>",
        "iconify:a:b:c",
        "lucide:",
        "lucide:" + "a" * 201,
    ],
)
def test_invalid_ids(id_):
    assert not valid_id(id_)


def test_persian_normalization():
    assert normalize("  كِتاب ي ۱۲۳\u200cابر ") == "کتاب ی 123 ابر"
    assert valid_id("simple-icons:handshake_protocol")


@pytest.mark.parametrize(
    "text,id_",
    [
        ("خدمات ابری", "cloud"),
        ("cloud infrastructure", "cloud"),
        ("وکالت و قرارداد", "scale"),
        ("پزشکی", "stethoscope"),
        ("باغبانی", "sprout"),
        ("تعمیرات", "wrench"),
        ("آموزش", "graduation-cap"),
    ],
)
def test_bilingual_fallback(catalog, text, id_):
    result = Matcher(catalog).suggest(text)
    assert result["engine"] == "lexical"
    assert "lucide:" + id_ in [i["id"] for i in result["suggestions"][:3]]


def test_empty_match(catalog):
    result = Matcher(catalog).suggest("zzzzzz")
    assert result["lowConfidence"]
    assert result["suggestions"][0]["id"] == "lucide:briefcase"


def test_api_contract(catalog):
    with TestClient(create_app(catalog.root)) as client:
        assert client.get("/health/ready").json()["icons"] == 12
        first = client.get("/v1/icons?limit=3").json()
        assert len(first["items"]) == 3
        assert first["nextOffset"] == 3
        assert client.get("/v1/icons?limit=101").status_code == 422
        assert (
            client.get("/v1/icons/lucide:cloud").json()["iconUrl"]
            == "/assets/icons/lucide/cloud.svg"
        )
        assert client.get("/v1/icons/categories").json()["categories"]
        assert client.get("/v1/icons/sources").json()["sources"]
        assert client.post("/v1/icons/suggest", json={"title": "ابر"}).json()[
            "suggestions"
        ]
        assert client.post("/v1/icons/suggest", content="a" * 16385).status_code == 413
        assert (
            client.post(
                "/v1/icons/suggest", json={"text": "a", "limit": 21}
            ).status_code
            == 422
        )
        assert client.post("/admin/ingest").status_code == 404
        svg = client.get("/assets/icons/lucide/cloud.svg")
        assert svg.status_code == 200
        assert "nosniff" == svg.headers["x-content-type-options"]
        assert (
            client.get(
                "/assets/icons/lucide/cloud.svg",
                headers={"If-None-Match": svg.headers["etag"]},
            ).status_code
            == 304
        )


def test_missing_assets(catalog, tmp_path, monkeypatch):
    monkeypatch.setenv("ICON_SEMANTIC_ENABLED", "true")
    with TestClient(
        create_app(catalog.root, tmp_path / "missing", tmp_path / "missing")
    ) as client:
        assert client.get("/health/ready").json()["mode"] == "lexical"
        assert (
            client.post("/v1/icons/suggest", json={"text": "cloud"}).status_code == 200
        )
    with TestClient(create_app(tmp_path / "absent")) as client:
        assert client.get("/health/live").status_code == 200
        assert client.get("/health/ready").status_code == 503


def test_rate_limit(catalog, monkeypatch):
    monkeypatch.setenv("ICON_RATE_PER_MINUTE", "2")
    with TestClient(create_app(catalog.root)) as client:
        assert client.get("/v1/icons").status_code == 200
        assert client.get("/v1/icons").status_code == 200
        assert client.get("/v1/icons").status_code == 429


def test_timeout_and_busy(catalog, monkeypatch):
    monkeypatch.setenv("ICON_TIMEOUT_SECONDS", ".01")
    app = create_app(catalog.root)
    with TestClient(app) as client:
        original = app.state.matcher.suggest

        def slow(**args):
            time.sleep(0.15)
            return original(**args)

        app.state.matcher.suggest = slow
        result = client.post("/v1/icons/suggest", json={"text": "cloud"})
        assert result.status_code == 200
        assert result.json()["reason"] == "semantic_timeout"
        assert (
            client.post("/v1/icons/suggest", json={"text": "cloud"}).status_code == 429
        )


def test_vector_resume_and_version(catalog, tmp_path):
    class Model:
        version = "fake-v1"

        def encode(self, texts):
            out = np.zeros((len(texts), 384), dtype=np.float32)
            out[:, 0] = 1
            return out

    model = Model()
    root = tmp_path / "vectors"
    assert build(root, catalog, model, max_icons=3)["count"] == 3
    assert build(root, catalog, model)["count"] == 12
    assert build(root, catalog, model)["count"] == 12
    assert len(Vectors(root, catalog, model).nearest(model.encode(["a"])[0])) == 12
    model.version = "different"
    with pytest.raises(ValueError):
        Vectors(root, catalog, model)
    with pytest.raises(ValueError):
        build(root, catalog, model)


@pytest.mark.skipif(
    not os.getenv("TEST_ICON_MODEL_DIR"), reason="pinned model is optional in unit CI"
)
def test_real_e5_no_network(catalog, monkeypatch):
    import socket

    monkeypatch.setattr(
        socket.socket,
        "connect",
        lambda *_: (_ for _ in ()).throw(AssertionError("network forbidden")),
    )
    model = E5(os.environ["TEST_ICON_MODEL_DIR"])
    vectors = model.encode(["cloud infrastructure", "زیرساخت ابری"], True)
    assert vectors.shape == (2, 384)
    assert np.allclose(np.linalg.norm(vectors, axis=1), 1, atol=1e-5)
    assert vectors[0] @ vectors[1] > 0.7
    with TestClient(create_app(catalog.root)) as client:
        assert client.get("/assets/icons/lucide/cloud.svg").status_code == 200


def test_vector_reuse_only_for_unchanged_text(catalog, tmp_path):
    class Model:
        version = "reuse-v1"
        count = 0

        def encode(self, texts):
            self.count += len(texts)
            return np.ones((len(texts), 384), dtype=np.float32) / np.sqrt(384)

    model = Model()
    old = tmp_path / "old"
    new = tmp_path / "new"
    build(old, catalog, model)
    assert model.count == 12
    from app.catalog import connect

    db = connect(catalog.path)
    db.execute(
        "UPDATE icons SET search_text='changed cloud meaning' WHERE id='lucide:cloud'"
    )
    db.commit()
    db.close()
    catalog.manifest["version"] = "fixture-v2"
    report = build(new, catalog, model, reuse_root=old)
    assert report["reused"] == 11
    assert model.count == 13
    assert report["count"] == 12


def test_malformed_svg():
    with pytest.raises(ValueError, match="malformed"):
        sanitize('<svg viewBox="0 0 24 24"><path></svg>')


def test_catalog_integrity(catalog):
    from app.catalog import Catalog

    with catalog.path.open("ab") as f:
        f.write(b"corruption")
    with pytest.raises(ValueError, match="integrity"):
        Catalog(catalog.root)


def test_unapproved_model_revision(tmp_path):
    (tmp_path / "manifest.json").write_text(
        json.dumps(
            {"revision": "unapproved", "repository": "intfloat/multilingual-e5-small"}
        )
    )
    with pytest.raises(ValueError, match="revision"):
        E5(tmp_path)
