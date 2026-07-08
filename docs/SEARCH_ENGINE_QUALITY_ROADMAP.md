# Search Engine Quality Roadmap

## From Foundation to Top Search Engine Parity

> **Document Purpose:** A comprehensive, step-by-step roadmap showing exactly what has been built, what needs to be built, how each step changes the user experience, what problems each step solves, and how close we get to Top Search Engine quality at every stage.
>
> **Audience:** Team, investors, product managers, and technical stakeholders.
>
> **Last Updated:** July 9, 2026

> **Current Position:** M0 foundation modules are partly implemented in `modules/`, but production C++ integration (task 01.6) is not yet proven. The live user-facing search path is currently closer to a RedisSearch lexical retrieval baseline than to the fully integrated M0 pipeline (~18–24% for Persian/Iranian market today).

---

## Table of Contents

1. [Quality Score Model](#quality-score-model)
2. [Current State — What's Already Built](#current-state--whats-already-built)
3. [Step-by-Step Milestone Roadmap](#step-by-step-milestone-roadmap)
   - [M0 — Foundation (Text Processing)](#m0--foundation-text-processing) ← **You Are Here**
   - [M1 — Core Retrieval Baseline](#m1--core-retrieval-baseline)
   - [M1.5 — Search Quality Evaluation Gates](#m15--search-quality-evaluation-gates)
   - [M2 — Authority Signals & Content Understanding](#m2--authority-signals--content-understanding)
   - [M5 — Spam & Quality Filtering](#m5--spam--quality-filtering)
   - [M3 — Semantic Understanding](#m3--semantic-understanding)
   - [M4 — Intent & Vertical Classification](#m4--intent--vertical-classification)
   - [M6 — Ranking Fusion & Query Pipeline](#m6--ranking-fusion--query-pipeline)
   - [M7 — Metrics, Dashboards & Experimentation](#m7--metrics-dashboards--experimentation)
   - [M8 — Click Modeling & Learning-to-Rank](#m8--click-modeling--learning-to-rank)
   - [M9 — Production Operations & SRE](#m9--production-operations--sre)
4. [Iran/Persian Market — Advertising Readiness](#iranpersian-market--advertising-readiness)
5. [Gaps, Missing Milestones & Risks](#gaps-missing-milestones--risks)
6. [Summary Table](#summary-table)

---

## Quality Score Model

### How We Measure Quality

Quality is measured relative to a mature Top Search Engine (Google, Bing) on **Persian/Iranian market queries**. We use two dimensions:

| Dimension                  | Description                                                                                                               |
| -------------------------- | ------------------------------------------------------------------------------------------------------------------------- |
| **Iran/Persian Quality %** | Estimated relevance and usefulness for Persian-language queries, `.ir` domains, Iranian entities, and Iranian user intent |
| **Global Quality %**       | Quality across all languages and topics                                                                                   |

**Why two dimensions?** A search engine optimized for Iranian content can reach a higher percentage of Top Search Engine quality in its target market than it can globally. This is the correct metric to optimize and advertise.

### Important Caveat

The percentages below are **engineering estimates until M1.5 creates a real evaluation set** with judged queries, NDCG/MRR/MAP measurements, freshness checks, spam checks, and repeated regression reports. They should be read as planning ranges, not as certified quality measurements.

### Estimated Quality by Milestone

| Stage               | Iran/Persian % | Representative |       Global % | Representative | Claim Strength                                                |
| ------------------- | -------------: | -------------: | -------------: | -------------: | ------------------------------------------------------------- |
| Current live system |         18–24% |           ~21% |         12–16% |           ~14% | Internal demo only                                            |
| M0 fully integrated |         26–32% |           ~29% |         18–22% |           ~20% | Persian-aware foundation                                      |
| M1 retrieval        |         34–40% |           ~37% |         24–30% |           ~27% | Usable retrieval beta                                         |
| M1.5 evaluation     | no direct gain |              — | no direct gain |              — | Quality becomes measurable                                    |
| M2 authority        |         43–50% |           ~47% |         30–36% |           ~33% | Authority-ranked beta                                         |
| M5 spam quality     |         48–55% |           ~52% |         34–40% |           ~37% | Safer public beta                                             |
| M3 semantic         |         54–61% |           ~58% |         39–45% |           ~42% | Better recall, typos, synonyms                                |
| M4 intent           |         58–64% |           ~61% |         42–48% |           ~45% | Better navigational and commercial queries                    |
| M6 ranking fusion   |         63–70% |           ~67% |         47–53% |           ~50% | Credible focused public launch (if evaluation gates pass)     |
| M7 metrics          | no direct gain |              — | no direct gain |              — | Safer iteration                                               |
| M8 click/LTR        |         68–74% |           ~71% |         50–56% |           ~53% | Learns from real usage                                        |
| M9 production       |         70–76% |           ~73% |         52–58% |           ~55% | Reliable national product for covered Persian/Iranian content |

> **Note:** All estimates are based on information retrieval research benchmarks (NDCG, MRR, MAP) applied to the Iranian web corpus. They will be replaced by measured values after M1.5 delivers a real judged query set.

> **Scope of the high-end figures:** From M6 onward, the Iran/Persian percentages refer specifically to **covered** Persian/Iranian _text_ content — pages actually crawled and indexed — not the entire Persian web. All figures in this document are engineering planning estimates until M1.5 produces measured results, and should not be used in external/public claims before then. Read `~X%` as "an estimated X% of the relevance a mature Top Search Engine would deliver on the same query set," not as an absolute accuracy score.

---

## Current State — What's Already Built

The repository contains substantial C++ search infrastructure and several M0 Python modules. The important distinction is that **module completion is not the same as user-facing production integration**.

### C++ Core Infrastructure (Always Active)

| Component                 | Status        | What It Does                                                          |
| ------------------------- | ------------- | --------------------------------------------------------------------- |
| **HTTP Server**           | ✅ Production | uWebSockets-based, handles thousands of concurrent requests           |
| **Basic BM25 Search**     | ✅ Active     | RedisSearch full-text retrieval — keyword matching                    |
| **SPA Web Crawler**       | ✅ Production | Crawls React/Vue/Angular single-page applications via headless Chrome |
| **Robots.txt Compliance** | ✅ Active     | Respects crawl exclusion rules                                        |
| **MongoDB Storage**       | ✅ Production | Stores all crawled documents, profiles, analytics                     |
| **Redis Cache**           | ✅ Production | Query caching, session data, stopword lookups                         |
| **Kafka Crawler Queue**   | ✅ Active     | Durable, restartable URL frontier                                     |
| **API Rate Limiting**     | ✅ Active     | Prevents API abuse                                                    |
| **Pulse Analytics**       | ✅ Active     | Public search trend page at `/نبض`                                    |
| **Profile System**        | ✅ Active     | Personal profiles with link blocks and click tracking                 |
| **JS Minification**       | ✅ Active     | 99.6% faster JS serving via Node.js Terser + Redis                    |
| **Grafana Monitoring**    | ✅ Active     | Infrastructure metrics dashboards                                     |

### Current Live Search Reality

The live C++ search path currently includes some **M1-lite** behavior:

- The **web search** path (`/search`, `/search/more`) uses `RedisSearchStorage::search()` with multi-tier title/content/fallback retrieval, score boosts, and URL deduplication when `REDIS_SEARCH_ENABLED=true`.
- The **API search** path (`/api/search`) calls `SearchClient::search()` directly — a simpler, less tiered retrieval path.
- RedisSearch field weighting exists in the index schema for URL, title, content, and description.
- Redis-backed search uses title/content/description tiering and score boosts before sorting results.
- A simple hardcoded Persian/English stopword list is used during query token filtering.
- Duplicate URLs are removed while assembling combined tiered results.
- Snippets are simple description/content excerpts, not full query-aware RTL-safe snippets.

The live path does **not** yet prove:

- Full Unicode/Persian normalization from the M0 Python module at query and index time.
- Language detection and language-aware query routing in production search.
- IDF/Redis-backed dynamic stopword detection from the M0 module.
- Persian morphological analysis.
- Semantic embeddings, authority ranking, spam models, freshness ranking, or click-based learning.

### What This Baseline Gives Users

At the current live stage, a Persian user gets lexical search with some useful field weighting, but not a fully Persian-aware retrieval pipeline:

- Gets results for **exact keyword matches only** (field-weighted, but still lexical)
- Persian text normalization is not yet proven at production query/index time
- `کتاب` and `كتاب` (Arabic vs Persian letters) may return different results ← **Bug**
- No proven language-aware query routing — Persian and English not reliably differentiated
- No authority signals — spam and low-quality pages rank equally with good content

**Estimated current quality: 18–24% for Persian/Iranian market | 12–16% globally**

### Reusable Assets Already in the Tree

Some capabilities the roadmap treats as "future" already exist in the codebase but are **not yet wired into the live search path**. Reusing them shortens several milestones:

| Asset                    | Location                                               | What It Already Does                                                                                                                                         | Where It's Used Today  | Reuse Opportunity                                                                              |
| ------------------------ | ------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------ | ---------------------- | ---------------------------------------------------------------------------------------------- |
| **PulseQueryNormalizer** | `src/pulse/PulseQueryNormalizer.cpp`                   | Arabic→Persian character folding (`ي→ی`, `ك→ک`, `ة→ه`), Persian/Arabic digit normalization, punctuation/whitespace cleanup, lightweight `estimateLanguage()` | Pulse analytics only   | Wire into `RedisSearchStorage::search()` at query + index time (part of M0/01.1, M6/09.1–09.2) |
| **UrlCanonicalizer**     | `src/common/UrlCanonicalizer.cpp`                      | Canonical URL, `www`/default-port stripping, path/query normalization, punycode                                                                              | Crawler frontier dedup | Feeds M1 task 02.4 (URL deduplication) — already partially built at crawl time                 |
| **SearchScorer**         | `src/scoring/SearchScorer.cpp` + `config/scoring.json` | Configurable BM25F-style scoring: field weights, TF params, length + score normalization                                                                     | Scoring library        | Basis for M1 task 02.1 (BM25F) instead of a from-scratch build                                 |

> **Implication:** The gap to M0/M1 is smaller than "nothing built yet." Several tasks are integration-and-wiring work on existing components rather than greenfield implementation.

### Quick Wins (Pre-M0) — Fixable Now Without Full Integration

The document elsewhere describes the `کتاب` vs `كتاب` mismatch as a bug blocked on the full M0 Python/pybind11 integration (task 01.6). It is not fully blocked: a native C++ normalizer already exists.

| Quick Win                                                                                          |  Effort | Fixes                                                           | Blocked By                           |
| -------------------------------------------------------------------------------------------------- | ------: | --------------------------------------------------------------- | ------------------------------------ |
| Call `PulseQueryNormalizer::normalize()` on search queries **and** on indexed title/content fields |     Low | `کتاب`/`كتاب`, Arabic/Persian digit variants, mixed punctuation | Nothing — component exists           |
| Route `/api/search` through the same tiered `RedisSearchStorage::search()` used by `/search`       | Low–Med | Inconsistent results between API and web search paths           | Nothing — see M1 "Route unification" |
| Harden `escapeRedisString()` against RedisSearch query-syntax injection                            |     Low | Malformed/adversarial query crashes and injection               | Nothing                              |

> **Note:** These are stopgaps, not replacements for the full M0 pipeline (NFKC, morphology, IDF-driven stopwords). They deliver a small immediate quality bump and de-risk M0 by proving the query/index wiring before the Python integration lands.

---

## Step-by-Step Milestone Roadmap

---

### M0 — Foundation (Text Processing)

> **Status:** Modules implemented in `modules/M0-foundation/01-text-processing/`; production C++ integration (task 01.6) still pending. M0 is the required foundation but is not yet the live production path.

#### What Was Built

| Task                                                             | Status        | Key Metric                                                            |
| ---------------------------------------------------------------- | ------------- | --------------------------------------------------------------------- |
| **01.1** Unicode NFKC Normalization                              | ✅ Complete   | 11,271 docs/sec, 92% test coverage                                    |
| **01.2** Language Detection (176 languages)                      | ✅ Complete   | 96% accuracy, 8,000 detections/sec                                    |
| **01.3** Script-Specific Processing (Arabic ZWNJ, CJK, Cyrillic) | 🔵 ~70%       | Arabic/Persian ZWNJ handling ready                                    |
| **01.4** Stopword & IDF Analysis (100+ languages)                | ✅ Complete   | <1ms Redis lookup, 12,000 docs/sec                                    |
| **01.5** Nightly Batch Pipeline                                  | ⚠️ Unverified | Cache artifacts found; no source code or test evidence confirmed      |
| **01.6** C++ Integration (pybind11)                              | 🔵 ~30%       | Wrapper planned                                                       |
| **01.7** Persian Morphological Analysis                          | ⏳ Planned    | Atomic task documented; no implemented module found in `modules/` yet |

#### How Users Search Before M0 (Baseline)

> **User searches:** `خرید لپ‌تاپ`
>
> **What happens:** The system looks for the exact byte sequence "خرید لپ‌تاپ". The zero-width non-joiner (ZWNJ) in `لپ‌تاپ` means a page writing it without ZWNJ (`لپتاپ`) is completely missed. Arabic-form letters (`ك` instead of `ک`) produce zero results.
>
> **User experience:** Missing 30-40% of relevant results due to encoding variants. Frustrating for Persian speakers.

#### How Users Search After M0 (Once Fully Integrated)

> **User searches:** `خرید لپ‌تاپ` OR `خريد لپتاپ` OR `خرید Laptop`
>
> **What happens:**
>
> 1. Unicode NFKC normalization converts all Arabic/Persian variants to a canonical form
> 2. ZWNJ characters are handled correctly in script-specific processing
> 3. Language detection identifies the query as Persian (98% confidence)
> 4. Stopwords like `و`, `در`, `از` are automatically detected via IDF and excluded
> 5. All three query variants now retrieve the same result set
>
> **User experience:** Results are consistent regardless of how the user types Persian. No more "no results" for encoding variants.

#### Problems This Step Solves

- ❌ → ✅ Arabic-letter vs. Persian-letter variants (`ي`/`ی`, `ك`/`ک`) now match correctly
- ❌ → ✅ ZWNJ presence/absence no longer splits result sets
- ❌ → ✅ Persian stopwords (`و`, `در`, `به`, `از`, `که`) no longer dominate ranking
- ❌ → ✅ Language of each document and query is known — enabling language-aware routing
- ❌ → ✅ CJK (Chinese/Japanese/Korean) queries are word-segmented correctly
- ❌ → ✅ Mixed Persian-English queries are handled (language per token)

#### Quality Impact

| Metric                             | Before M0  | After M0   | Gain            |
| ---------------------------------- | ---------- | ---------- | --------------- |
| Persian encoding coverage          | ~65%       | ~98%       | +33%            |
| Stopword contamination             | High       | Eliminated | Significant     |
| Language routing accuracy          | 0%         | 96%        | +96%            |
| **Estimated Iran/Persian quality** | **18–24%** | **26–32%** | **+6–8 points** |
| **Estimated global quality**       | **12–16%** | **18–22%** | **+4–6 points** |

---

### M1 — Core Retrieval Baseline

> **Status: ⏳ Pending** (some M1-lite field-weighting behavior is already present in the live Redis search path, but the full milestone is not yet implemented)
> **Task files:** `.github/ISSUE_TEMPLATE/atomic-tasks/M1-retrieval/02-core-retrieval/`

#### What Needs to Be Built

| Task                  | Description                                                                                                                                 | Duration |
| --------------------- | ------------------------------------------------------------------------------------------------------------------------------------------- | -------- |
| **02.1**              | BM25/BM25F with field weights (title × 3.0, h1 × 2.5, anchors × 2.0, body × 1.0), phrase/exact-match bonuses, and morphology-aware matching | 4 days   |
| **02.2**              | Character n-gram index (3–5 grams) for fallback retrieval                                                                                   | 3 days   |
| **02.3**              | Universal index schema (language, script, timestamp, schema_type metadata)                                                                  | 5 days   |
| **02.4**              | URL deduplication (canonical URL, HTTPS preference, tracking param stripping)                                                               | 4 days   |
| **02.5**              | Quality gate (HTTPS signal, mobile viewport, 100+ word minimum)                                                                             | 3 days   |
| **02.6**              | Performance optimization (P95 latency ≤80ms for BM25 top-200)                                                                               | 4 days   |
| **02.12**             | Query-aware snippet generation (RTL-safe, XSS-safe, morphology-aware)                                                                       | 4 days   |
| **Route unification** | `/api/search` and `/search` must use the same retrieval and ranking logic (or differ only intentionally and documented)                     | ongoing  |

#### How Users Search Before M1 (M0 only)

> **User searches:** `آموزش برنامه‌نویسی پایتون`
>
> **What happens:** BM25 finds documents containing those exact tokens. A page titled "آموزش پایتون" with the words in the body scores the same as a page with the full phrase in the title. Duplicate pages (same content, different URLs) all appear. Snippets are raw text excerpts with no highlighting.
>
> **User experience:** Irrelevant results mixed with relevant ones. Multiple copies of the same page. Unhelpful snippets.

#### How Users Search After M1

> **User searches:** `آموزش برنامه‌نویسی پایتون`
>
> **What happens:**
>
> 1. Title matches score 3× higher than body matches — "آموزش پایتون" in title ranks above buried body mentions
> 2. Near-duplicate pages (same content, slightly different URLs) are collapsed to one canonical result
> 3. Pages without mobile support, HTTPS, or with fewer than 100 words are filtered out
> 4. Snippet shows the query terms **highlighted in context**, RTL-aligned for Persian text
> 5. N-gram fallback handles typos and rare words with no exact match
>
> **User experience:** Top result is almost always the most relevant page. Clean, non-duplicate result list. Snippets show exactly why the page matches.

#### Problems This Step Solves

- ❌ → ✅ Title relevance matters more than body — better precision
- ❌ → ✅ Duplicate pages collapsed — cleaner result list
- ❌ → ✅ Low-quality pages (no HTTPS, mobile, thin content) removed
- ❌ → ✅ RTL-correct, highlighted snippets — users see relevance at a glance
- ❌ → ✅ N-gram fallback means no "zero results" for near-typos
- ❌ → ✅ P95 latency under 80ms — search feels instant

#### Quality Impact

| Metric                             | Before M1  | After M1   | Gain            |
| ---------------------------------- | ---------- | ---------- | --------------- |
| Precision@10 (Iran queries)        | ~40%       | ~60%       | +20%            |
| Duplicate results in top-10        | ~25%       | <5%        | -80%            |
| Thin/spam pages in top-10          | ~30%       | <10%       | -67%            |
| Snippet relevance                  | Poor       | Good       | Significant     |
| **Estimated Iran/Persian quality** | **26–32%** | **34–40%** | **+6–8 points** |
| **Estimated global quality**       | **18–22%** | **24–30%** | **+5–7 points** |

---

### M1.5 — Search Quality Evaluation Gates

> **Status: ⏳ Pending — CRITICAL GATING MILESTONE**
> **Task files:** `.github/ISSUE_TEMPLATE/atomic-tasks/M1.5-evaluation/`

> **Why this milestone exists:** Without measurement, it is impossible to know if M2, M3, M5, or M6 changes actually improve quality — or silently break it. This milestone installs the quality measurement system that gates all future work.

#### What Needs to Be Built

| Task      | Description                                              | Duration |
| --------- | -------------------------------------------------------- | -------- |
| **02.7**  | Versioned query set with human relevance judgments       | 4 days   |
| **02.8**  | Offline metrics harness (NDCG@10, MRR, MAP, precision@k) | 4 days   |
| **02.9**  | Persian/Iran-specific relevance test suite               | 4 days   |
| **02.10** | Crawl freshness baseline (stale result detection)        | 4 days   |
| **02.11** | CI regression gates — blocks merges that reduce quality  | 4 days   |

#### How This Changes the Engineering Process

> **Before M1.5:** A developer implements a new ranking feature. It looks good in manual tests. It ships. Three weeks later, navigational queries break silently.
>
> **After M1.5:** Every pull request that touches ranking runs the offline metrics harness automatically. A regression in NDCG@10 by >1% blocks the merge. The team has a shared, objective truth about quality.

#### Problems This Step Solves

- ❌ → ✅ Quality regressions detected before shipping, not after users complain
- ❌ → ✅ Objective measurement of "is this better?" — ends subjective debates
- ❌ → ✅ Persian/Iran-specific query coverage — not just English benchmarks
- ❌ → ✅ Freshness tracking — know when index is going stale
- ❌ → ✅ Foundation for all future A/B testing and LTR

#### Quality Impact

> M1.5 does not directly improve result quality — it makes quality **measurable, defensible, and safer to improve**. It is required before any strong public quality claims can be made.

| Metric                        | Impact                                           |
| ----------------------------- | ------------------------------------------------ |
| Iran/Persian quality estimate | No direct gain; protects and enables later gains |
| Global quality estimate       | No direct gain; protects and enables later gains |
| Launch value                  | **Required** before strong public quality claims |

#### Minimum Gates Before Any Public Launch Claim

| Gate                                               |                                                                  Minimum Target |
| -------------------------------------------------- | ------------------------------------------------------------------------------: |
| NDCG@10 vs Top SE sample on Persian/Iran benchmark |                                     ≥70% for first claim; ≥75% for strong claim |
| Navigational success@1                             |                                                                            ≥90% |
| Duplicate URLs/content in top 10                   |                                                                             <5% |
| Spam/thin/adversarial pages in top 10              |                                                                             <3% |
| Freshness for news and current-event queries       |                                Top results crawled or validated within 24 hours |
| Zero-result rate for common Persian queries        |                                                                             <5% |
| P95 search latency (user-facing web)               |                                                                          <300ms |
| Query-aware snippets present and safe              |                                                            ≥80% of result pages |
| Index coverage                                     | Top trusted Iranian domains covered by category, with published coverage report |
| Regression gate                                    |                        Ranking PRs blocked if key metrics drop beyond threshold |

---

### M2 — Authority Signals & Content Understanding

> **Status: ⏳ Pending**
> **Task files:** `.github/ISSUE_TEMPLATE/atomic-tasks/M2-content-understanding/`

> **⚠️ Largest single projected jump:** M2 is the biggest estimated step in the roadmap (+9–10 Persian points). Moving from purely lexical retrieval to authority-aware ranking is historically one of the largest quality gains in IR, but the size of this gain depends entirely on crawl coverage and the density of the Iranian link graph. Treat the upper end of this range as optimistic until M1.5 measures it.

#### What Needs to Be Built

**Link Graph & Authority (24 days)**

| Task     | Description                                                                                  |
| -------- | -------------------------------------------------------------------------------------------- |
| **03.1** | Extract all links from crawled documents                                                     |
| **03.2** | Build host-level link graph (aggregate URL graph to domain level)                            |
| **03.3** | Compute PageRank (damping factor 0.85) — authority score per domain                          |
| **03.4** | Store host-level signals in feature store (PageRank, link count, anchor text)                |
| **03.5** | Incremental weekly updates to the link graph                                                 |
| **03.6** | **TrustRank** with 50+ Iranian seed domains: `AuthorityScore = 0.6×PageRank + 0.4×TrustRank` |

**Structured Data Extraction (16 days)**

| Task     | Description                                                                        |
| -------- | ---------------------------------------------------------------------------------- |
| **04.1** | Schema.org parser (Book, Product, Article, Event, Organization)                    |
| **04.2** | ISBN and price extraction from Iranian e-commerce sites                            |
| **04.3** | Structured boost signals (schema-annotated pages rank higher for relevant queries) |
| **04.4** | Validation and testing suite                                                       |

#### How Users Search Before M2

> **User searches:** `خرید موبایل سامسونگ`
>
> **What happens:** A well-optimized spam site with keyword stuffing ranks above Digikala (Iran's largest e-commerce platform) because BM25 cannot distinguish authority. Schema.org product data from legitimate retailers is ignored.
>
> **User experience:** Spam, thin, and low-authority sites crowd the top results. Trusted Iranian retailers are buried.

#### How Users Search After M2

> **User searches:** `خرید موبایل سامسونگ`
>
> **What happens:**
>
> 1. Digikala, Snapp Shop, and trusted `.ir` retailers have high TrustRank (seeded from known trusted Iranian domains)
> 2. Spam link farms are penalized — their PageRank passes through spam, so TrustRank is low
> 3. Product schema data surfaces price, rating, and availability directly in the snippet
> 4. Iranian news sites (IRNA, Tasnim) rank authoritatively for news queries about Samsung
>
> **User experience:** Trusted Iranian sites appear at the top. Prices and ratings visible in results. Spam dramatically reduced.

#### Iranian-Specific Authority Seeds (Task 03.6)

The TrustRank seed set includes 50+ trusted Iranian domains across categories:

| Category     | Examples                                       |
| ------------ | ---------------------------------------------- |
| News & Media | IRNA, Tasnim, Mehr News, ISNA, Farsnews        |
| E-commerce   | Digikala, Snapp Shop, Basalam, Bamilo          |
| Government   | `.ir` official portals, government ministries  |
| Education    | Top Iranian universities (Sharif, Tehran, AUT) |
| Encyclopedia | WikiFarsi, Daneshnameh                         |

#### Problems This Step Solves

- ❌ → ✅ Spam and link farms demoted — trusted Iranian sites rise
- ❌ → ✅ Authority signals differentiate between `digikala.com` and a fake clone
- ❌ → ✅ Product price/rating/availability shown in search snippets
- ❌ → ✅ Book searches show ISBN, author, and publisher
- ❌ → ✅ Link-spam manipulation largely neutralized for Iranian web graph

#### Quality Impact

| Metric                             | Before M2  | After M2               | Gain             |
| ---------------------------------- | ---------- | ---------------------- | ---------------- |
| Spam in top-10                     | ~25%       | <8%                    | -68%             |
| Trusted Iranian site ranking       | Poor       | Good                   | Significant      |
| Structured data in snippets        | 0%         | ~40% of eligible pages | +40%             |
| **Estimated Iran/Persian quality** | **34–40%** | **43–50%**             | **+9–10 points** |
| **Estimated global quality**       | **24–30%** | **30–36%**             | **+6 points**    |

---

### M5 — Spam & Quality Filtering

> **Status: ⏳ Pending** (implemented before M3/M4 because spam harms signal quality)
> **Task files:** `.github/ISSUE_TEMPLATE/atomic-tasks/M5-quality/`

#### What Needs to Be Built

| Task     | Description                                                                        | Duration |
| -------- | ---------------------------------------------------------------------------------- | -------- |
| **07.1** | Spam feature extraction (text patterns, link patterns, structural features)        | 4 days   |
| **07.2** | One-Class SVM training (unsupervised — learns "normal" page fingerprint)           | 5 days   |
| **07.3** | Isolation Forest complementary model                                               | 4 days   |
| **07.4** | Site-level reputation aggregation (domain-level spam score)                        | 4 days   |
| **07.5** | Quality scoring pipeline (integrates `AdversarialSEOScore` into ranking)           | 3 days   |
| **07.6** | **Adversarial SEO detection** (keyword stuffing, cloaking, doorways, thin content) | 4 days   |

#### Persian-Specific Spam Patterns Detected (Task 07.6)

Iranian web spam has distinct patterns that this task specifically targets:

| Spam Pattern                | Description                                                      | Prevalence  |
| --------------------------- | ---------------------------------------------------------------- | ----------- |
| Hidden Persian keyword divs | `<div style="display:none">` filled with Persian keywords        | Very common |
| Translated doorway pages    | Low-quality machine-translated pages targeting Persian queries   | Common      |
| Footer keyword blocks       | Hundreds of keywords stuffed in footer, same color as background | Common      |
| Persian cloaking            | Shows Google's crawler a quality page, serves users spam         | Moderate    |
| Link scheme farms           | Networks of `.ir` sites linking to each other artificially       | Common      |

#### How Users Search Before M5

> **User searches:** `بهترین وکیل تهران`
>
> **What happens:** SEO-spammed sites with hundreds of hidden Persian keywords rank above actual law firms. Pages with invisible text (`color: white; background: white`) containing "وکیل تهران" repeated 500 times appear in top results.
>
> **User experience:** Top results are spam, not actual lawyers. Users must scroll through fake directories to find real services.

#### How Users Search After M5

> **User searches:** `بهترین وکیل تهران`
>
> **What happens:**
>
> 1. Pages with keyword-stuffed hidden divs receive `AdversarialSEOScore` penalty
> 2. Domain-level spam score aggregates page spam across entire sites — chronic offenders get sitewide demotion
> 3. Cloaking detection compares crawled content vs. rendered content — mismatches trigger penalty
> 4. Thin content filter removes pages with fewer than 100 meaningful words after stopword removal
> 5. Actual law firm websites with structured content, real contact info, and legitimate link profiles rank at top
>
> **User experience:** Top results are real law firms, professional directories, and review sites. Spam pages disappear from visible results.

#### Problems This Step Solves

- ❌ → ✅ Keyword stuffing no longer rewarded — penalized instead
- ❌ → ✅ Hidden div spam (Persian-specific pattern) detected and demoted
- ❌ → ✅ Cloaking detected — users see the same content Google sees
- ❌ → ✅ Thin/doorway pages removed from index
- ❌ → ✅ Domain-level penalties hit entire spammy networks, not just individual pages
- ❌ → ✅ False-positive review protects legitimate `.gov.ir`, `.ac.ir`, medical, legal, and small local sites from incorrect penalization

#### Quality Impact

| Metric                                      | Before M5  | After M5   | Gain            |
| ------------------------------------------- | ---------- | ---------- | --------------- |
| Spam pages in top-10                        | ~8%        | <3%        | -62%            |
| Keyword-stuffed pages visible               | ~15%       | <2%        | -87%            |
| False positive (legitimate sites penalized) | —          | <2%        | Required        |
| **Estimated Iran/Persian quality**          | **43–50%** | **48–55%** | **+4–5 points** |
| **Estimated global quality**                | **30–36%** | **34–40%** | **+3–4 points** |

---

### M3 — Semantic Understanding

> **Status: ⏳ Pending**
> **Task files:** `.github/ISSUE_TEMPLATE/atomic-tasks/M3-semantic/`

#### What Needs to Be Built

| Task     | Description                                                                | Duration |
| -------- | -------------------------------------------------------------------------- | -------- |
| **05.1** | Co-occurrence matrix from title/body/anchor text (window=5–10 words)       | 5 days   |
| **05.2** | PPMI/SVD training — dense word vectors from co-occurrence data             | 5 days   |
| **05.3** | Subword (fastText-style) embeddings — handles morphologically rich Persian | 4 days   |
| **05.4** | Embedding inference service (HTTP/gRPC, <10ms/doc)                         | 4 days   |
| **05.5** | Document embedding precomputation (batch-embed the corpus)                 | 4 days   |
| **05.6** | Character n-gram spell correction models                                   | 4 days   |
| **05.7** | Nightly lexicon export (`lexicon.json` with synonyms and corrections)      | 4 days   |

> **Note:** Task IDs match the files under `.github/ISSUE_TEMPLATE/atomic-tasks/M3-semantic/05-embeddings/`. Embedding _similarity/semantic validation_ is covered by the test suites within each task rather than a standalone `05.x` file.

#### Why Semantic Understanding Matters for Persian

Persian is a morphologically rich language. Without semantic understanding:

| User Searches             | Without M3                           | With M3                              |
| ------------------------- | ------------------------------------ | ------------------------------------ |
| `نوشتن` (to write)        | Misses pages about `نوشته` (written) | Understands they are related         |
| `کتاب‌های علمی`           | Misses `علم`, `علوم`, `دانش`         | Expands to related concepts          |
| `رستوران ایتالیایی تهران` | Exact match only                     | Finds `پیتزا`, `پاستا`, `ایتالیا`    |
| `گوشی اپل` (Apple phone)  | Misses `آیفون`, `iPhone`             | Connects Persian/English brand terms |

#### How Users Search Before M3

> **User searches:** `درمان سردرد` (headache treatment)
>
> **What happens:** Only pages containing the exact word `درمان` AND `سردرد` are returned. A highly relevant medical article using `درمان میگرن` (migraine treatment) or `علاج سردرد` (remedy for headache) is missed entirely. A typo like `سرددر` returns zero results.
>
> **User experience:** Search is brittle. Slightly different vocabulary, synonyms, or typos produce zero results or irrelevant results.

#### How Users Search After M3

> **User searches:** `درمان سردرد`
>
> **What happens:**
>
> 1. Embedding model knows `درمان` is semantically close to `علاج`, `مداوا`, `بهبود`
> 2. Query is expanded with related terms from `lexicon.json`
> 3. Spell correction suggests `سردرد` if user typed `سرددر`
> 4. Semantic similarity score adds to BM25 — pages about migraine treatment rank for headache queries
> 5. Multi-lingual connection: `headache` in mixed queries connects to Persian results
>
> **User experience:** Search understands meaning, not just words. Synonyms, related concepts, and typo corrections work seamlessly.

#### Problems This Step Solves

- ❌ → ✅ Synonyms and related terms found — higher recall
- ❌ → ✅ Persian morphological variants connected via subword embeddings
- ❌ → ✅ Spell correction covers 95%+ of corpus vocabulary
- ❌ → ✅ Mixed Persian-English queries handled semantically
- ❌ → ✅ "Zero results" for typos virtually eliminated

#### Quality Impact

| Metric                             | Before M3  | After M3   | Gain            |
| ---------------------------------- | ---------- | ---------- | --------------- |
| Recall@100 (Persian queries)       | ~55%       | ~75%       | +20%            |
| Typo recovery rate                 | ~10%       | ~85%       | +75%            |
| Synonym query coverage             | 0%         | ~70%       | +70%            |
| **Estimated Iran/Persian quality** | **48–55%** | **54–61%** | **+5–6 points** |
| **Estimated global quality**       | **34–40%** | **39–45%** | **+4–5 points** |

---

### M4 — Intent & Vertical Classification

> **Status: ⏳ Pending**
> **Task files:** `.github/ISSUE_TEMPLATE/atomic-tasks/M4-intent/`

#### What Needs to Be Built

| Task     | Description                                                            | Duration |
| -------- | ---------------------------------------------------------------------- | -------- |
| **06.1** | Weak-label generation (transactional/navigational/informational seeds) | 4 days   |
| **06.2** | Intent classifier training (logistic regression / GBDT)                | 5 days   |
| **06.3** | Vertical detectors: Book, Product, Article, News, Download             | 5 days   |
| **06.4** | Real-time query intent inference (<5ms)                                | 4 days   |
| **06.5** | Evaluation: confusion matrices, Persian/Iran coverage                  | 3 days   |

#### The Three Intent Types

| Intent             | Example Queries                            | Best Result Type                       |
| ------------------ | ------------------------------------------ | -------------------------------------- |
| **Informational**  | `تاریخ ایران`, `چگونه پایتون یاد بگیرم`    | Wikipedia, tutorials, articles         |
| **Navigational**   | `دیجیکالا`, `بانک ملی ورود`, `ایمیل یاهو`  | Direct link to the site/page           |
| **Transactional**  | `خرید لپ‌تاپ`, `قیمت آیفون ۱۵`             | E-commerce listings, price comparisons |
| **Local**          | `رستوران نزدیک من`, `دکتر متخصص تهران`     | Local directories, maps, service pages |
| **News/Freshness** | `نتیجه بازی پرسپولیس امروز`, `اخبار امروز` | Current news pages, sports results     |

#### How Users Search Before M4

> **User searches:** `دیجیکالا` (navigational — user wants the Digikala website)
>
> **What happens:** BM25 returns articles _about_ Digikala (news, reviews, Wikipedia) before the actual `digikala.com` homepage, because articles have more keyword-rich text.
>
> **User experience:** For navigational queries (very common), users have to scroll past articles to find the actual website they want. Frustrating.

> **User searches:** `خرید آیفون ۱۵` (transactional)
>
> **What happens:** Returns a mix of reviews, news articles, and product listings — no distinction. The "buy" intent is ignored.
>
> **User experience:** Users looking to purchase see irrelevant reviews before product listings.

#### How Users Search After M4

> **User searches:** `دیجیکالا`
>
> **What happens:** Navigational intent detected with 98% confidence. `digikala.com` homepage is pinned to top-1 result. Intent-boosted ranking overrides BM25 for navigational queries.
>
> **User experience:** Navigational searches always land on the right site in position 1.

> **User searches:** `خرید آیفون ۱۵`
>
> **What happens:** Transactional intent + Product vertical detected. Results are re-ranked to surface product listings with prices, ratings, and "buy" links first. Informational articles moved to positions 6–10.
>
> **User experience:** First 5 results are places to buy the product. Users can compare prices directly from the search page.

#### Problems This Step Solves

- ❌ → ✅ Navigational queries go directly to the target site — no wrong page at #1
- ❌ → ✅ Product/transactional queries surface purchase options, not just reviews
- ❌ → ✅ Informational queries surface encyclopedias and tutorials, not ads
- ❌ → ✅ Book searches show library/purchase options with ISBN
- ❌ → ✅ Vertical detection enables future specialized result layouts (product cards, news carousels)

#### Quality Impact

| Metric                             | Before M4  | After M4           | Gain            |
| ---------------------------------- | ---------- | ------------------ | --------------- |
| Navigational success@Top-1         | ~60%       | ≥95%               | +35%            |
| Transactional result quality       | ~40%       | ~75%               | +35%            |
| NDCG@5 improvement                 | —          | +8% vs M3 baseline | Measured        |
| **Estimated Iran/Persian quality** | **54–61%** | **58–64%**         | **+3–4 points** |
| **Estimated global quality**       | **39–45%** | **42–48%**         | **+2–3 points** |

---

### M6 — Ranking Fusion & Query Pipeline

> **Status: ⏳ Pending — Largest single milestone**
> **Task files:** `.github/ISSUE_TEMPLATE/atomic-tasks/M6-ranking/`

#### What Needs to Be Built

**Ranking Fusion (21 days)**

| Task     | Description                                                                                           |
| -------- | ----------------------------------------------------------------------------------------------------- |
| **08.1** | Feature fusion implementation — `FinalScore` formula assembling all signals with configurable weights |
| **08.2** | MMR diversification (≤3 results per domain in Top-10)                                                 |
| **08.3** | Parameter optimization using M1.5 metrics as objective                                                |
| **08.4** | Integration testing of the fused ranker                                                               |
| **08.5** | **Freshness ranking signal** — temporal intent + adaptive decay                                       |

> **Note:** Task IDs match the files under `.github/ISSUE_TEMPLATE/atomic-tasks/M6-ranking/08-ranking-fusion/`.

**Query Pipeline (34 days)**

| Task     | Description                                                        |
| -------- | ------------------------------------------------------------------ |
| **09.1** | Language detection & routing per query                             |
| **09.2** | Context-aware stopword filtering                                   |
| **09.3** | Query expansion (synonyms/related terms from `lexicon.json`)       |
| **09.4** | Multi-stage retrieval (BM25 → semantic re-rank → intent boost)     |
| **09.5** | Hybrid spell correction pipeline                                   |
| **09.6** | Query rewriting                                                    |
| **09.7** | Feature gathering (assemble per-document signals for fusion)       |
| **09.8** | End-to-end integration (caching, feature precompute, optimization) |

> **Note:** Task IDs match the files under `.github/ISSUE_TEMPLATE/atomic-tasks/M6-ranking/09-query-pipeline/`.

#### The FinalScore Formula

```
FinalScore = w1  × BM25F
           + w2  × PhraseAndExactMatch     (proximity and phrase bonus)
           + w3  × SemanticSimilarity(query, doc)
           + w4  × AuthorityScore          (PageRank component)
           + w5  × TrustRank               (Iranian seed-domain trust component)
           + w6  × AnchorTextMatch
           + w7  × StructuredDataBoost     (Schema.org bonus)
           + w8  × FreshnessScore          (temporal decay)
           + w9  × IntentAlignmentScore
           - w10 × SpamScore
           - w11 × DuplicateOrLowQualityPenalty
```

Key changes from a simple BM25 formula:

- `PhraseAndExactMatch` rewards pages where query terms appear together, not just individually
- `AuthorityScore` and `TrustRank` are separate weights — PageRank measures link authority globally; TrustRank is seeded from trusted Iranian domains specifically
- `DuplicateOrLowQualityPenalty` demotes near-duplicate pages and thin-content pages independently of the spam model

All weights are optimized against M1.5 NDCG measurements — not tuned by intuition.

> **Prerequisite:** M6 should not begin weight tuning until feature gathering (task 08.1) can actually collect real signals from M0 integration, M1 retrieval, M2 authority, M3 semantic, M4 intent, and M5 spam. Fusion without real features is just a formula with empty weights.

#### Freshness Ranking (Task 08.5)

Adaptive decay based on query intent:

| Query Type            | Half-Life | Example             |
| --------------------- | --------- | ------------------- |
| News / Breaking       | 3 days    | `اخبار امروز ایران` |
| Commercial / Seasonal | 30 days   | `تخفیف عید نوروز`   |
| Evergreen             | 365 days  | `تاریخچه ایران`     |

#### How Users Search Before M6

> **User searches:** `بهترین گوشی ۲۰۲۵` (best phone 2025)
>
> **What happens:** Signals are applied independently without coordination. BM25 finds keyword matches. Authority is applied as a separate multiplier. Spam score is applied separately. A page can score well on BM25 and authority but still be 2 years old and ranked above fresh 2025 reviews. Result diversity is not controlled — one site can dominate all 10 results.
>
> **User experience:** Old reviews mixed with new ones. One news site occupies 6 of 10 results. Feels repetitive and occasionally stale.

#### How Users Search After M6

> **User searches:** `بهترین گوشی ۲۰۲۵`
>
> **What happens:**
>
> 1. Temporal intent detected — strong freshness preference (half-life: 30 days for commercial queries)
> 2. FinalScore combines BM25 + semantic similarity + authority + freshness decay + spam penalty + intent alignment
> 3. MMR diversification ensures no more than 3 results from the same domain
> 4. 3-stage spell correction pipeline handles mixed Persian-English queries
> 5. Multi-stage retrieval: BM25 gets top-1000 candidates → semantic re-rank to top-200 → intent boost to final top-10
> 6. Result cached in Redis with an **intent-aware TTL** — subsequent users get sub-5ms results without serving stale news
>
> **User experience:** Top results are recent 2025 reviews from 4–5 different trusted sources. Results feel curated, diverse, and current. Feels like a professional search engine.

> **⚠️ Cache/Freshness Consistency:** The result cache TTL must respect the freshness half-life above — otherwise a long, flat cache silently defeats freshness ranking for time-sensitive queries. Recommended intent-aware TTLs:
>
> | Query Type                | Cache TTL               | Rationale                         |
> | ------------------------- | ----------------------- | --------------------------------- |
> | News / Breaking           | 0–60s (or bypass cache) | Ordering changes minute-to-minute |
> | Commercial / Seasonal     | 5–15 min                | Prices/stock change slowly        |
> | Evergreen / Informational | 1–24 h                  | Results are stable                |
>
> Cache keys should also incorporate normalized query + language + intent so different intents don't collide.

#### Problems This Step Solves

- ❌ → ✅ All signals combined optimally — no signal dominates inappropriately
- ❌ → ✅ Result diversity — one site cannot flood all 10 positions
- ❌ → ✅ Fresh results for time-sensitive queries
- ❌ → ✅ End-to-end query pipeline in a single optimized pass
- ❌ → ✅ P95 latency ≤300ms for head queries, ≤500ms for tail queries
- ❌ → ✅ High-traffic query results served from cache in <5ms

#### Quality Impact

| Metric                             | Before M6  | After M6   | Gain            |
| ---------------------------------- | ---------- | ---------- | --------------- |
| NDCG@10 vs BM25 baseline           | +0%        | +20%       | +20%            |
| Result diversity                   | Low        | High (MMR) | Significant     |
| Freshness relevance                | Poor       | Good       | Significant     |
| P95 Latency                        | ~150ms     | ≤300ms     | -50%            |
| **Estimated Iran/Persian quality** | **58–64%** | **63–70%** | **+5–6 points** |
| **Estimated global quality**       | **42–48%** | **47–53%** | **+4–5 points** |

> **M6 is the earliest credible focused public-launch point — but only if M1.5 evaluation, crawl coverage, freshness, and spam gates are passing with verified results on representative Persian/Iranian query buckets.**

---

### M7 — Metrics, Dashboards & Experimentation

> **Status: ⏳ Pending**
> **Task files:** `.github/ISSUE_TEMPLATE/atomic-tasks/M7-M8-learning/10-metrics/`

#### What Needs to Be Built

| Task     | Description                                                                       | Duration |
| -------- | --------------------------------------------------------------------------------- | -------- |
| **10.1** | Proxy metrics: NDCG@10, navigational success, vertical presence, diversity, dedup | 4 days   |
| **10.2** | Grafana dashboards (CTR, latency, error budgets, quality trends)                  | 4 days   |
| **10.3** | Team-draft interleaving harness for safe online A/B testing                       | 5 days   |
| **10.4** | Daily quality reports with language/category breakdowns                           | 4 days   |
| **10.5** | SLI/SLO definitions (99.9% uptime, ≤300ms P95, <0.1% error rate)                  | 4 days   |

#### How This Changes Operations

> **Before M7:** The team makes changes based on intuition and manual testing. There is no systematic way to know if Monday's deployment made search better or worse for the average user. There is no early warning for quality degradation.
>
> **After M7:** Every quality metric is visible on a dashboard. Daily reports show NDCG trends by query type and language. The interleaving harness lets the team test two ranking algorithms simultaneously without users noticing — and objectively measures which one users prefer by click behavior.

#### Quality Impact

> M7 primarily protects quality and enables faster, safer iteration. It does not count as a direct relevance jump.

| Metric                        | Impact                                                          |
| ----------------------------- | --------------------------------------------------------------- |
| Iran/Persian quality estimate | No direct gain; safer and faster iteration                      |
| Global quality estimate       | No direct gain; safer and faster iteration                      |
| Launch value                  | Better monitoring, rollback confidence, and experiment velocity |

---

### M8 — Click Modeling & Learning-to-Rank

> **Status: ⏳ Pending — Requires privacy gate**
> **Task files:** `.github/ISSUE_TEMPLATE/atomic-tasks/M7-M8-learning/11-click-modeling/`

> ⚠️ **Privacy Gate (Task 11.1 — MANDATORY FIRST):** Click data collection requires explicit user consent, anonymization pipeline, position-bias correction, and a rollback gate. These must be implemented and reviewed **before** any click data is collected.

#### What Needs to Be Built

| Task     | Description                                                                     | Duration |
| -------- | ------------------------------------------------------------------------------- | -------- |
| **11.1** | Click logging + privacy compliance (consent, anonymization, retention limits)   | 4 days   |
| **11.2** | Position bias correction (DCTR/UBM/DBN click models)                            | 4 days   |
| **11.3** | Click model training on anonymized data                                         | 5 days   |
| **11.4** | Pairwise Learning-to-Rank (LTR) using click-derived labels                      | 5 days   |
| **11.5** | Dwell time and query reformulation signals (detect bounces and failed searches) | 3 days   |
| **11.6** | Nightly model updates + rollback gates                                          | 4 days   |

#### How Search Evolves With User Feedback

> **Scenario:** Users consistently click result #3 over result #1 for the query `دستور پخت قورمه سبزی`.
>
> **Without M8:** The ranking stays the same indefinitely. The model never learns that users prefer result #3.
>
> **With M8:** The click model detects this preference (correcting for position bias — users are more likely to click #1 simply because it is first). The LTR model is retrained nightly. The next day, the preferred result is at #1.

This is the **flywheel** that powers Top Search Engines: more users → more clicks → better rankings → more users.

#### Problems This Step Solves

- ❌ → ✅ Rankings improve automatically from real user behavior
- ❌ → ✅ Position bias corrected — real preference signal extracted
- ❌ → ✅ Long-tail queries improve as the corpus of behavioral data grows
- ❌ → ✅ Nightly retraining means the engine adapts to changing user needs
- ❌ → ✅ Privacy-compliant from day one — user trust protected

#### Quality Impact

| Metric                             | Before M8  | After M8                   | Gain            |
| ---------------------------------- | ---------- | -------------------------- | --------------- |
| CTR@1 improvement                  | —          | +10–15% month-over-month   | Growing         |
| Long-tail query quality            | Moderate   | Good                       | Significant     |
| Rank stability                     | High       | High (with rollback gates) | Maintained      |
| **Estimated Iran/Persian quality** | **63–70%** | **68–74%**                 | **+4–5 points** |
| **Estimated global quality**       | **47–53%** | **50–56%**                 | **+2–3 points** |

---

### M9 — Production Operations & SRE

> **Status: ⏳ Pending**
> **Task files:** `.github/ISSUE_TEMPLATE/atomic-tasks/M9-production/`

#### What Needs to Be Built (20 tasks, 90 days)

**Performance & Caching**

- Query result cache with Redis (P95 ≤300ms for cache miss, <5ms for cache hit)
- Document feature precomputation (avoid re-computing authority/spam per query)
- Feature store API for ranking pipeline
- Load testing (simulate 10k concurrent users)
- Capacity planning documentation

**DevOps & Reliability**

- SLI/SLO definitions (99.9% uptime, ≤300ms P95, <0.1% error rate)
- Automated health checks and alerting
- Canary deployment pipeline (gradual rollout of ranking changes)
- Backup and disaster recovery procedures
- On-call runbooks

**Security & Compliance**

- Robots.txt compliance audit
- PII anonymization in all logs and analytics
- Network policy hardening
- Secrets management (no credentials in code)
- Independent security review

**Documentation**

- Architecture diagrams (current state)
- Complete API specification
- Feature glossary (for new team members)
- Troubleshooting runbooks
- Developer onboarding guide

#### What M9 Means for Users

> **Before M9:** The engine produces good search results but occasionally has downtime, slow responses during peak traffic, and lacks the operational polish that gives users confidence in a service.
>
> **After M9:** The engine runs at 99.9% uptime. Ranking changes are deployed gradually with automatic rollback if quality drops. Security is hardened. The system is documented well enough that any new engineer can contribute in their first week.

#### Quality Impact

> M9 does not change ranking algorithms — it makes them reliable, fast, and trustworthy at production scale.

| Metric                             | After M9                |
| ---------------------------------- | ----------------------- |
| Uptime SLO                         | 99.9%                   |
| P95 Latency (head queries)         | ≤300ms                  |
| P95 Latency (tail queries)         | ≤500ms                  |
| Cache hit rate (common queries)    | >80%                    |
| **Estimated Iran/Persian quality** | **68–74%** → **70–76%** |
| **Estimated global quality**       | **50–56%** → **52–58%** |

---

## Iran/Persian Market — Advertising Readiness

> **The key question:** At what stage can we credibly say _"ما یک موتور جستجوی ایرانی با کیفیت داریم"_ (We have a quality Iranian search engine) — and how strong is that claim?

### Stage-by-Stage Claim Strength

---

#### 🔵 Now (Current Live System) — Internal / Demo Only

**Quality estimate:** 18–24% for Persian/Iranian market queries

**What you can say:**

> _"We have a working search platform with active RedisSearch retrieval, C++ infrastructure, crawler and storage systems, and several Persian-aware M0 foundation modules under implementation."_

**Honest limitation:** M0 modules are not yet fully proven in the live C++ search path. Results are still primarily lexical. No authority, semantic ranking, spam model, freshness ranking, or LTR is active.

**Audience:** Team, early investors, technical demos only.

---

#### 🟡 After M1 + M1.5 + M2 + M5 — Controlled Beta

**Quality estimate:** 48–55% for Persian/Iranian market queries

**What you can say:**

> _"Our search engine is Persian-aware, has a measurable evaluation baseline, ranks trusted Iranian websites higher, and reduces common spam patterns. It works well for head queries (the most common searches)."_

**Honest limitation:** Semantic understanding, intent routing, full ranking fusion, and behavioral learning are still incomplete.

**Audience:** Controlled beta users, friendly partners, early adopters. **Not yet suitable for broad public launch claims.**

**Advertising claim:**

> _"موتور جستجوی آزمایشی ایرانی با ارزیابی‌پذیری، رتبه‌بندی منابع معتبر، و کنترل کیفیت"_
> (Iranian search beta with measurable Persian relevance, trusted-source ranking, and quality controls)

---

#### 🟢 After M6 (Full Ranking Pipeline Complete) — Focused Public Launch Candidate

**Quality estimate:** 63–70% for covered Persian/Iranian web content (if evaluation gates pass)

**What you can say:**

> _"For covered Persian-language and Iranian web content, our search engine combines lexical relevance, authority, spam filtering, semantic signals, intent, freshness, and result diversity in one measured ranking pipeline."_

**Honest limitation:** This is not a claim of broad Top Search Engine-level equivalence. Quality depends on crawl coverage, M1.5 evaluation results, spam operations, and freshness performance. Global coverage is lower (~47–53%).

**Audience:** Public launch candidate if the evaluation dashboard proves stable quality on representative Persian/Iranian query buckets. Press coverage. Investor presentations.

**Advertising claims:**

> _"موتور جستجوی ایرانی فارسی‌محور، ساخته‌شده برای محتوای محلی، منابع معتبر، و کیفیت قابل اندازه‌گیری"_
> (Persian-first Iranian search engine, built for local content, trusted sources, and measurable quality)

> _"ما محتوای ایرانی را بهتر می‌فهمیم: زبان فارسی، سایت‌های .ir، فروشگاه‌های ایرانی"_
> (We understand Iranian content better: Persian language, .ir sites, Iranian stores)

**This is the milestone recommended for a focused public launch announcement — once evaluation metrics confirm quality.**

---

#### 🌟 After M8/M9 (Click Learning + Production Complete) — Strong National Product

**Quality estimate:** 70–76% for covered Persian/Iranian web content (with real traffic, privacy-safe learning, and stable operations)

**What you can say:**

> _"A reliable Iranian search product for Persian and Iranian web content, with measured relevance, local authority signals, spam defenses, production reliability, and privacy-aware learning from user behavior."_

> **Important:** Do not claim a fixed Top Search Engine-level percentage unless an independent evaluation or a stable judged benchmark proves it.

**Key differentiators over foreign engines:**

- **Persian morphology:** Handles conjugations, plurals, clitics, ZWNJ — once task 01.7 is fully integrated
- **Iranian domain authority:** `.ir` sites evaluated with curated Iranian trust seeds, not US/EU web graph
- **No algorithmic disadvantage:** Foreign engines under-serve `.ir` domains due to crawl prioritization
- **Local privacy:** User data stays in Iran, subject to Iranian law
- **Persian spam patterns:** Specifically trained on Iranian SEO manipulation patterns

**Flagship advertising claim:**

> _"موتور جستجوی ایرانی فارسی‌محور؛ سریع، قابل اعتماد، و ساخته‌شده برای محتوای ایرانی"_
> (Persian-first Iranian search engine — fast, reliable, and built for Iranian content)

---

### Competitive Advantage Summary

| Capability                        | Our Engine (After M8/M9)         | Generic Global Search in Iran       |
| --------------------------------- | -------------------------------- | ----------------------------------- |
| Persian morphology                | ✅ Native (once 01.7 integrated) | ⚠️ Approximate for edge cases       |
| Iranian domain trust graph        | ✅ Curated Iranian seed domains  | ❌ Not locally tuned                |
| Persian spam patterns             | ✅ Trained on local patterns     | ❌ Generic models                   |
| Local content freshness           | ✅ Can prioritize `.ir` sources  | ⚠️ Depends on global crawl priority |
| RTL result presentation           | ✅ Designed Persian-first        | ⚠️ Adapted from generic layout      |
| Data sovereignty                  | ✅ Iran-hosted                   | ❌ Foreign servers                  |
| Persian query spelling correction | ✅ Trained on local corpus       | ⚠️ Global model, not local-first    |

> **Key insight:** The credible near-term goal is not a broad equivalence claim. The credible goal is a high-quality Persian/Iranian search product for covered local content, backed by measured relevance, crawl freshness, spam control, and production reliability.

---

## Gaps, Missing Milestones & Risks

The milestone plan above covers the ranking/retrieval core well. This section captures cross-cutting items that the numbered milestones assume but do not explicitly own — and the risks that could invalidate the estimates.

### Missing / Under-Specified Work Items

| Area                                          | Gap                                                                                                                                            | Why It Matters                                                                                                         | Suggested Home                                     |
| --------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------- |
| **Crawl coverage & scale**                    | No milestone owns _how much_ of the Iranian web is crawled, how often, or how completeness is measured. Coverage is used only as an M1.5 gate. | Freshness, recall, and every quality % depend on corpus breadth. A great ranker over a thin index still scores poorly. | New track (parallel to M2), or expand M9 crawl ops |
| **Query autocomplete / suggestions**          | Not covered. Pulse trend data (`normalized_query`) is an obvious feed.                                                                         | High-visibility UX; reduces zero-result and reformulation rates.                                                       | Extend M4/M7 (Pulse-driven)                        |
| **Zero-result & "did you mean" UX**           | M3 covers spell-correction internals, but not the user-facing fallback experience (suggestions, relaxed matching, related queries).            | Zero-result rate is a launch gate (<5%); the UX affects perceived quality directly.                                    | Product layer on top of M3                         |
| **Instant answers / knowledge panel**         | No plan for calculator, currency/FX, weather, Jalali↔Gregorian date conversion, unit conversion.                                              | Very common Persian head queries; cheap, high-impact differentiators.                                                  | New lightweight vertical (post-M4)                 |
| **Non-text verticals (image/video/news/map)** | Roadmap is text-only but frames "Top Search Engine parity."                                                                                    | Parity framing is misleading without acknowledging these are out of scope for now.                                     | Explicit scope statement (done here)               |
| **Search-input security**                     | `escapeRedisString()` is partial; RedisSearch query-syntax injection not hardened. Currently only implied under M9.                            | A malformed query can crash search or leak/scan the index; this is a _now_ risk, not a launch-polish item.             | Pull forward into M1 / Quick Wins                  |
| **Analytics-driven evaluation**               | M1.5 uses human judgments; Pulse behavioral data could seed weak labels and query sets earlier.                                                | Faster, cheaper judged-set bootstrapping for Persian queries.                                                          | Bridge M1.5 ↔ M7                                  |

### Explicit Scope Statement

This roadmap targets **Persian/Iranian web _text_ search quality**. Image, video, news-carousel, map/local-pack, and shopping-feed verticals are **out of scope** for the current milestones. "Top Search Engine parity" throughout this document means _text relevance parity on covered Persian/Iranian content_, not feature parity across all Google/Bing surfaces.

### Risk Register

| Risk                                                 | Impact                             | Likelihood | Mitigation                                                                                        |
| ---------------------------------------------------- | ---------------------------------- | ---------- | ------------------------------------------------------------------------------------------------- |
| M0 Python↔C++ integration (01.6) slips              | Blocks M0 gains; live bug persists | Medium     | Ship the C++ Quick Wins now; treat pybind11 as an enhancement, not a blocker                      |
| Crawl coverage too thin for launch                   | All quality % overstated           | High       | Stand up coverage measurement early; publish coverage report (M1.5 gate)                          |
| Estimated % never validated                          | Public claims indefensible         | Medium     | Hard-gate all external claims on M1.5 measured NDCG                                               |
| Data-sovereignty claim vs. hosting/sanctions reality | Legal/marketing exposure           | Medium     | Legal review before using the "data stays in Iran / Iranian law" claim (see below)                |
| Spam model false positives on `.gov.ir`/`.ac.ir`     | Trusted sites demoted              | Medium     | Allow-list + false-positive review loop (already noted in M5)                                     |
| Dependency chain (M6 needs M0–M5) underestimated     | Timeline optimism                  | High       | Track the critical path explicitly; M6 cannot start fusion tuning until upstream signals are real |

### Legal / Hosting Caveat

The "Local privacy — user data stays in Iran, subject to Iranian law" and "Iran-hosted" advantages are stated as unconditional wins. They carry trade-offs (regulatory obligations, potential data-access requirements, infrastructure/sanctions constraints, and user-trust implications). **Treat these as claims requiring legal review before external use**, not as settled differentiators.

### Estimate Methodology (to be firmed up at M1.5)

The Iran/Persian and Global % bands are planning estimates. After M1.5, each band should be re-anchored to a measured target — e.g. "M6 = NDCG@10 ≥ 0.70 on the Persian judged set relative to a Top-SE reference sample" — so the numbers become falsifiable rather than illustrative. Until then, treat every percentage in this document as a hypothesis, not a measurement.

---

## Summary Table

| Milestone               | Status                                   | Iran/Persian % | Global %       | Step Gain  | What Changes for Users                                      |
| ----------------------- | ---------------------------------------- | -------------- | -------------- | ---------- | ----------------------------------------------------------- |
| **Current Live System** | ✅ Active                                | 18–24%         | 12–16%         | Baseline   | RedisSearch lexical retrieval with field weighting          |
| **M0 — Foundation**     | 🔵 Modules done; C++ integration pending | 26–32%         | 18–22%         | +6–8 pts   | Persian normalization, language detection, IDF stopwords    |
| **M1 — Retrieval**      | ⏳ Pending                               | 34–40%         | 24–30%         | +6–8 pts   | BM25F, dedup, quality gates, snippets, n-gram fallback      |
| **M1.5 — Eval Gates**   | ⏳ Pending                               | No direct gain | No direct gain | Protection | Quality measurement, judged query sets, regression gates    |
| **M2 — Authority**      | ⏳ Pending                               | 43–50%         | 30–36%         | +9–10 pts  | Trusted Iranian sources and structured data become rankable |
| **M5 — Spam**           | ⏳ Pending                               | 48–55%         | 34–40%         | +4–5 pts   | Persian keyword stuffing eliminated                         |
| **M3 — Semantic**       | ⏳ Pending                               | 54–61%         | 39–45%         | +5–6 pts   | Synonyms, spell correction, meaning-based search            |
| **M4 — Intent**         | ⏳ Pending                               | 58–64%         | 42–48%         | +3–4 pts   | Navigational/transactional/informational routing            |
| **M6 — Ranking**        | ⏳ Pending                               | 63–70%         | 47–53%         | +5–6 pts   | All signals fused, diversity, freshness, fast               |
| **M7 — Metrics**        | ⏳ Pending                               | No direct gain | No direct gain | Protection | Quality dashboards, A/B testing, safer iteration            |
| **M8 — LTR**            | ⏳ Pending                               | 68–74%         | 50–56%         | +4–5 pts   | Learns from user clicks, improves nightly                   |
| **M9 — Production**     | ⏳ Pending                               | **70–76%**     | **52–58%**     | +2 pts     | 99.9% uptime, ≤300ms, hardened, documented                  |

**Total estimated work:** ~330–400 working days with 3–4 engineers, parallelizable across independent tracks.

**Recommended public launch point:** After M6 (~63–70% quality for covered Persian/Iranian content) — once M1.5 evaluation, crawl coverage, freshness, spam, and latency gates pass with verified results.

---

_Document updated: July 9, 2026_
_Repository: hatefsystems/search-engine-core_
_Branch: master_
