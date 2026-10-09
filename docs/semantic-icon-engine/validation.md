# Validation and measured behavior

Measurements below were collected from actual native runs of this implementation. The machine details and complete per-query outputs are in `measurements/`. They are a development-set comparison, not a production SLA or independent accuracy study.

## Catalog and index

| Source | Approved IDs (including duplicates) | Duplicate SVG records | Excluded | Rejected |
| --- | ---: | ---: | ---: | ---: |
| Lucide 0.468.0 | 1544 | 1 | 0 | 0 |
| Iconify, reviewed Lucide collection | 2160 | 220 | 423062 | 0 |
| Simple Icons 14.0.0 | 0 | 0 | 3247 | 0 |

Total approved: **3704 canonical IDs**. Both FP32 and INT8 indexing completed **3704/3704** embeddings. Every upstream source pack was discovered. The approved catalog is a policy-limited subset, not all 425,222 Iconify IDs.

Iconify exclusions: 315,597 need original-notice review; 107,465 have unknown or non-allowlisted licenses. Simple Icons exclusions: 3,076 require brand exposure review, 102 have non-allowlisted individual licenses, 69 need an individual notice review. These exact source pins and decisions are recorded in `measurements/ingestion.json`.

Catalog snapshot `bac06580c1c646cd7b800aa6` occupies 69,484,544 bytes. Full ingestion took 10.53 seconds, 10.53 CPU seconds and 255.88 MiB peak RSS, separately from runtime recommendation measurement.

## Retrieval comparison

The versioned 50-query dataset covers ten categories with Persian, English and mixed queries, including healthcare, education, law, architecture, design, agriculture, retail and repair. Each query has three labeled relevant IDs. Recall is the number of those IDs retrieved divided by three. Hit rate, also recorded in the JSON results, means at least one relevant ID was returned. Labels and synonyms were developed together; **this is not a held-out dataset**.

| Mode | Recall@3 | Recall@5 | Mean ms | p95 ms | Peak RSS MiB | CPU seconds | Average CPU cores |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| lexical | 0.81 | 0.85 | 100.66 | 194.35 | 17.15 | 5.15 | 1.00 |
| fp32 | 0.76 | 0.81 | 174.35 | 237.28 | 1115.18 | 12.11 | 1.08 |
| int8 | 0.74 | 0.78 | 185.45 | 261.01 | 1052.13 | 12.55 | 1.07 |

FP32 ONNX size: 470,268,510 bytes. Dynamic INT8 MatMul variant: 406,734,568 bytes. Tokenizer/model-card sizes are additional. Both variants used ONNX Runtime 1.20.1 and the same pinned E5 revision.

The lexical baseline performs best on this dictionary-aware development set. FP32 improves Hit@5 to 1.00 but does not improve Recall@3 or Recall@5; INT8 loses more retrieval quality and does not establish a reliable speed advantage. Keep lexical mode as the default. When semantic mode is explicitly enabled, retain FP32 unless a held-out evaluation on target hardware justifies a change. Scores are not calibrated confidence probabilities.

Peak memory is process maximum RSS, including model startup. Latencies measure `Matcher.suggest` after one warm-up, sequentially, and exclude HTTP/browser overhead. CPU seconds and mean occupied cores cover initialization plus the measured run. Native runs were **not constrained by Docker's 1-CPU/1536-MiB cgroup**; some measured CPU averages exceed one core. Concurrent compilation and shared-host scheduling can affect timings. These numbers do not prove compliance under production load or worst-case 512-token requests. The default keeps semantic inference off until target-host ceiling tests pass.

## Executed tests

- Python: 48 passed, including malicious SVG, malformed XML, invalid IDs, license/brand restrictions, editorial enrichment, checksums, full-source discovery fixtures, incremental source reuse, interrupted import recovery, bundle traversal/link/size protection, API contracts/rate limits, model outage, timeout/busy fallback, vector resume/reuse/version guards and real pinned E5 inference.
- The real E5 test replaced Python socket connection attempts with failure, then loaded local weights and compared normalized Persian/English embeddings. This is a Python runtime no-download check, **not a container egress proof**.
- C++ profile CTest: 3 suites passed, including stable manual icons, legacy defaults and same-item custom-media references.
- JavaScript autosave: 14 passed.
- Browser fixture tests: editor and public profile passed at seven widths; icon picker passed automatic/manual/custom/none, Persian filters, pagination, keyboard navigation, late responses, low confidence, unavailable backend, unsafe URLs and four RTL widths.
- Production C++ bridge was compiled against the repository's pinned uWebSockets/uSockets dependencies and tested against the real FastAPI service on loopback. Validated recommendations, SVG delivery, malformed paths, size limits, query-log privacy, client abort and backend outage fallback.
- Docker Compose 2.32.4 configuration passed both with and without `icons`. The existing SMTP variables were unset in this isolated configuration check.

Docker daemon was unavailable in the local executor. Full image build, container startup with no network, cgroup limits, main application's complete MongoDB-backed startup and expanded authenticated media API tests must pass the PR CI gates. `semantic-icon-engine.yml` supplies the offline synthetic-catalog container gate. `profile-workspace.yml` builds the main application and runs actual API/browser integration, including the newly extended five-section media/icon assertions. Keep the PR draft until those results are reviewed. A synthetic lexical container gate alone does not verify full-model memory limits.

## Repeat the checks

```sh
.venv-icons/bin/pip install pytest==8.3.4 httpx==0.28.1
TEST_ICON_MODEL_DIR="$ICON_ARTIFACT_DIR/models" .venv-icons/bin/python -m pytest services/semantic-icon-engine/tests -q
node --test tests/profile/test_autosave.mjs tests/profile/test_content_autosave.mjs
# With the existing profile browser dependencies installed:
node tests/profile/icon-picker-browser-check.cjs
node tests/profile/workspace-browser-check.cjs
node tests/profile/public-browser-check.cjs
cmake -S tests/profile -B build/profile
cmake --build build/profile --parallel 2
ctest --test-dir build/profile --output-on-failure
.venv-icons/bin/python services/semantic-icon-engine/scripts/evaluate.py \
  --data "$ICON_ARTIFACT_DIR/data" --output /tmp/lexical.json
.venv-icons/bin/python services/semantic-icon-engine/scripts/evaluate.py \
  --data "$ICON_ARTIFACT_DIR/data" --models "$ICON_ARTIFACT_DIR/models" \
  --vectors "$ICON_ARTIFACT_DIR/vectors" --output /tmp/fp32.json
.venv-icons/bin/pip install onnx==1.17.0
.venv-icons/bin/python services/semantic-icon-engine/scripts/quantize.py \
  "$ICON_ARTIFACT_DIR/models" /tmp/models-int8
.venv-icons/bin/python services/semantic-icon-engine/scripts/manage.py index \
  --data "$ICON_ARTIFACT_DIR/data" --models /tmp/models-int8 --vectors /tmp/vectors-int8
.venv-icons/bin/python services/semantic-icon-engine/scripts/evaluate.py \
  --data "$ICON_ARTIFACT_DIR/data" --models /tmp/models-int8 \
  --vectors /tmp/vectors-int8 --output /tmp/int8.json
```

Full-model container acceptance on the deployment host should repeat this corpus plus long descriptions and bursts under `--cpus 1 --memory 1536m`, with egress denied and read-only artifacts, before semantic mode is enabled. Record throttling, OOM kills, latency, fallback rate and restart behavior. For a much larger approved catalog, measure the scan cost again before choosing an approximate-nearest-neighbor index.
