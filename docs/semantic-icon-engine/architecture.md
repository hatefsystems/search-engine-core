# Architecture and API

The private Python service owns immutable SQLite catalogs, FTS5 search, sanitized SVGs and memory-mapped vector shards. A bounded asynchronous C++ bridge exposes same-origin URLs. MongoDB continues to store profile content, including `iconMode`, `iconId` and `iconMediaId`; no new database service is introduced.

Preparation fetches pinned upstream data. Ingestion visits all packs, enforces policy, preserves provenance and aliases, deduplicates SVG bytes, records every decision, then atomically publishes `current.json`. Import checkpoints commit every 100 approved/rejected icon attempts and between source packs. Only a completed catalog becomes active. Catalog startup verifies its SHA-256. Large generated artifacts and model weights are excluded from ordinary Git history and the main application Docker context.

E5 uses `intfloat/multilingual-e5-small`, revision `614241f622f53c4eeff9890bdc4f31cfecc418b3`, ONNX Runtime 1.20.1, CPUExecutionProvider. Queries use `query: `, documents use `passage: `. Token sequences are truncated at 512; attention-mask-weighted mean pooling and L2 normalization produce 384-dimensional float32 vectors. The embedding identity includes model metadata/checksums, quantization, normalization, metadata schema version and dictionary. Startup never embeds the catalog.

The runtime loads at most one 512-row vector matrix at a time and retains at most 150 nearest candidates. Model queries use a 128-entry LRU cache. One bounded inference slot prevents an unbounded executor queue. The C++ bridge allows eight active forwards, caps responses at 2 MiB, connects within 200 ms and times out at 2500 ms. SVG and empty catalog pages do not execute the model.

## Text and ranking

Normalization applies NFKC, Arabic/Persian ی/ک and digit equivalence, Persian combining-mark removal, ZWNJ/format-character separation, lowercase and tokenized whitespace. The versioned bilingual dictionary covers professions, common icon concepts and technology/brand aliases. Direct equivalents and related phrases are separate. Persian labels and direct aliases are indexed at preparation time, so `قلب`, `دل`, `AWS` and `ترافورم` work without inference. It is an association dictionary, not machine translation presented as human-reviewed text.

Candidates combine up to 150 FTS matches, up to 60 per matched category, and up to 100 vector matches. These are runtime candidate budgets, not ingestion limits. Default ranking is 0.4 cosine similarity + 0.4 category relevance + 0.2 lexical/synonym coverage. With no model, the two remaining weights are normalized. Small bonuses favor dictionary canonical terms, the requested source (Lucide by default) and style. Content hashes and Lucide-family names suppress duplicate suggestions without removing their catalog identities. Brand suggestions additionally require the name or a recognized direct brand alias in the query. Exact concept names receive a ranking bonus.

Scores are ranking scores and can exceed 1. They are **not probabilities**. `confidenceThreshold` and `confidenceMargin` live in policy; a low score or small top-two margin sets `lowConfidence`. If nothing matches, an existing `lucide:briefcase` is used as a low-confidence generic fallback. Unknown IDs are never fabricated. Optional context is returned to the caller; it does not currently change ranking weights.

Manual search combines up to 500 lexical matches and 150 semantic candidates. Exact names and direct equivalents lead, followed by canonical variants (for example `heart-pulse` after `heart`), then related concepts and remaining semantic candidates. Only semantic-only candidates above `searchSemanticThreshold` (default 0.75, a ranking cutoff rather than a probability) are included. Existing category/source/style filters apply to both candidate paths. Pagination slices this bounded stable ranking; identical content and Lucide mirrors are deduplicated. Empty queries retain ordinary catalog pagination. Missing, busy or timed-out inference falls back to the same lexical/synonym ordering without a fabricated generic result.

## Metadata and profile fields

Each icon exposes `id`, `source`, `name`, `labelEn`, `labelFa`, `keywords`, `categories`, primary `category`, `style`, `brand`, `hash`, `metadataVersion`, `persianProvenance`, `origin`, original `raw` metadata and same-origin `iconUrl`. `origin.sourceRecord` resolves through `/sources` to version, repository, notice, license and collection details. Iconify themes/prefixes/suffixes remain in source records; aliases remain in raw metadata.

