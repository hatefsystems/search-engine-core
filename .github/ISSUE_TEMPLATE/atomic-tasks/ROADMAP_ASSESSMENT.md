# Search Quality Roadmap Assessment

## Executive Summary

The atomic-task roadmap is directionally strong: it covers multilingual text processing, lexical retrieval, authority signals, semantic expansion, intent detection, spam and quality scoring, ranking fusion, evaluation, click learning, and production operations.

That said, completing the tasks step by step does not automatically guarantee industry-leading search quality. Search quality depends on much more than having the right component list. It also requires crawl scale, freshness, high-quality indexing, strong abuse controls, real evaluation data, query logs, click feedback, localization, and continuous ranking measurement.

The right target language for this roadmap should be neutral and measurable: "production-grade search quality" and "industry-leading relevance" rather than comparison to any named search engine.

## Quality Estimate

These percentages are engineering estimates, not absolute guarantees. A real percentage requires a fixed query set, human judgments, traffic data, and repeatable metrics.

| Scenario | Expected quality ceiling |
|---|---:|
| Tasks implemented without substantial real data or feedback loops | 25-35% of a mature open-web search experience |
| Tasks implemented with a strong Persian/Iran corpus and offline judgments | 50-65% for the target local audience (updated: 5 new gap-closing tasks added) |
| Tasks implemented with large crawl coverage, query logs, click feedback, and active spam operations | 60-75% for a focused local or vertical search experience |
| General open-web parity with mature global search platforms | Requires years of crawl scale, feedback data, anti-spam operations, infrastructure, and specialized teams |

The 5 newly added tasks improve the 45-60% ceiling to 50-65% by closing specific high-value gaps:
- **Task 01.7 (Persian Morphological Analysis):** Handles plurals (-ها/-ان), verb conjugations, agglutinative prefixes, ezafe, and clitics. Without this, Persian queries miss conjugated and inflected forms — the single biggest precision/recall gap for Persian-language search.
- **Task 02.12 (Snippet Generation):** Produces query-aware, morphology-aware snippets with RTL rendering and XSS-safe HTML. Directly impacts user-visible result quality and click-through.
- **Task 03.6 (TrustRank & Spam-Aware Authority):** Propagates trust from 50+ curated Iranian/Persian seed domains. Prevents link-spam farm manipulation of PageRank. Replaces raw HostRank with `AuthorityScore = 0.6*PageRank + 0.4*TrustRank`.
- **Task 07.6 (Adversarial SEO Detection):** Detects keyword stuffing, cloaking, doorway pages, thin/scraped content, link schemes. Prevents low-quality spam from surfacing in results.
- **Task 08.5 (Freshness Ranking Signal):** Classifies temporal query intent using 200+ Persian temporal keywords, applies adaptive freshness decay (half-life 3d/30d/365d), and boosts fresh content for news/current-events queries.

The remaining gap after all tasks is mostly data and operating maturity:

| Remaining factor | Approximate share of remaining gap |
|---|---:|
| Crawl coverage, freshness, canonicalization, and index quality | 20-25% |
| Query logs, click logs, dwell, reformulation behavior, and position-bias correction | 10-20% |
| Human judgments and Persian/Iran-local evaluation sets | 10-15% |
| Adversarial spam defense, continuous monitoring, rollback, and tuning | 10-15% |

## Current Repository State

- The `modules/` directory currently shows implemented or documented work mainly for M0 foundation tasks: Unicode normalization, language detection, script-specific processing, stopword/IDF analysis, and batch-job support.
- The `.github/ISSUE_TEMPLATE/atomic-tasks/` directory is much broader than the current implementation. M1-M9 include retrieval, ranking, semantic, quality, evaluation, learning, and production plans, but most of those areas are still task specifications rather than implemented modules.
- Existing foundation work is a good base for Persian, Arabic-script, mixed-script, and multilingual handling, but it is not enough by itself to produce production-grade result quality.

## What Is Strong

- Multilingual and Unicode-first thinking is present from the start.
- Persian/Arabic-script details such as ZWNJ, Arabic/Persian character variants, and script-aware processing are treated as first-class concerns.
- The roadmap includes key real-world search subsystems: BM25 retrieval, n-gram fallback, deduplication, link authority, structured data, semantic expansion, ranking fusion, spam scoring, evaluation, click modeling, and production SLOs.
- The task breakdown is practical for implementation because most tasks are small enough to assign, test, and review.

