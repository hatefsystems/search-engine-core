# Epic: Universal Automated Multilingual Ranking & Retrieval (Zero Manual Labels)

**Goal:** Build an end‑to‑end, self‑hosted ranking system that automatically works for any language/script worldwide without manual labeling or language-specific configuration. Universal retrieval (BM25 + n‑gram) + authority/structure signals + embedding‑based re‑rank + weak‑supervision intent + spam/quality scoring + online learning from clicks. Target P95 latency ≤ 300ms for any language.

**Outcomes / Success Criteria**
- NDCG@10 (proxy, then click‑based) ↑ over baseline by ≥ 20%.
- CTR@1/3/10 ↑ month‑over‑month; Navigational success@Top‑3 ≥ 95%.
- P95 latency ≤ 300ms for head queries; ≤ 500ms for tail queries.
- Index coverage, dedup rate, spam down‑ranking, and diversity within guardrails.
- 100% self‑hosted; no third‑party services.

**Non‑Goals**
- No manual labeling/curation.
- No external cloud APIs for ranking or embeddings.

**Tech Notes / Assumptions**
- Universal core retrieval via RedisSearch (or equivalent) with weighted fields supporting any language/script.
- Multilingual re‑ranker/service in C++ or Python (uWebSockets/HTTP) with feature store.
- Nightly batch jobs for universal signals; weekly multilingual embeddings refresh with incremental updates.
- Automatic language/script detection with no manual configuration required.

---

## Milestones (suggested)
- **M0 – Bootstrap**: Normalization + language detection + minimal index.
- **M1 – Retrieval Baseline**: Weighted BM25 + char n‑gram fallback + dedup/quality gate.
- **M2 – Authority & Structure**: Link graph (HostRank) + schema/ISBN/price extractors.
- **M3 – Synonyms & Embeddings**: Co‑occurrence → PPMI/SVD; train subword embeddings; nightly lexicon.
- **M4 – Intent & Verticals**: Weakly‑supervised classifier for Info/Trans/Nav + vertical detectors (Book/Product/Article…).
- **M5 – Spam & Quality**: One‑Class SVM/Isolation Forest + site‑level roll‑ups.
- **M6 – Rank Fusion & Diversification**: Feature fusion, MMR diversification, parameter tuning (proxy objective).
- **M7 – Metrics & Dashboards**: Proxy metrics, then click‑based; interleaving.
- **M8 – Click Model & Online Learning**: DCTR/UBM/DBN + pairwise LTR; nightly updates.
- **M9 – Performance & SRE**: P95≤300ms, caching, feature store, load tests; SLOs/runbooks.

---

## Labels / Project Setup (recommended)
- `area/index`, `area/ranking`, `area/embeddings`, `area/signals`, `area/eval`, `area/devops`, `area/security`  
- `kind/feature`, `kind/infra`, `kind/bug`, `kind/research`  
- `priority/P0`, `priority/P1`, `priority/P2`  
- `status/backlog`, `status/in‑progress`, `status/review`, `status/done`

---

## High‑Level Tasks & Sub‑Tasks (GitHub‑ready)

### 1) Universal Language & Text Normalization (M0)
**Issue Title:** `[M0][core] Universal text normalization & automatic language detection`

**Description:** Implement comprehensive Unicode normalization (NFKC) supporting all scripts worldwide, automatic character unification across languages, script-specific handling (ZWNJ for Arabic scripts, word segmentation for CJK), and universal language detection supporting 100+ languages without manual configuration.

**Tasks**
- [ ] Implement universal Unicode NFKC normalization pipeline supporting all scripts
- [ ] Build script-agnostic character normalization (Arabic→Persian, Cyrillic variants, CJK unification)
- [ ] Create language-agnostic n-gram detection supporting 100+ languages/scripts
- [ ] Implement script-specific preprocessing (ZWNJ for Arabic scripts, word segmentation for CJK)
- [ ] Add automatic language confidence scoring and fallback handling
- [ ] Comprehensive unit tests with content from 20+ languages/scripts
- [ ] Export normalized text with language metadata to downstream indexer

