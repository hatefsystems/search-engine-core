# Persian 500-query icon benchmark

## Current ESCO/Persian iteration

The new shared ranker and Persian knowledge layer match a frozen expected icon among the first three results for **467/500 cases (93.4%)** with real E5 inference, up from the original hybrid baseline of **146/500 (29.2%)**. Lexical plus curated Persian mappings reaches **463/500 (92.6%)**, up from **89/500 (17.8%)**. The dataset and relevance labels have not changed.

**This meets the numerical development target, not independent human acceptance.** The implementation was developed with access to these cases. The separate review packet remains pending; its proposed labels have not been treated as approved or evaluated as a holdout. Both new reports record `acceptanceEvaluated=false` and `acceptancePassed=null`.

| Measure                                 | Current lexical + curated mappings |  Current hybrid |
| --------------------------------------- | ---------------------------------: | --------------: |
| Hit@1                                   |                              85.8% |           86.0% |
| Hit@3                                   |                    92.6% (463/500) | 93.4% (467/500) |
| Misses at 3                             |                                 37 |              33 |
| Empty results                           |                                 13 |               0 |
| Cases with a frozen expected ID present |                            490/500 |         490/500 |

The pinned model indexed all 7,770 approved icons and 18,504 concepts. All 500 hybrid queries used semantic retrieval. Local direct-ranking latency averaged 49.47 ms (P95 59.76 ms), with peak RSS 1,109.8 MiB. These native measurements have no Docker CPU/memory ceiling and are not production guarantees. The full-image CI repeats the benchmark separately under the existing limits.

Catalog identity: `d5d5d387eb4c71f3c9ed15e8`. Knowledge identity: `35cfbfbe24b244d07bf81d36`. Embedding identity: `9e7de405834077a2725957fa46473582fe56119600a86eed34abcd14119dc60d`.

Full evidence: [current lexical results](measurements/fa-500-v2/lexical-search.json), [current hybrid results](measurements/fa-500-v2/hybrid-search.json), and [knowledge build/proposal counts](measurements/fa-500-v2/knowledge-build.json). The [knowledge workflow](persian-knowledge.md) explains untranslated ESCO entries, mapping review states and human acceptance. New artwork does not retroactively change frozen expected IDs; absent expected IDs still count as misses.

## Original baseline, preserved

Measured on 2026-10-16 against the ranking code in PR #64, commit `bac0e43ab1ff67128be4b00779f1afa3477c8db2` (identical tree to local `609b17f`).

**The original baseline did not meet the 90% target.** Hybrid manual search matched an expected icon in its first three results for **146/500 queries (29.2%)**. Lexical search matched **89/500 (17.8%)**. These are frozen-label matching rates, not independently audited human relevance scores.

## Method

- Exactly 500 distinct Persian phrases, 25 domains, 20 phrases per domain. The set covers short names and more specific occupational/skill phrases, including trades and nontechnical work.
- Dataset: `services/semantic-icon-engine/tests/evaluation-fa-500.json`.
- Frozen dataset SHA-256: `9a47d346814a068f492fe4a9bf6655e2d0c43fc68c44c8094229814ca9b2bb6d`.
- Labels were authored by the coding assistant with access to the implementation and dictionary, then frozen before the first search run. No labels or ranking parameters were adjusted after seeing these results. This is a diagnostic development set, not a blind, independent or representative production sample.
- Each case declares acceptable canonical icon IDs. Direct Lucide and mirrored Iconify IDs are equivalent only when explicitly listed; brand alternatives are also enumerated. Hit@3 means at least one declared relevant ID appears among the first three results.
- All 500 cases remain in the denominator. Empty results and absent expected artwork count as misses. The relevance list may omit other defensible icons; an independent review is required before interpreting this number as perceived quality.
- The measured path is `Matcher.search`, corresponding to manual search with no category filter. Automatic suggestions (`Matcher.suggest`) are a separate path and are not measured by this report.
- Hybrid uses the real pinned multilingual E5 ONNX model and a complete 7,768-icon vector index. Every hybrid query actually used semantic candidates. This is not a mock-model run.
- Direct ranking-function measurements exclude HTTP rate limiting and inference timeout behavior. Local latency and RSS are diagnostic measurements, not production service guarantees.

## Results

| Measure                                       |        Lexical |          Hybrid |
| --------------------------------------------- | -------------: | --------------: |
| Hit@1                                         |          13.6% |           20.6% |
| Hit@3                                         | 17.8% (89/500) | 29.2% (146/500) |
| Misses at 3                                   |            411 |             354 |
| Empty results                                 |            254 |               0 |
| Cases with at least one expected icon present |        490/500 |         490/500 |
| Mean ranking latency                          |        3.04 ms |        38.19 ms |
| P95 ranking latency                           |        9.44 ms |        47.38 ms |
| Peak process RSS                              |       22.9 MiB |      1111.7 MiB |

