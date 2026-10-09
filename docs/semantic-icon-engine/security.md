# License policy and security

## Approval policy

`config/sources.lock.json` pins official Lucide 0.468.0, Simple Icons 14.0.0, an Iconify source commit and an E5 model revision. `config/policy.json` allows explicitly enumerated SPDX identifiers and binds Iconify reviews to a source revision, SPDX value and original notice. An allowlisted SPDX identifier alone does not publish an Iconify collection. All packs are discovered, including excluded packs, and their reasons are retained.

The initial policy approves the direct Lucide set and the reviewed Iconify `lucide` collection. It includes the original Lucide/Feather notices. Other collections require review of the exact source revision and attribution terms. The reported exclusions are policy decisions, not legal conclusions that all excluded artwork is unusable. This implementation supports all reviewed collections; it does not claim that the default approved subset is the complete Iconify universe.

Simple Icons branding is opt-in through exact IDs in `publicBrands`. Individual icon licenses override the collection default. An unapproved individual license is excluded even if its brand is allowlisted. An allowed non-CC0 individual license also needs a matching `iconLicenseReviews` entry with original license text. Trademark/public exposure review remains distinct from copyright licensing. No brands are exposed by default.

Example policy fragment, to be completed after review:

```json
{
  "reviewedCollections": {
    "reviewed-prefix": {
      "revision": "EXACT_ICONIFY_SOURCE_REVISION",
      "license": "MIT",
      "licenseText": "ORIGINAL COMPLETE NOTICE",
      "style": "outline",
      "brands": false
    }
  },
  "publicBrands": ["simple-icons:reviewedbrand"],
  "iconLicenseReviews": {
    "simple-icons:reviewedbrand": {"license":"MIT","licenseText":"ORIGINAL COMPLETE NOTICE"}
  }
}
```

Do not use those placeholder values in production. Keep original source notices and all required attribution in the artifact catalog and bundle. `/v1/icons/sources` exposes them to consumers. Original records, source revisions, Iconify pack SHA-256 hashes, aliases, dimensions and transformation metadata remain traceable. Local SVG content hashes deduplicate storage while preserving distinct canonical IDs.

## SVG boundary

The importer accepts a small declarative subset: shapes, groups, safe gradients, clips, masks, title and description. It rejects scripts, event attributes, styles, foreign namespaces, use/image references, XML entities/declarations, external resources, unsupported elements, invalid viewBoxes and unresolved local references. Size, depth, node count and attribute length are bounded. Unsupported artwork is rejected with a reason, never repaired by embedding arbitrary markup.

All accepted SVGs have deterministic attribute ordering, normalized 24×24 display dimensions, preserved coordinate viewBox and retained safe multicolor fills/strokes. Canonical IDs and asset paths are validated independently in C++, JavaScript and Python. Browser rendering uses `<img>`; no raw SVG is inserted into profile HTML. SVG responses use the correct MIME type, `nosniff`, restrictive CSP and bounded cache lifetime.

## Request and filesystem boundaries

The public bridge accepts fixed API/asset path shapes and a configured internal HTTP origin. Clients cannot supply a remote fetch URL. Redirect following and proxy environment inheritance are disabled in curl. Bodies, queries, responses and concurrent forwards are bounded. Aborted connections are tracked on the event loop before sending asynchronous responses.

The private service applies a bounded 4096-entry token-bucket table, default 120 requests per minute per client, and one inference slot. The C++ bridge derives `X-Icon-Client` from the actual peer address, ignoring browser-provided values. The private service trusts this header and must not be publicly published. Deployments behind a shared reverse proxy currently share that peer's bucket; choose an appropriate controlled rate rather than trusting arbitrary forwarded-IP headers.

The runtime user cannot write catalog/model/vector mounts. It cannot invoke ingestion, downloads or indexing via HTTP. Runtime startup loads local assets only. Catalog and model checksums are checked; vector identity must match model and catalog. The bundle adds per-file integrity checks, traversal/link rejection and expanded-size limits. Administrative sources and policy files remain trusted operator input.

The model manifest allows locally derived quantized variants with pinned parent checksums. Producing and installing such a variant is an administrative action. Artifact checksums are integrity controls, not a substitute for authenticating the internal artifact distribution channel.

## Resource behavior

Lexical requests, category lookup and SVG retrieval do not run model inference. Inference timeout falls back lexically while keeping the in-flight model slot occupied until completion. Busy work returns 429; there is no unbounded inference queue. SVG responses cap at 2 MiB at the bridge. Long-lived shutdown waits for an active inference to finish.

The full-catalog vector scan is bounded in process allocations but still reads every vector shard. Hundreds of thousands of approved icons may need a different index after measurement. A 425,222-row float32 matrix at 384 dimensions contains about 623 MiB of raw vector data, excluding metadata, IDs and page cache. This is a size estimate, not a measured full-catalog latency/RSS claim. Import uses one JSON source pack at a time and checkpoints processed icons; a large individual pack still determines peak parser memory. Keep bulk administration outside the runtime resource budget.