**Acceptance Criteria**
- Universal normalizer works for all Unicode scripts and reduces token variants
- Language detection accuracy ≥ 95% across 50+ languages on test corpus
- Script-specific handling works for Arabic, CJK, Cyrillic, Latin scripts
- Automatic language detection with confidence scoring
- Pipeline adds universal `lang` and `script` fields per doc/query

---

### 2) Universal Retrieval Index (BM25 + n‑gram) (M1)
**Issue Title:** `[M1][core] Universal BM25 weighted index + character n-gram fallback`

**Description:** Build primary lexical index with universal field weights supporting any language automatically, and secondary character n-gram index for unknown/mixed language content.

**Tasks**
- [ ] Define universal schema: `title`, `h1h3`, `body`, `anchors`, `url`, `domain`, `lang`, `script`, `timestamp`, `schema_type`, `author`, `publisher`, `isbn`, `pagerank`, `url_quality`.
- [ ] Configure universal BM25 with field weights (works for any detected language).
- [ ] Build language-agnostic character n-gram index (3-5 grams) for fallback retrieval.
- [ ] Implement automatic query-time language/script detection and routing.
- [ ] Implement cross-language deduplication (URL-level + content simhash/shingles).
- [ ] Universal quality gate: filter short/boilerplate pages regardless of language.
- [ ] Integration tests & latency benchmarks for multiple languages.

**Acceptance Criteria**
- Universal top-N retrieval works for any detected language with n-gram fallback.
- Cross-language deduplication reduces near-duplicates by ≥ 60% without hurting recall > 2%.
- P95 retrieval latency ≤ 80ms for BM25 top-200 across all supported languages.

---

### 3) Universal Link Graph & HostRank (M2)
**Issue Title:** `[M2][graph] Universal link graph construction + host-level PageRank (HostRank)`

**Description:** From crawled data of any language websites worldwide, construct universal URL→URL and host→host graphs; compute HostRank prestige scores that work across all languages.

**Tasks**
- [ ] Extract outlinks/inlinks from crawl.  
- [ ] Build host graph; collapse by eTLD+1.  
- [ ] Compute PageRank on host graph (d=0.85); normalize to [0,1].  
- [ ] Persist `hostrank_norm` in feature store and index.  
- [ ] Add daily incremental updates.  
- [ ] Validate against sample domains; sanity checks.

**Acceptance Criteria**
- HostRank correlates with known authoritative sites.  
- Daily delta updates complete within SLA (< 30m for 100M edges).

---

### 4) Universal Structured Signals Extraction (M2)
**Issue Title:** `[M2][extraction] Universal structured data extraction (schema.org + regex hints)`

**Description:** Auto-detect schema.org types and key fields (Book/Product/Article) from web pages in any language. Universal regex hints for ISBN, price, author/publisher that work across languages and scripts.

**Tasks**
- [ ] Parse JSON‑LD/Microdata/RDFa universally; collect `@type` from any language content.
- [ ] Extract fields: `isbn`, `author`, `publisher`, `price`, `offer`, `brand` (works for any script).
- [ ] Universal regex hints for ISBN-10/13; multi-currency price patterns.
- [ ] Store `schema_type` and fields in index; add `structured_boost` flag.
- [ ] Unit tests on varied markup from different languages/scripts.

**Acceptance Criteria**
- ≥ 90% precision for ISBN extraction on test pages from any language.
- `schema_type` present for ≥ 60% of structured pages worldwide.

---

### 5) Universal Synonym & Related-Terms Mining (M3)
**Issue Title:** `[M3][embeddings] Universal co-occurrence → PPMI/SVD + subword embeddings + nightly lexicon`

**Description:** Build universal distributional semantics pipeline to mine near-synonyms/related phrases across all languages, plus cross-lingual semantic connections.

**Tasks**
- [ ] Build co‑occurrence matrix (window=5–10) over titles/body/anchors.  
- [ ] Compute PPMI and truncated SVD for low‑rank embeddings.  
- [ ] Train subword skip‑gram (fastText‑style) on corpus; export vectors.  
- [ ] Cluster frequent n‑grams (bi/tri‑grams) to mine phrases.  
- [ ] Cross‑lingual alignment (unsupervised) or anchor/URL pivot mapping.  
- [ ] Nightly job to export `lexicon.json` (top‑k related terms per token/phrase).  
- [ ] Thresholding + caps (≤ 3 expansions/query).  
- [ ] Evaluation: intrinsic (cosine neighbors) + retrieval delta.

