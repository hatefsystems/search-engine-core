import json
from .text import CONFIG, SYNONYMS, normalize, categories, terms, expand


class Matcher:
    def __init__(self, catalog, model=None, vectors=None):
        self.catalog, self.model, self.vectors = catalog, model, vectors
        with catalog.db() as db:
            row = db.execute("SELECT value FROM state WHERE key='policy'").fetchone()
        self.policy = (
            json.loads(row[0])
            if row
            else json.loads((CONFIG / "policy.json").read_text())
        )

    def suggest(
        self,
        text,
        category="",
        context="",
        preferredSource="",
        preferredStyle="",
        limit=5,
    ):
        candidates = self.catalog.search(text, limit=150)
        cats = set(categories(text))
        semantic = {}
        available = False
        if category:
            cats.add(category)
        for cat in sorted(cats):
            candidates += self.catalog.search(category=cat, limit=60)
        if self.model and self.vectors:
            try:
                semantic = dict(self.vectors.nearest(self.model.query(text), 100))
                available = True
                candidates += [m for i in semantic if (m := self.catalog.get(i))]
            except Exception:
                pass  # Inference failure must leave lexical search functional.
        qt = terms(expand(text))
        ranked = []
        seen = set()
        for item in candidates:
            if item["id"] in seen:
                continue
            seen.add(item["id"])
            if item.get("brand") and not terms(item["labelEn"]).issubset(terms(text)):
                continue
            ct = terms(
                expand(
                    item["name"].replace("-", " ")
                    + " "
                    + " ".join(item.get("keywords", []))
                )
            )
            lexical = len(qt & ct) / max(1, len(qt))
            category_score = float(bool(cats & set(item.get("categories", []))))
            weights = self.policy["weights"]
            score = (
                weights["lexical"] * lexical
                + weights["category"] * category_score
                + weights["semantic"] * max(0.0, semantic.get(item["id"], 0.0))
            )
            if not available:
                score /= max(1e-9, weights["lexical"] + weights["category"])
            canonical = any(
                normalize(item["name"])
                in {
                    normalize(a)
                    for a in SYNONYMS["categories"].get(c, {}).get("aliases", [])
                }
                for c in cats
            )
            score += (
                0.06 * canonical
                + 0.05 * (item["source"] == (preferredSource or "lucide"))
                + 0.01 * (item["style"] == preferredStyle)
            )
            ranked.append((score, item))
        ranked.sort(key=lambda x: (-x[0], x[1]["id"]))
        output = []
        hashes = set()
        families = set()
        for score, item in ranked:
            family = (
                ("lucide", item["name"])
                if item["source"] == "lucide"
                or item["id"].startswith("iconify:lucide:")
                else (item["source"], item["id"])
            )
            if item["hash"] in hashes or family in families:
                continue
            hashes.add(item["hash"])
            families.add(family)
            output.append({**item, "score": round(score, 6), "rank": len(output) + 1})
            if len(output) >= limit:
                break
        fallback = not output or output[0]["score"] < self.policy["confidenceThreshold"]
        if not output and (generic := self.catalog.get("lucide:briefcase")):
            output = [{**generic, "score": 0.0, "rank": 1}]
        margin = (
            output[0]["score"] - output[1]["score"]
            if len(output) > 1
            else (output[0]["score"] if output else 0.0)
        )
        return {
            "suggestions": output,
            "engine": "hybrid" if available else "lexical",
            "modelAvailable": available,
            "catalogVersion": self.catalog.manifest["version"],
            "semanticCoverage": (
                self.vectors.count / self.catalog.count
                if self.vectors and self.catalog.count
                else 0.0
            ),
            "lowConfidence": fallback or margin < self.policy["confidenceMargin"],
            "scoreMeaning": "ranking score, not probability",
            "context": context,
        }