Catalog version: `4e8773d0e5e52dcead551b02`. Embedding identity: `c3558c510ba4a1c6370f005a5477a505f2d5796be7ab1fa704a44cb11e3d92f0`.

## Domain breakdown

| Domain                          | Cases | Lexical hits at 3 | Hybrid hits at 3 | Hybrid Hit@3 |
| ------------------------------- | ----: | ----------------: | ---------------: | -----------: |
| آموزش و پژوهش آموزشی            |    20 |                 3 |                4 |          20% |
| ابر و زیرساخت                   |    20 |                16 |               17 |          85% |
| ارتباطات و خدمات عمومی          |    20 |                 8 |               10 |          50% |
| امنیت و امداد                   |    20 |                 6 |                9 |          45% |
| برق و الکترونیک                 |    20 |                 2 |                3 |          15% |
| حقوق و امور اداری               |    20 |                 0 |                1 |           5% |
| حمل‌ونقل و لجستیک               |    20 |                 3 |                4 |          20% |
| دندانپزشکی و خدمات درمانی تخصصی |    20 |                 1 |                2 |          10% |
| رسانه و تولید محتوا             |    20 |                 1 |                7 |          35% |
| زیبایی و مراقبت فردی            |    20 |                 1 |                1 |           5% |
| ساختمان و معماری                |    20 |                 4 |                6 |          30% |
| صنعت و مکانیک                   |    20 |                 3 |                4 |          20% |
| طراحی و هنرهای تجسمی            |    20 |                 3 |                3 |          15% |
| علوم و آزمایشگاه                |    20 |                 0 |                7 |          35% |
| غذا و آشپزی                     |    20 |                 4 |                7 |          35% |
| فروش و بازاریابی                |    20 |                 4 |                5 |          25% |
| مالی و حسابداری                 |    20 |                 1 |                4 |          20% |
| محیط زیست و خدمات شهری          |    20 |                 1 |                5 |          25% |
| مدیریت و منابع انسانی           |    20 |                 2 |                4 |          20% |
| نرم‌افزار و فناوری              |    20 |                13 |               13 |          65% |
| ورزش و تندرستی                  |    20 |                 1 |                6 |          30% |
| پزشکی و سلامت                   |    20 |                 4 |                7 |          35% |
| پوشاک و صنایع دستی              |    20 |                 0 |                0 |           0% |
| کشاورزی و دامداری               |    20 |                 5 |               11 |          55% |
| گردشگری و هتلداری               |    20 |                 3 |                6 |          30% |

## Observations

- Hybrid retrieval adds 57 successful cases (11.4 percentage points) and eliminates empty result lists, but returning something does not guarantee a useful match.
- Expected artwork exists for 490/500 labeled cases. Of the 354 hybrid misses, 344 have at least one expected icon already in the catalog. Adding icon packs alone cannot close this ranking/understanding gap.
- For `وکالت در دادگاه`, the first results are `expand`, `shrink` and `cookie`. Broad category expansion and ambiguous English tags such as `scale` leak unrelated concepts; some Persian display labels inherit this ambiguity as well.
- `برنامه‌نویسی پایتون` includes the Python logos in the top three; `تعمیر موتور خودرو` includes a car icon. The test therefore measures representative related artwork, not a unique pictogram for every specialist skill.
- Ten cases, including dental and some organ-specific concepts, have no declared expected artwork in the approved catalog. Those remain failures rather than being replaced with arbitrary generic labels.
- A reasonable next iteration is structured occupation/skill concepts, Persian aliases and definitions, explicit concept-to-icon mappings, and disambiguation of broad English keywords. Add artwork for proven gaps. Validate that iteration on a separate human-reviewed holdout set.

## Reproduce

From the repository root, against matching prepared artifacts:

```sh
python services/semantic-icon-engine/scripts/benchmark.py \
  --data /path/to/artifacts/data \
  --queries services/semantic-icon-engine/tests/evaluation-fa-500.json \
  --output /tmp/fa-500-lexical.json

OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
python services/semantic-icon-engine/scripts/benchmark.py \
  --data /path/to/artifacts/data \
  --models /path/to/artifacts/models \
  --vectors /path/to/artifacts/vectors \
  --queries services/semantic-icon-engine/tests/evaluation-fa-500.json \
  --output /tmp/fa-500-hybrid.json
```

The release-image CI now runs this benchmark in a separate offline, read-only container at 1 CPU and 1536 MiB, after stopping the API test container, and uploads the complete report. It is explicitly report-only while the target is unmet. A green infrastructure job does not mean the 90% relevance goal passed.

Full per-query evidence: [lexical-search.json](measurements/fa-500/lexical-search.json) and [hybrid-search.json](measurements/fa-500/hybrid-search.json).
