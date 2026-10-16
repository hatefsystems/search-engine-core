# Persian occupation and skill knowledge

This iteration addresses the ranking failures measured in the frozen 500-query development set. It does not claim universal coverage or independently verified 90% relevance.

## Prepared data

`config/knowledge.lock.json` pins the official English ESCO 1.2.1 CSV archive by URL and SHA-256. The importer deduplicates repeated source URIs and preserves original English labels, aliases, descriptions, source version and relationships. Internal IDs are UUID5 values derived from each source URI, independent of translated titles. Changes in Persian spelling do not change concept identity.

| Item                                                      |   Count |
| --------------------------------------------------------- | ------: |
| Unique ESCO occupations                                   |   3,039 |
| Unique ESCO skills                                        |  13,939 |
| Occupation/skill hierarchy groups                         |   1,259 |
| Deduplicated relationships                                | 156,336 |
| Initial Persian editorial concept families                |     267 |
| Explicit editorial icon links                             |   1,039 |
| Missing artwork referenced by the editorial configuration |       0 |

The Persian families cover common professional concepts, trades, care, education, arts and technology aliases. They are assistant-authored editorial configuration, not an independent human audit. They do not constitute a full Persian translation of ESCO. All 18,237 ESCO records without a reviewed Persian overlay retain `translationStatus=untranslated` and original English definitions. E5 provides cross-language retrieval for them. Local concepts may have multiple related icons; a generic icon is not evidence of a unique pictogram for every specialist skill.

Two additional reviewed Material Design Icons, `tooth` and `lungs`, fill specific artwork gaps. This is an explicit subset, not approval of every Iconify collection. Existing Simple Icons and Devicon catalogs remain available with original brand colors.

## Mapping and ranking

The immutable SQLite bank stores concepts, FTS text, source relationships and icon mappings. Complete E5 vectors are prepared for both icons and concepts. Precomputation produces at most three distinct icon proposals for each ESCO record:

- `curated`: explicit Persian editorial or reviewer-provided mapping.
- `curated-rule`: an English title matches a known editorial alias; this remains an unreviewed rule result.
- `proposed`: E5 icon similarity exceeds the proposal threshold; this remains an unreviewed machine suggestion.

Explicit Persian aliases work without inference. Unknown phrases use multilingual concept and icon retrieval. Weak semantic evidence cannot authorize automatic profile selection. Both manual search and automatic suggestions use the same ranker, and source preference only breaks ties. Manual category filters remain supported. The editor shows Persian categories and keeps selected-card styling separate from the artwork colors.

The private `/v1/concepts/search` and `/v1/concepts/{id}` endpoints make the bank reusable. They are not exposed through the public C++ bridge. Profile skill autocomplete and searching people by expertise would require their own product and indexing integration; this change supplies the underlying data rather than claiming those features already exist.

## Review Persian ESCO overlays

Export a manageable batch from prepared artifacts:

```sh
python services/semantic-icon-engine/scripts/knowledge.py export-review \
  --bank /path/to/artifacts/knowledge --limit 100 --offset 0 \
  --output /tmp/esco-review.json
```

A reviewer fills `labelFa`, `aliasesFa`, `descriptionFa`, chooses final canonical icon IDs in `icons`, and sets `translationStatus=reviewed` with their `reviewer` attribution. Original titles and machine proposals remain visible as context. Pending proposals are not automatically accepted.

```sh
python services/semantic-icon-engine/scripts/knowledge.py import-reviewed \
  --bank /path/to/artifacts/knowledge --data /path/to/artifacts/data \
  --input /tmp/esco-review.json \
  --output /tmp/knowledge-fa-reviewed.json
```

Review the generated diff and replace `config/knowledge-fa.json` with the reviewed result. Rebuild prepared artifacts in a new output directory, or reuse text-identical vectors through the existing index mechanism. The importer rejects unknown IDs, duplicate overrides, unrecognized artwork, missing reviewer attribution and records without reviewed status. It cannot authenticate a person's identity or certify independence; that remains a review-process responsibility. Original English source fields and stable identities cannot be overwritten by this overlay.

## Evaluation and acceptance

`evaluation-fa-500.json` remains byte-for-byte unchanged. The original baseline evidence remains under `measurements/fa-500/`. The new ranker and Persian layer were developed with access to that set, so subsequent scores are development measurements. Do not present them as blind acceptance results.

`tests/evaluation-fa-holdout-review.json` contains 500 fresh, assistant-drafted review candidates, generated after the initial configuration freeze and not evaluated against the engine. Their phrases use templates and known editorial concepts. This is a review starting point, not an independent, representative or challenging human holdout. Every row is pending, has an empty final relevance list and separates proposed artwork from approved labels. Reviewers should replace templated language with real unseen user phrases, balance occupations/domains, and judge acceptable artwork independently before freezing a genuine acceptance set. Keep that reviewed set separate from development tuning.

The acceptance command refuses pending/unattributed labels:

```sh
python services/semantic-icon-engine/scripts/benchmark.py \
  --data /path/to/artifacts/data --models /path/to/artifacts/models \
  --vectors /path/to/artifacts/vectors \
  --queries /path/to/human-reviewed-holdout.json --require-reviewed \
  --output /tmp/acceptance.json
```

The target is an acceptable icon among the first three results in at least 90% of all reviewed cases. Empty results and missing expected artwork remain misses. Without `--require-reviewed`, reports explicitly set `acceptanceEvaluated=false` and `acceptancePassed=null`. CI reports the frozen development score; infrastructure success is not relevance acceptance. Human acceptance remains pending.

## Offline release

The preparation job downloads the pinned archive, verifies checksums, builds the bank, embeds all records and precomputes proposals. Packaging requires complete icon and concept coverage, normalized finite vectors, matching identities and file checksums. Runtime-only files are bundled into the release image. The deployment host pulls the tested image and performs no ESCO/model downloads, indexing or source import. The full-image CI probe verifies the bank, complete concept index, Persian examples and restart under the existing CPU/memory limits.

Source attribution: European Commission, ESCO 1.2.1, adapted with Hatef editorial overlays. Source/reuse notice: <https://commission.europa.eu/legal-notice_en>; ESCO handbook: <https://esco.ec.europa.eu/system/files/2021-07/Handbook.pdf>. Icon notices are preserved separately in the catalog and `licenses/`.

CI preparation uses a separate administrative budget of 2 CPUs, 4 GiB and two ONNX threads; runtime probes remain limited to 1 CPU and 1536 MiB. Concept documents are grouped by text length to reduce batch padding. On this bank, batch-8 padded input drops from 2,503,384 to 1,739,944 tokens (30.5%). A real-E5 padding-invariance smoke check confirmed agreement within 1e-5. Existing text-identical vectors remain compatible. Superseded pull-request image jobs are cancelled; trusted publication jobs remain serialized without cancellation.