## Main Gaps

- Crawl quality and freshness need stronger acceptance criteria: recrawl policy, canonicalization, robots/noindex handling, stale index detection, and page-change prioritization.
- Evaluation needs to move earlier. Before advanced ranking work, the project needs judgment sets, query buckets, Persian/Iran-specific relevance tests, and repeatable offline metrics.
- Ranking tasks need stricter proof requirements. Proxy NDCG, navigational success, spam demotion, freshness, diversity, and latency should be measured against a baseline before a task is marked complete.
- ~~Abuse and adversarial SEO handling are too shallow~~ → **Addressed by Task 07.6** (adversarial SEO detection) and **Task 03.6** (TrustRank). Link spam, scraped content, doorway pages, keyword stuffing, cloaking, and redirect chains now have dedicated detection tasks.
- Localization is not just language detection. For users in Iran, relevance needs Persian morphology, local entities, local commerce patterns, local news freshness, mixed Persian/English queries, Persian digits, calendar/date handling, and Iranian domain/site quality signals. → **Partially addressed by Task 01.7** (Persian morphological analysis). Persian entity normalization and calendar handling remain outstanding.
- ~~Snippet generation missing~~ → **Addressed by Task 02.12** (query-aware snippet extraction with RTL, morphology-aware term highlighting, XSS-safe HTML).
- ~~Freshness ranking not integrated~~ → **Addressed by Task 08.5** (adaptive temporal freshness ranking signal with Persian intent classification).
- Click learning and personalization must be privacy-safe and delayed until logging, consent, anonymization, position-bias correction, and rollback gates are clear. → **Task 11.1 now includes a mandatory Privacy Gate** requirement before any click data is collected.

### Fixed Dependency Issues
- **08.3 circular dependency resolved:** Task 08.3 (Parameter Optimization) previously listed Task 10.1 (M7 Proxy Metrics) as a dependency. This was a circular dependency since M6 runs before M7. Fixed: Task 08.3 now uses Task 02.8 (M1.5 Offline Metrics Harness) as the objective function. Task 10.1 can be used for a second refinement round after M7 is complete.
- **05.1/05.2 scope overlap resolved:** Task 05.1 (Co-occurrence Matrix) had scope overlap with Task 05.2 (PPMI/SVD). Fixed: 05.1 now produces only the raw co-occurrence CSR matrix; 05.2 owns all PPMI and SVD work.

## Priority Updates

1. Clarify M1 as the first measurable retrieval baseline: BM25, n-gram fallback, field weighting, exact-match behavior, deduplication, and latency.
2. Complete the new M1.5 evaluation baseline before M2/M3/M6 quality-impacting work: query sets, Persian/Iran buckets, judgment labels, baseline metrics, crawl/freshness checks, and regression gates.
3. Add crawl and freshness tasks: canonical URL handling, recrawl scheduling, freshness scoring, stale-result detection, sitemap support, and index coverage reporting.
4. Strengthen quality and spam criteria: adversarial SEO features, site-level reputation, false-demotion review, and safe rollback.
5. Add localization acceptance tests: Persian ZWNJ, Persian/Arabic digit variants, local entity names, mixed-language queries, local commerce and services, and high-freshness news queries.
6. Require every ranking change to produce an evaluation artifact: baseline comparison, metric delta, latency impact, failure examples, and rollback notes.

## Revised Execution Order

1. M0 Foundation.
2. M1 Retrieval Baseline.
3. M1.5 Search Quality Evaluation Baseline.
4. M2 Authority, structured data, and crawl-derived signals.
5. M5 Quality and spam scoring.
6. M3 Semantic understanding.
7. M4 Intent and vertical detection.
8. M6 Ranking and query pipeline.
9. M7 Metrics, dashboards, and interleaving.
10. M8 Click modeling and learning-to-rank.
11. M9 Production performance, reliability, security, and documentation.

Security, privacy, robots compliance, and basic observability should start early in lightweight form, even though their full tasks remain in M9.

## Honest Assessment

If the current roadmap is completed carefully and measured with real evaluation data, it can move the project toward production-grade search quality. It is a serious roadmap, not a toy keyword-search plan.

However, it is not yet sufficient to promise industry-leading relevance. The missing ingredient is not one more algorithm; it is the operating loop: crawl, index, measure, learn, inspect failures, fight spam, localize, and repeat. The docs should make that clear so the project sets credible expectations and avoids overclaiming.
