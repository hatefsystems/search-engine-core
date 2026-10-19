# Atomic Tasks Documentation Audit

## Scope

This audit covers the atomic-task documentation tree recursively, including all milestone folders and top-level guide documents.

- Initial scan before this update: 89 Markdown files.
- After this update: 95 Markdown files, including this audit and the new M1.5 evaluation baseline tasks.
- Milestone task documents after this update: 85.
- Future vertical idea documents: 1.

## Overall Finding

The documentation set is strong as a planning system, but it needed a clearer quality loop. The original order put evaluation and learning too late, which made the roadmap vulnerable to adding many features without knowing whether relevance actually improved.

The corrected roadmap introduces M1.5 immediately after M1 so the project has query sets, judgments, metrics, Persian/Iran-local checks, crawl/freshness baselines, and regression gates before advanced semantic and ranking work.

## Folder Audit

| Folder | Status | Main Finding | Required Follow-Up |
|---|---|---|---|
| Top-level guides | Cleaned up | Counts, internal links, and obsolete parent-file references are aligned with the current tree. | Keep `README-atomic-tasks.md`, the epic, and this audit as source-of-truth docs. |
| M0 foundation | Strong | Good coverage for Unicode, language detection, script handling, and stopword/IDF. | Keep Persian edge cases in every downstream test. |
| M1 retrieval | Strong but incomplete without evaluation | BM25, n-gram, indexing, dedup, quality gate, and latency are the right baseline; early retrieval tasks now require evidence artifacts. | Complete M1.5 before M2/M3/M6 quality-impacting work. |
| M1.5 evaluation | Added | Provides the missing quality loop. | Treat as P0 and keep artifacts versioned. |
| M2 content understanding | Improved | Link and structured-data tasks exist, and link-graph tasks now require explicit evidence artifacts. | Add canonicalization, nofollow/sponsored/ugc, stale links, and spam-aware authority checks when implementing. |
| M3 semantic | Good but risky | Semantic expansion and embeddings can harm precision without gates. | Require expansion caps, per-query failure review, and M1.5 evaluation reports. |
| M4 intent | Good | Weak labels and vertical detection are useful. | Add confusion matrices and Persian/Iran-local vertical coverage. |
| M5 quality/spam | Good base | Unsupervised spam models are planned, but adversarial SEO is under-specified. | Add scraped content, doorway pages, keyword stuffing, redirects, cloaking, and false-demotion review. |
| M6 ranking/query pipeline | Important but should not run blind | FinalScore, MMR, query expansion, spell correction, and pipeline tasks are valuable; language routing now requires explicit routing and latency evidence. | Every task must include baseline comparison, latency impact, and failure examples. |
| M7-M8 evaluation/learning | Valuable but too late in original order | Dashboards, interleaving, click models, and LTR are necessary for mature quality. | Keep full M7/M8 later, but use M1.5 as the early offline version. |
| M9 production/security/docs | Cleaned up | Performance, DevOps, security, and documentation are present; documentation task files now have explicit H1 titles and SLO tasks require quality-aware evidence. | Keep robots/privacy/security from being delayed until launch. |
| Future real estate vertical | Useful future idea | Good example of a local vertical. | Only start after M1.5 and base ranking are measurable. |

## Quality Standards For Every Task

Every task that can affect search quality must now include:

- Baseline comparison against the latest approved evaluation set.
- Per-bucket metrics for language, script, intent, freshness, and Persian/Iran-local queries.
- Failure examples, not only aggregate scores.
- Latency and resource impact.
- Spam, duplicate, stale-result, and zero-result checks where relevant.
- Rollback notes for deployable ranking, model, crawling, or indexing changes.

## Task-Level Recommendations

| Task Area | Recommendation |
|---|---|
| 01.1-01.6 | Keep as foundation; ensure every downstream task reuses normalization and language/script metadata. |
| 02.1-02.6 | Treat as the first measurable retrieval baseline; do not tune by intuition. |
| 02.7-02.11 | Complete before advanced ranking, semantic expansion, link authority tuning, or click learning. |
| 03.1-03.5 | Add canonical URL, link spam, nofollow/sponsored/ugc, redirect, and stale-link handling during implementation. |
| 04.1-04.4 | Add Iran commerce formats, rial/toman handling, Persian digits, and structured-data false-positive review. |
| 05.1-05.7 | Add precision guardrails for expansion; require rollback for bad lexicon/model refreshes. |
| 06.1-06.5 | Add confusion matrices and a Persian/Iran-local intent slice. |
| 07.1-07.5 | Expand spam scope to adversarial SEO and false-demotion review. |
| 08.1-08.4 | Require an M1.5 evaluation report before accepting any score-weight change. |
| 09.1-09.8 | Validate every query-processing stage against exact-match, typo, Persian ZWNJ, mixed-language, and freshness queries. |
| 10.1-10.5 | Build on M1.5 rather than replacing it; dashboards should show both global and local slices. |
| 11.1-11.5 | Do not train LTR from clicks until privacy, consent/anonymization, and position-bias correction are verified. |
| 12.1-12.5 | Treat performance as a quality feature; stale cache and slow results both harm relevance perception. |
| 13.1-13.5 | Add quality-aware canary and rollback gates, not only uptime/latency checks. |
| 14.1-14.5 | Keep robots, PII, audit, and egress controls active before production crawling. |
| 15.1-15.5 | Keep architecture, API, glossary, runbooks, and onboarding aligned with the revised M1.5 quality loop. |

## Remaining Documentation Risks

- Some task files still include optimistic quality targets that require real corpora, crawl data, and judgment sets to prove. M1.5 defines the policy; implementation PRs must attach the report artifact.
- Some late-stage learning tasks depend on query logs and click feedback that the project cannot manufacture from documentation alone.
- The docs should eventually be enforced by CI with Markdown linting and internal-link checks so link drift does not return.