**Acceptance Criteria**
- Query expansion improves recall with ≤ 5% precision loss (proxy).  
- Lexicon build finishes nightly within SLA.

---

### 6) Universal Intent & Vertical Detection (M4)
**Issue Title:** `[M4][classification] Universal weakly-supervised intent (Info/Trans/Nav) + vertical classifiers`

**Description:** Use universal structural seeds (Product/Offer/price/ISBN) and navigational patterns to train light classifiers that work across any language.

**Tasks**
- [ ] Generate universal transactional seeds from structured pages (Product/Offer/price in any language).
- [ ] Generate language-agnostic navigational seeds (brand/domain patterns).
- [ ] Train logistic/GBDT classifier for intent from weak labels (works for any language).
- [ ] Build universal vertical detectors: Book/Product/Article/Download using schema + phrase clusters.
- [ ] Export `intent_probs` and `vertical_type` per query/document in any language.
- [ ] Evaluate with universal proxy metrics.

**Acceptance Criteria**
- Navigational success@Top‑3 ≥ 95% on synthetic tests across languages.
- Transactional queries show ≥ 30% presence of Product/Offer in Top‑10 for any language.

---

### 7) Universal Spam & Quality Scoring (M5)
**Issue Title:** `[M5][quality] Universal one-class spam/quality scoring + site-level roll-ups`

**Description:** Detect low-quality/spam content using unsupervised modeling with language-agnostic page/site features that work worldwide.

**Tasks**
- [ ] Extract features: text/HTML ratio, ad/script density, keyword abuse, outlink patterns, near‑dup %, content volatility.  
- [ ] Train Isolation Forest / One‑Class SVM; calibrate to [0,1] `spamness`.  
- [ ] Site‑level aggregation; global down‑rank for high‑spam domains.  
- [ ] Add safe‑list for well‑known hosts.  
- [ ] Evaluate using manual spot checks + proxy impact.

**Acceptance Criteria**
- Spammy hosts demoted without harming head quality; complaint rate ↓.  
- False demotion rate < 2% in spot checks.

---

### 8) Universal Ranking Fusion & Diversification (M6)
**Issue Title:** `[M6][ranking] Universal feature fusion + MMR diversification + auto parameter tuning`

**Description:** Combine universal features into a final score that works across all languages; light MMR to cover sub-intents; auto-tune weights on proxy objective.

**Tasks**
- [ ] Implement universal feature gather: bm25, embSim(q,d), hostrank, anchorMatch, structuredBoost, freshness, urlQuality, spamness, intentAlign (works for any language).
- [ ] Implement `FinalScore` formula with configurable weights for any language.
- [ ] MMR diversification with lambda control to reduce domain repetition.
- [ ] Line/grid search over weights to optimize universal proxy metrics.
- [ ] Safe parameter deployment (feature-flagged).
- [ ] Integration tests across multiple languages.

**Acceptance Criteria**
- Proxy NDCG@10 improves ≥ 10% vs. BM25-only for any language.
- Diversity guardrail: ≤ 3 results per domain in Top‑10 (configurable).

---

### 9) Universal Embeddings Service & Feature Store (M3/M6)
**Issue Title:** `[M3/M6][embeddings] Universal embedding training + inference service + doc precompute`

**Description:** Train universal multilingual subword sentence/word embeddings supporting any language/script; expose inference as a local service; precompute doc embeddings.

**Tasks**
- [ ] Train universal sentence/word embeddings (subword) on multilingual corpus; version models.
- [ ] Build local inference service (HTTP/uWS) with batching + cache for any language.
- [ ] Precompute doc embeddings for any language content; store vector IDs in feature store.
- [ ] Implement cache for frequent query embeddings in any language.
- [ ] Latency tests; throughput targets for universal embeddings.

