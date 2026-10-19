# Complete Persian drafts and independent acceptance

The ESCO Persian draft corpus covers every pinned ESCO 1.2.1 source ID. It is a machine translation work product, not a human-approved Persian edition. The release ranker continues to trust only the existing editorial concepts and explicitly reviewed overlays. Draft text is not silently added to trusted search aliases, embeddings, icon mappings or automatic profile selection.

## Translation provenance and coverage

| Prepared item                                       |   Count |
| --------------------------------------------------- | ------: |
| ESCO records with Persian draft titles              |  18,237 |
| Nonempty source definitions with draft translations |  18,104 |
| Source aliases with aligned draft translations      | 117,009 |
| Source-bound assistant title corrections            |     256 |
| Records flagged for priority human review           |   1,299 |
| Human-approved ESCO translations                    |   **0** |

The assistant corrected titles that the neural translator left in Latin text, plus one untranslated definition, using the original English definitions. These edits are recorded separately in `config/esco-fa-corrections.json` and are also **not human-reviewed**. Some technical aliases retain Latin names and remain flagged. Unflagged records are not presumed accurate. [Coverage evidence](measurements/esco-fa/translation-coverage.json) and [human-work readiness](measurements/esco-fa/human-review-readiness.json) distinguish preparation from actual human approval.

`services/semantic-icon-engine/config/esco-fa-drafts/manifest.json` records the exact source archive SHA-256, model revision, file checksums, coverage and structural warnings. Its JSONL shards contain a draft title, complete description where the source provides one, and one draft for every source alias in original alias order. Original English is preserved in the pinned archive and immutable bank. Every draft is bound to its original source record by `sourceSha256`; changing an English title, description, aliases, URI or version invalidates that binding.

The translator is `quickmt/quickmt-en-fa`, revision `e6b227180baab6a3e3ca9a07e08c96debb40bbe7`, under CC BY 4.0. It runs locally with CTranslate2 INT8 and beam size 5. The administrative environment is separate from runtime dependencies. Weights and tokenizers are verified against `config/translation.lock.json`; model code is not downloaded or executed. Source and model downloads happen only through explicit administrative commands.

```sh
python3.12 -m venv .venv-esco-translation
.venv-esco-translation/bin/pip install --require-hashes \
  -r services/semantic-icon-engine/requirements-translation.txt
.venv-esco-translation/bin/python services/semantic-icon-engine/scripts/translate_esco.py \
  download-source --archive /tmp/esco-en-1.2.1.zip
.venv-esco-translation/bin/python services/semantic-icon-engine/scripts/translate_esco.py \
  download-model --model /tmp/quickmt-en-fa
.venv-esco-translation/bin/python services/semantic-icon-engine/scripts/translate_esco.py \
  generate --archive /tmp/esco-en-1.2.1.zip --model /tmp/quickmt-en-fa \
  --cache /tmp/esco-translations.sqlite --output /tmp/esco-fa-drafts
```

The optional dependency lock targets CPython 3.12 on Linux x86-64. The SQLite cache is keyed by the translation specification and complete source segment. Generation resumes after interruption. Long descriptions are split into complete sentence/token segments; they are not truncated to the model window. Paragraphs and bullet items are preserved as segment boundaries. A decoding-limit hit triggers recursive subdivision and is recorded for review. Any untranslatable fragment is retained from the source and explicitly flagged; it is never silently omitted. Sentence case and terminal punctuation are added to phrase inputs. Unambiguous exact English aliases may use the existing assistant-authored Persian glossary, which is still not a human audit.

Warnings identify untranslated/script anomalies, unusually long titles, unknown tokens and shared Persian titles. They prioritize manual review; they do not prove that unflagged translations are correct. Occupational, medical, scientific, legal and technical terminology can be wrong even when fluent. Proper names and programming-language names may legitimately retain Latin text. All records remain `machine-draft` with an empty reviewer. No tool invocation or generated number counts as human approval.

## Human translation review

The full-image CI publishes a `persian-review-handoff` artifact with the local HTML workbench, the complete approved icon catalog and 100-record translation packets. Flagged records are first. To reproduce packets using the normal icon-engine Python environment:

```sh
python services/semantic-icon-engine/scripts/human_review.py translations \
  --archive /path/to/esco/dataset.zip --output /tmp/persian-review/translations
python services/semantic-icon-engine/scripts/human_review.py export-catalog \
  --data /path/to/artifacts/data --output /tmp/persian-review/catalog.json
```

Open `review.html` in a browser, choose a packet, enter the real reviewer's identity and optionally load `catalog.json` for artwork review. Compare each English title, definition and alias with its Persian draft. Correct specialist terminology, ambiguity, unnatural phrasing and translated brand names. Use the English definition to distinguish occupations with similar titles. Keep aliases as direct equivalents; related tools and occupations are not interchangeable skills. Each explicitly approved row gets personal human-review attribution. Editing an approved row returns it to pending. Download the saved JSON before changing packets or closing the page. The workbench works offline and sends no data to a server. Downloaded packets must be stored durably by the review coordinator.

Import a returned packet only after actual personal review:

```sh
python services/semantic-icon-engine/scripts/human_review.py import-translations \
  --archive /path/to/esco/dataset.zip --data /path/to/artifacts/data \
  --input /path/to/translation-review-000-saved.json \
  --output /tmp/knowledge-fa-reviewed.json
```

The importer checks the source/draft snapshot, source identities and immutable English fields, accepts only rows with both `translationStatus=reviewed` and `humanReviewed=true`, and validates reviewer attribution and icon IDs. Pending rows stay pending. Review the resulting configuration diff, replace `config/knowledge-fa.json`, and rebuild offline artifacts. Reviewed overlays then use the existing trusted bank path. This is the activation point. Importing a draft file directly as a reviewed configuration is rejected.