Use `policy.metadataOverrides` for reviewed per-icon `labelFa`, `keywordsFa` and `keywordsEn`. For example:

```json
{
  "metadataOverrides": {
    "lucide:cloud": {
      "labelFa": "زیرساخت ابری",
      "keywordsFa": ["میزبانی سازمانی"],
      "keywordsEn": ["managed infrastructure"]
    }
  }
}
```

Changing the dictionary or policy produces a new catalog. Rebuild or safely reuse text-identical vectors. Automatically associated Persian labels say so; explicit overrides say `editorial override`.

Profile modes are `auto`, `manual`, `custom`, `none`. Old records default to `none`. The editor exposes search and Persian category labels (`labelFa`) while retaining source/style metadata internally. Categories count membership across all assigned categories, and filtering includes secondary categories. Selected cards use a purple border/background, a visible check badge and `aria-pressed`; the artwork itself is not recolored. Selection is recomputed after loading results, mode changes and reopening the editor from stored fields. The editor debounces title changes, shows three suggestions and adopts only a non-low-confidence candidate while still in auto mode. Generation counters and aborts prevent stale responses from overwriting manual selection. Custom mode references an image already owned by that item through the existing authenticated raster-upload pipeline. Copying an item does not copy image ownership. Supported editor sections are services, skills, projects, experiences and achievements. Shared public/preview rendering and server templates resolve the stable ID to an `<img>` URL; raw SVG never enters profile HTML.

## Endpoints

| Private endpoint                        | Same-origin route           | Behavior                                                        |
| --------------------------------------- | --------------------------- | --------------------------------------------------------------- |
| `GET /health/live`                      | Internal only               | Process liveness                                                |
| `GET /health/ready`                     | Internal only               | Catalog/model readiness; lexical mode is a valid degraded state |
| `GET /metrics`                          | Internal only               | Aggregate counters without query text                           |
| `GET /v1/icons`                         | `GET /api/icons`            | Catalog page                                                    |
| `GET /v1/icons/search`                  | `GET /api/icons/search`     | Hybrid multilingual catalog search                              |
| `GET /v1/icons/categories`              | `GET /api/icons/categories` | All-category counts and Persian labels                          |
| `GET /v1/icons/sources`                 | `GET /api/icons/sources`    | Source provenance and original notices                          |
| `GET /v1/icons/{id}`                    | `GET /api/icons/{id}`       | Stable ID metadata                                              |
| `POST /v1/icons/suggest`                | `POST /api/icons/suggest`   | Hybrid or fallback recommendation                               |
| `GET /assets/icons/{source}/{name}.svg` | Same path                   | Sanitized local SVG; Iconify adds the collection segment        |

Catalog parameters: `q` (500 chars), `category`, `source`, `style`, `limit` (1–100), `offset` (0–1,000,000). Responses contain `items`, `nextOffset`, `catalogVersion` and `engine`; degraded searches can include a `reason`. Category entries contain stable `category`, Persian `labelFa` and `count`. Recommendation accepts `text` (2000 chars), `title` (200), `description` (2000), category, context, preferredSource, preferredStyle and limit (1–20). Total request body is capped at 16 KiB. Unknown recommendation fields are rejected. No arbitrary fetch URL is accepted.

```sh
curl 'http://localhost:3000/api/icons/search?q=cloud&source=lucide&limit=3'
curl 'http://localhost:3000/api/icons/suggest' \
  -H 'Content-Type: application/json' \
  --data '{"text":"مشاوره حقوقی شرکت‌ها و تنظیم قراردادها","context":"service","limit":3}'
curl -D - -o /dev/null 'http://localhost:3000/assets/icons/lucide/scale.svg'
```

Actual response examples are recorded in [measurements/api-examples.json](measurements/api-examples.json).

Malformed IDs return 400, missing icons 404, invalid input 422, oversized bodies 413, exhausted limits 429, unavailable catalogs/bridge 503. Private SVG responses provide an ETag and support 304. The C++ bridge currently returns cacheable 200 SVG responses without forwarding ETag negotiation. Browser cache lifetime is one hour; deploy content updates with this cache window in mind.