**Acceptance Criteria**
- embSim available for re-rank within ≤ 10ms/doc (cached batch) for any language.
- Model versioning + rollback documented for multilingual models.

---

### 10) Universal Query Pipeline (M1–M6)
**Issue Title:** `[M1–M6][pipeline] Universal query pipeline: auto-detect → expand → retrieve → re-rank → diversify`

**Description:** Implement full online flow with automatic language detection, universal expansion and fallback n-gram retrieval that works for any language worldwide.

**Tasks**
- [ ] Automatic language/script detection (no manual configuration needed).
- [ ] Universal query expansion from nightly `lexicon.json` (≤ 3 terms, thresholded, works for any language).
- [ ] Retrieve BM25@200 + n-gram@100; merge + dedup (universal across languages).
- [ ] Gather features; call embeddings service; compute FinalScore for any language.
- [ ] Diversify (MMR); cut Top‑K; cache popular queries in any language.
- [ ] Expose API; add metrics/logging for multilingual queries.

**Acceptance Criteria**
- P95 end-to-end latency ≤ 300ms for head queries in any language.
- API returns feature traces for debugging when `debug=1` for any language.

---

### 11) Universal Metrics & Evaluation Framework (M7)
**Issue Title:** `[M7][metrics] Universal proxy metrics + dashboards + interleaving harness`

**Description:** Instrument universal proxy metrics for any language; enable team-draft interleaving for safe online comparison across all supported languages.

**Tasks**
- [ ] Universal proxy metrics: NDCG@10 (anchor/title), nav success, vertical presence, diversity, dedup rate (works for any language).
- [ ] Build Grafana dashboards (CTR once available); latency and error budgets for multilingual queries.
- [ ] Implement interleaving framework between rankers for any language.
- [ ] Export daily reports with language breakdowns.

**Acceptance Criteria**
- Dashboards live with SLIs/SLOs; interleaving on by flag for any language.
- Daily report artifacts in object storage with multilingual analytics.

---

### 12) Universal Click Logging & Online Learning (M8)
**Issue Title:** `[M8][learning] Universal click model + pairwise LTR + nightly updates`

**Description:** Collect clicks/dwell with position bias from any language queries; train universal click model (DCTR/UBM/DBN) and use debiased labels for pairwise LTR.

**Tasks**
- [ ] Collect impressions/clicks with position/examined depth; anonymize PII.  
- [ ] Train click model; estimate attractiveness; store per (q,d).  
- [ ] Train pairwise LTR (LambdaMART/GBDT) on debiased labels.  
- [ ] Nightly model refresh; model gating and rollback.  
- [ ] Interleaving to validate improvements.

**Acceptance Criteria**
- Stat‑sig CTR improvement on interleaving runs.  
- Safe rollbacks; no regression on latency.

---

### 13) Universal Performance & Caching (M9)
**Issue Title:** `[M9][performance] Universal caching, precomputation & feature store for P95≤300ms`

**Description:** Introduce multi-layer caches and precompute heavy features for any language. Centralize universal features with TTL/versioning that work worldwide.

**Tasks**
- [ ] Universal query result cache for hot head queries in any language; TTL + invalidation.
- [ ] Precompute doc features (hostrank, spamness, structured flags) for any language content.
- [ ] Universal feature store API with versioned keys supporting all languages.
- [ ] Load tests (wrk/vegeta); capacity planning for multilingual queries.
- [ ] Optimize serialization and network hops for any language.

**Acceptance Criteria**
- Sustained QPS target with P95 latency within SLOs for any language.
- Cache hit-rate ≥ 60% on head traffic across all languages.

---

### 14) Universal DevOps & Reliability (M9)
**Issue Title:** `[M9][devops] Universal SLOs, alerts, canaries, rollbacks & runbooks`

**Description:** Productionize universal multilingual ranking services with observability, safe deployment, and incident response that works for any language.

**Tasks**
- [ ] Define universal SLIs/SLOs: latency, error rate, freshness lag, index health for any language.
- [ ] Alerts + on-call; runbooks for index rebuild, model rollback, cache flush (multilingual).
- [ ] Blue/green or canary deploys; feature flags for multilingual models.
- [ ] Backups for index/feature store; disaster recovery drills.
- [ ] Security hardening; audit logs for any language content.

