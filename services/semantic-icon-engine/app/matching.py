"""Deterministic evidence fusion. Weak semantics never auto-select an icon."""

import json
from .text import (
    CONFIG,
    SYNONYMS,
    categories,
    concepts,
    brand_matches,
    lexical_tier,
)


class Matcher:
    def __init__(self, catalog, model=None, vectors=None, knowledge=None):
        self.catalog, self.model, self.vectors = catalog, model, vectors
        self.knowledge = knowledge
        with catalog.db() as db:
            row = db.execute("SELECT value FROM state WHERE key='policy'").fetchone()
        self.policy = (
            json.loads(row[0])
            if row
            else json.loads((CONFIG / "policy.json").read_text())
        )

    def _rank(self, text, category="", source="", style=""):
        lexical = self.catalog.search(text, category, source, style, limit=500)
        candidates = {item["id"]: item for item in lexical}
        positions = {item["id"]: rank for rank, item in enumerate(lexical)}
        semantic = {}
        available = False
        if self.model and self.vectors:
            try:
                semantic = dict(self.vectors.nearest(self.model.query(text), 150))
                available = True
                for icon_id in semantic:
                    if icon_id not in candidates and (
                        item := self.catalog.get(icon_id)
                    ):
                        candidates[icon_id] = item
            except Exception:
                pass
        anchors = {}
        for cat in categories(text):
            for rank, name in enumerate(
                SYNONYMS["categories"].get(cat, {}).get("icons", [])
            ):
                for icon_id in ["lucide:" + name, "iconify:lucide:" + name]:
                    if item := self.catalog.get(icon_id):
                        candidates[icon_id] = item
                        anchors[icon_id] = max(
                            anchors.get(icon_id, 0), 0.50 - 0.025 * rank
                        )
        mapped = (
            self.knowledge.icon_candidates(text, self.model) if self.knowledge else {}
        )
        for icon_id in mapped:
            if item := self.catalog.get(icon_id):
                candidates[icon_id] = item
        brand_query = bool(concepts(text, brands=True))
        semantic_positions = {icon_id: rank for rank, icon_id in enumerate(semantic)}
        ranked = []
        for item in candidates.values():
            if (
                (category and category not in item.get("categories", []))
                or (source and source != item["source"])
                or (style and style != item["style"])
                or not brand_matches(item, text)
            ):
                continue
            icon_id = item["id"]
            tier = lexical_tier(item, text)
            sim = semantic.get(icon_id, 0)
            if (
                icon_id not in positions
                and icon_id not in anchors
                and icon_id not in mapped
                and sim < self.policy.get("searchSemanticThreshold", 0.75)
            ):
                continue
            # Comparable reciprocal ranks let either retriever improve ordering.
            fusion = 1 / (60 + positions[icon_id]) if icon_id in positions else 0
            fusion += (
                1 / (60 + semantic_positions[icon_id])
                if icon_id in semantic_positions
                else 0
            )
            score = 12 * fusion
            score += 2.0 * max(
                0.0, sim - self.policy.get("searchSemanticThreshold", 0.75)
            )
            score += {0: 1.4, 1: 0.65, 2: 0.30, 3: 0.40, 4: 0.18}.get(tier, 0)
            score += anchors.get(icon_id, 0)
            evidence = "exact" if tier == 0 else "related"
            if item.get("brand") and brand_query:
                score += 1.0
            if icon_id in mapped:
                score += mapped[icon_id]["score"]
                evidence = mapped[icon_id]["evidence"]
            # Source is only a deterministic tie break; it never suppresses a
            # stronger semantic or concept match.
            preferred = 0 if item["source"] == "lucide" else 1
            ranked.append((score, preferred, item, evidence))
        ranked.sort(key=lambda row: (-row[0], row[1], row[2]["id"]))
        output, hashes, families = [], set(), set()
        for score, _, item, evidence in ranked:
            mapping = mapped.get(item["id"], {})
            # A mirrored Lucide glyph shares relevance with the canonical ID.
            # Preserve the family's best score while returning its stable direct
            # identity, unless the caller explicitly filters the source.
            if item["id"].startswith("iconify:lucide:") and not source:
                direct = self.catalog.get("lucide:" + item["name"])
                if (
                    direct
                    and (not category or category in direct.get("categories", []))
                    and (not style or style == direct["style"])
                ):
                    item = direct
            family = (
                ("lucide", item["name"])
                if (
                    item["source"] == "lucide"
                    or item["id"].startswith("iconify:lucide:")
                )
                else (item["source"], item["id"])
            )
            if item["hash"] in hashes or family in families:
                continue
            hashes.add(item["hash"])
            families.add(family)
            output.append(
                {
                    **item,
                    "score": round(score, 6),
                    "evidence": evidence,
                    "conceptId": mapping.get("conceptId"),
                    "rank": len(output) + 1,
                }
            )
        return output, available

    def search(self, text="", category="", source="", style="", limit=20, offset=0):
        if not text.strip():
            items = self.catalog.search("", category, source, style, limit + 1, offset)
            return {
                "items": items[:limit],
                "nextOffset": offset + limit if len(items) > limit else None,
                "engine": "lexical",
                "catalogVersion": self.catalog.manifest["version"],
            }
        output, available = self._rank(text, category, source, style)
        return {
            "items": output[offset : offset + limit],
            "nextOffset": offset + limit if len(output) > offset + limit else None,
            "engine": "hybrid" if available else "lexical",
            "catalogVersion": self.catalog.manifest["version"],
        }

    def suggest(
        self,
        text,
        category="",
        context="",
        preferredSource="",
        preferredStyle="",
        limit=5,
    ):
        output, available = self._rank(text)
        confident = bool(output and output[0]["evidence"] in ("exact", "curated"))
        if confident and output[0]["evidence"] == "curated":
            top = output[0]
            confident = not any(
                candidate["evidence"] == "curated"
                and candidate.get("conceptId") != top.get("conceptId")
                and top["score"] - candidate["score"] < 0.2
                for candidate in output[1:]
            )
        output = output[:limit]
        if not output and (generic := self.catalog.get("lucide:briefcase")):
            output = [{**generic, "score": 0.0, "rank": 1, "evidence": "fallback"}]
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
            "lowConfidence": not confident,
            "scoreMeaning": "ranking score, not probability",
            "context": context,
        }
