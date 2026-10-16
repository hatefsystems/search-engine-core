"""Versioned normalization without destructive stemming or runtime translation."""

import functools
import json
import re
import unicodedata
from pathlib import Path

CONFIG = Path(__file__).resolve().parents[1] / "config"
VERSION = "fa-en-v3"
SYNONYMS = json.loads((CONFIG / "synonyms.json").read_text())


@functools.lru_cache(maxsize=16384)
def normalize(text):
    text = (
        unicodedata.normalize("NFKC", text)
        .lower()
        .translate(str.maketrans("يك٠١٢٣٤٥٦٧٨٩۰۱۲۳۴۵۶۷۸۹", "یک01234567890123456789"))
    )
    text = "".join(
        " " if unicodedata.category(c) == "Cf" else c
        for c in text
        if not (
            "\u0610" <= c <= "\u061a"
            or "\u064b" <= c <= "\u065f"
            or c == "\u0670"
            or "\u06d6" <= c <= "\u06ed"
        )
    )
    return " ".join(re.findall(r"[^\W_]+", text))


def terms(text):
    return set(normalize(text).split())


def categories(text):
    value = " " + normalize(text) + " "
    return [
        k
        for k, v in SYNONYMS["categories"].items()
        if any(" " + normalize(a) + " " in value for a in v["aliases"])
    ]


@functools.lru_cache(maxsize=2048)
def concepts(text, related=False, brands=None):
    value = " " + normalize(text) + " "
    return [
        key
        for key, spec in SYNONYMS.get("concepts", {}).items()
        if (brands is None or bool(spec.get("brand")) == brands)
        and any(
            " " + normalize(alias) + " " in value
            for alias in spec["aliases"] + (spec.get("related", []) if related else [])
        )
    ]


@functools.lru_cache(maxsize=256)
def equivalents(text):
    """Direct concept names only. Related phrases never become exact matches."""
    return list(
        dict.fromkeys(
            [
                normalize(text),
                *(normalize(key) for key in concepts(text)),
                *(
                    normalize(alias)
                    for key in concepts(text)
                    for alias in SYNONYMS["concepts"][key]["aliases"]
                ),
            ]
        )
    )


def expand(text, related=False):
    return normalize(
        text
        + " "
        + " ".join(
            " ".join(SYNONYMS["concepts"][key]["aliases"])
            for key in concepts(text, related=related)
        )
        + " "
        + " ".join(
            " ".join(SYNONYMS["categories"][key]["aliases"]) for key in categories(text)
        )
    )


def category_label(key):
    return SYNONYMS["categories"].get(key, {}).get("fa", "سایر")


def enrich(name, tags, brand=False):
    value = name.replace("-", " ") + " " + " ".join(tags)
    matched = concepts(value, brands=brand)
    cats = list(
        dict.fromkeys(
            [
                *(["brands", "technology"] if brand else []),
                *(
                    SYNONYMS["concepts"][key]["category"]
                    for key in matched
                    if SYNONYMS["concepts"][key].get("category")
                ),
                *categories(value),
            ]
        )
    ) or ["general"]
    # Prefer the longest specific concept, e.g. heart-pulse before heart.
    matched.sort(key=lambda key: (-len(normalize(key)), key))
    label = SYNONYMS["concepts"][matched[0]]["fa"] if matched else ""
    return cats, label


def brand_matches(item, text):
    """Recognize reviewed aliases, including AWS, without suggesting random brands."""
    if not item.get("brand"):
        return True
    value = " " + normalize(text) + " "
    names = [item["labelEn"], item["name"]]
    if any(" " + normalize(name) + " " in value for name in names):
        return True
    return bool(
        set(concepts(text, brands=True)) & set(concepts(" ".join(names), brands=True))
    )


def lexical_tier(item, text):
    names = [normalize(item["name"]), normalize(item["labelEn"])]
    direct = equivalents(text)
    if any(name in direct for name in names):
        return 0
    canonical_names = {normalize(key) for key in SYNONYMS.get("concepts", {})}
    if any(name.startswith(term + " ") for name in names for term in direct if term):
        return 1 if canonical_names.intersection(names) else 2
    related_keys = set(concepts(text, related=True))
    if {normalize(key) for key in related_keys}.intersection(names):
        return 3
    if related_keys & set(concepts(" ".join(names))):
        return 4
    return 5