Names and review checkboxes are attestations, not identity authentication. The coordinator must verify who reviewed which fields and route specialist content to qualified people. Do not attribute an assistant's linguistic edits to a human reviewer.

## Independent 500-query acceptance

The old `evaluation-fa-500.json` and the assistant-drafted candidate set are development material. Neither becomes independent through a new filename, randomized order, a second model or repeated CI. The new protocol starts with **500 empty phrase slots**, balanced across the existing 25 domains. Humans independent from engine development supply unseen Persian phrases, ideally from consented real users. Each phrase records that authorship and independence attestation. No expected icons, suggested phrases or model predictions are prefilled.

1. Pin the exact engine revision and catalog before collection. Use the blank `acceptance/authoring.json` from the CI handoff or create a study below. Human authors fill each phrase and its source attribution in the workbench.
2. The coordinator seals the phrase collection. The tool rejects development overlaps, duplicates, missing human attribution and broken domain quotas. Keep the sealed phrases and subsequent labels away from developers until the acceptance run.
3. Export separate packets for two distinct human annotators who did not participate in engine development or author these queries. Each independently chooses all acceptable icons or explicitly records that the full catalog lacks suitable artwork. The workbench presents the complete catalog in canonical ID order with a manually entered substring filter. It never calls the ranker, auto-searches the phrase, shows scores or reveals the other annotator's choices.
4. If the two acceptable-icon sets differ, a third person resolves the disagreement. Use `export-adjudication` to produce a packet containing only disagreement cases. It shows the two prior human label sets for resolution, without engine predictions. A third person completes every disagreement. The coordinator can compare the two independent packets after both are returned. The third person must remain blind to engine results.
5. Freeze the reviewed labels and run the pinned engine once. Missing artwork, empty results and explicit no-suitable-artwork cases remain in the denominator. Do not tune on this set and then report its score as independent acceptance.

```sh
python services/semantic-icon-engine/scripts/human_review.py new-study \
  --engine-revision <exact-40-character-engine-commit> \
  --catalog-version <prepared-catalog-version> --output /tmp/holdout/authoring.json
python services/semantic-icon-engine/scripts/human_review.py seal-queries \
  --input /path/to/authoring-saved.json --output /tmp/holdout/sealed.json
python services/semantic-icon-engine/scripts/human_review.py export-labels \
  --input /tmp/holdout/sealed.json --reviewer '<real-annotator-A>' \
  --output /tmp/holdout/annotator-a.json
python services/semantic-icon-engine/scripts/human_review.py export-labels \
  --input /tmp/holdout/sealed.json --reviewer '<real-annotator-B>' \
  --output /tmp/holdout/annotator-b.json
python services/semantic-icon-engine/scripts/human_review.py export-adjudication \
  --input /tmp/holdout/sealed.json --first /path/to/annotator-a-saved.json \
  --second /path/to/annotator-b-saved.json --data /path/to/artifacts/data \
  --reviewer '<real-adjudicator>' --output /tmp/holdout/adjudication.json
# Only needed when the two raters disagree.
python services/semantic-icon-engine/scripts/human_review.py freeze \
  --input /tmp/holdout/sealed.json --first /path/to/annotator-a-saved.json \
  --second /path/to/annotator-b-saved.json --data /path/to/artifacts/data \
  --output /tmp/holdout/frozen.json
# Add --adjudication /path/to/third-person-saved.json for disagreements.
python services/semantic-icon-engine/scripts/benchmark.py \
  --data /path/to/artifacts/data --models /path/to/artifacts/models \
  --vectors /path/to/artifacts/vectors --queries /tmp/holdout/frozen.json \
  --require-independent --engine-revision <same-exact-engine-commit> \
  --acceptance-manifest /tmp/holdout/frozen.json.report.json \
  --output /tmp/holdout/acceptance.json
```

Freeze refuses existing output files, unreviewed cases, invalid artwork, altered phrases, duplicated raters, author/annotator overlap, unblinded attestations and unresolved disagreements. Its report records packet hashes, the complete frozen-label digest, exact-set agreement and adjudication counts. The benchmark requires that report and rejects changed labels. Protect the report alongside its source packets; this detects accidental changes, not a person rewriting both labels and manifest. The benchmark checks all 500 cases, source-domain quotas, query digest, two-rater evidence and pinned engine/catalog identities. The command-line engine revision is a caller-supplied attestation and must match the image/repository actually executed; the tool does not cryptographically attest the running binary. Keep input packets and reports together for audit.

`--require-reviewed` remains a compatibility mode for attributed review datasets. Only `--require-independent` performs the additional independent-protocol checks. Neither flag authenticates individuals or proves that they followed blinding instructions. CI validates preparation and synthetic protocol fixtures; it cannot perform or certify a human evaluation.

## Remaining external work

The repository contains the complete machine draft corpus, a review workbench, strict import/freeze checks and the CI handoff. **Human review of the translations and independent relevance acceptance are pending until real reviewers return their work.** No genuine human acceptance score has been produced. The historical 93.4% remains a development measurement and a translated draft corpus alone does not establish relevance for all jobs or skills.

The optional `Prepare Persian ESCO machine drafts` workflow uses 16 deterministic source-ID partitions and a verified merge. It is manually dispatched, or triggered only by designated preparation branches. It does not run bulk translation on every PR, alter runtime aliases, publish images or approve human work. The merge verifies each partition against its exact source IDs and recomputes duplicate-title warnings globally. When source-bound assistant corrections are present, `correct_fa_drafts.py` applies them after merge while preserving machine-draft status. It can also be run locally with `--archive`, `--drafts` and `--output` to reproduce the corrected corpus in a new directory.