**Acceptance Criteria**
- One full rollback drill completed; MTTR targets documented for any language.
- Alert fatigue avoided; actionable alerts only across all languages.

---

### 15) Universal Security & Compliance (continuous)
**Issue Title:** `[SEC] Universal robots compliance, PII safeguards, self-hosted isolation`

**Description:** Respect robots directives worldwide; ensure anonymized analytics for any language; isolate services and restrict egress globally.

**Tasks**
- [ ] Enforce robots and noindex/noarchive policies in retrieval for any language.
- [ ] Anonymize click logs from any language; rotate salts/keys; retention windows.
- [ ] Network policy: egress deny by default; allowlist only.
- [ ] Secrets management; least-privilege for components.
- [ ] Periodic security review for multilingual content.

**Acceptance Criteria**
- No robots violations globally; privacy review passed for all languages.
- Pen-test issues resolved or accepted with plan.

---

### 16) Universal Documentation & Runbooks (continuous)
**Issue Title:** `[DOC] Universal architecture, API, feature glossary & troubleshooting`

**Description:** Author comprehensive documentation for devs and on-call, including API contracts, feature descriptions, and debugging guides for the universal multilingual system.

**Tasks**
- [ ] Architecture diagrams and dataflow.  
- [ ] API spec for search endpoint (+ debug mode).  
- [ ] Feature glossary (bm25, embSim, hostrank, freshness, spamness, structuredBoost, intentAlign).  
- [ ] Troubleshooting guide for common ranking symptoms.  
- [ ] New‑hire onboarding doc.

**Acceptance Criteria**
- Docs reviewed and versioned; CI link‑check passes.

---

## Ranker Formula (reference for implementation)
```
FinalScore(d|q) =
  0.55 * BM25(d|q)
+ 0.15 * EmbSim(q,d)
+ 0.10 * HostRank(d)
+  0.06 * AnchorMatch(d|q)
+  0.05 * StructuredBoost(d,q)
+  0.04 * FreshnessDecay(d)
+  0.03 * URLQuality(d)
-  0.08 * Spamness(d)
+  0.04 * IntentAlign(q,d)
```
*Weights tuned automatically via line/grid search on universal proxy metrics that work across all languages, then refined with click‑based online learning from any language queries.*

---

## Copy‑Paste Issue Templates (for GitHub)

**Template A — Feature**
```
Title: [M#][area] <Feature name>
Labels: kind/feature, area/<area>, priority/P1, status/backlog

### Summary
<What and why>

### Tasks
- [ ]
- [ ]

### Acceptance Criteria
- [ ]

### Dependencies
-

### Notes
-
```

**Template B — Infra/Service**
```
Title: [M#][infra] <Service/Component>
Labels: kind/infra, area/devops, priority/P1, status/backlog

### Ownership
- Primary: @assignee

### Tasks
- [ ] Provisioning & config
- [ ] Observability (metrics/logs/traces)
- [ ] SLOs/alerts
- [ ] Backup/restore

### Acceptance Criteria
- [ ] Meets latency + reliability SLOs
```

**Template C — Experiment/Interleaving**
```
Title: [EXP] Interleaving: <A vs B>
Labels: kind/research, area/eval, priority/P2, status/backlog

### Hypothesis

### Design
- Interleaving: Team‑Draft, sample size N

### Metrics
- CTR@1/3/10, win‑rate, p‑value

### Rollout Plan
- [ ] Flag gating
- [ ] Rollback criteria
```

---

## Dependency Map (example)
- M0 → M1 → (M2, M3) → M4 → (M5, M6) → M7 → M8 → M9

---

## Definition of Done (Epic)
- All milestone acceptance criteria met for any language worldwide.
- Dashboards show SLO compliance for 14 consecutive days across all supported languages.
- Interleaving confirms ≥ 10% CTR@Top‑3 lift vs. BM25-only baseline for multilingual queries.
- Runbooks exercised; one full rollback drill passed for universal multilingual system.

