# Advanced personal profiles

This document describes the implemented personal-profile platform. Business profiles retain their existing routes and model. Identity federation, account recovery, payments, social feeds and automated imports are outside this delivery.

## Data and public projection

`PersonProfile.content` embeds schema version 1 and fourteen typed sections: `about`, `experiences`, `projects`, `skills`, `education`, `certifications`, `publications`, `openSource`, `services`, `achievements`, `languages`, `recommendations`, `contacts`, and `availability`. The DTO definitions and field defaults are in `ProfileContent.h`; the schema endpoint exposes these same definitions to the editor. Unknown sections and fields are rejected.

Every item has a stable client-generated lowercase ID (8–64 letters/digits/hyphens), server timestamps, display order and `PUBLIC`/`HIDDEN` visibility. References use item IDs in the same profile. Limits are 50 items per section, 100 skills, 20 languages/availability entries, 10 evidence references or project images per item, six featured references and 2 MiB serialized embedded content. Individual JSON mutation requests have a 64 KiB cap; media requests have an 8 MiB encoded-body cap.

Partial dates retain `{calendar, year, month, day}` with zero denoting an omitted component. A year does not become January 1. Supported calendars are `persian` and `gregory`. Gregorian dates use C++ calendar validation; Persian dates use the civil 33-year rule. Date text uses Persian labels/digits and identifies Gregorian input explicitly. Far-future astronomical calendar corrections are not implemented.

Absent sections are converted from legacy skills, school/education and languages on read without writing. IDs are deterministic. The first mutation of a section materializes that section only; an initialized empty section suppresses fallback permanently. Legacy skill changes preserve matching item evidence and details; removing a skill removes dependent references. Legacy fields outside the editor remain untouched.

`publicPersonProfile()` is the shared projection for HTML, public JSON, section pagination, JSON-LD, media authorization and derived search. Hidden profiles/sections/items take precedence. References and featured choices pointing to hidden items are removed. Header privacy also suppresses email/phone contact items and availability. Ownership keys, hashes and owner identifiers never enter public serialization. Owner-only completion data is omitted. Templates escape all user strings; JSON-LD escapes script delimiters separately. Public HTML revalidates; owner APIs and media use `no-store`.

## Atomic writes and versions

Content, order, visibility, featured choices, header privacy, publication and `publicSearch` update in the same profile document with a compare-and-set filter on `version`, followed by a version increment. A stale version returns `409`. Deleted profiles cannot be updated. This follows MongoDB's [single-document atomic update semantics](https://www.mongodb.com/docs/manual/core/write-operations-atomicity/).

Create-item requests contain a stable item ID. Replaying an identical creation returns the existing item without another insert or version increment. Reusing that ID with different data returns `409`. Delete prunes related IDs and featured selections in the same profile write. Profile reads do not migrate documents.

All content, layout, media, link and legacy header/image/skill mutations verify the existing profile ownership credential. Cookie mutations require matching origin. The shared owner/profile mutation budget is 120 per minute; `429` includes `Retry-After`. Existing 64-character keys, hashed storage, legacy keys and 30-day `HttpOnly; SameSite=Strict` cookies remain supported; cookies use `Secure` in HTTPS.

Link blocks remain in `link_blocks`, with an independent integer `version`. Link update/delete requests must send that version. They preserve unrelated link properties. A stable 24-character hexadecimal creation ID makes retries idempotent. New `visibility: HIDDEN` closes list, detail and redirect access. Legacy `privacy: HIDDEN` with `visibility: PUBLIC` remains an unlisted, working redirect. Only active, public links appear in HTML/SEO. Public link APIs also check parent-profile visibility. Link versions do not advance profile versions.

## API

Existing response envelopes and public route aliases remain. Content mutations return the updated owner profile in `data`; its public counterpart places new content in `data.sections`.

| Method and route                                                    | Behavior                                                                |
| ------------------------------------------------------------------- | ----------------------------------------------------------------------- |
| `GET /api/profiles/:id/content-schema`                              | Typed section defaults, limits and enum options; no profile information |
| `GET /api/profiles/:id/content/:section?offset=0&limit=20`          | Owner or filtered public items, total and version                       |
| `POST /api/profiles/:id/content/:section`                           | `{version, item: {id, ...fields}}`; new items default to hidden         |
| `PUT /api/profiles/:id/content/:section/:itemId`                    | `{version, item: {...changedFields}}`; partial update                   |
| `DELETE /api/profiles/:id/content/:section/:itemId`                 | `{version}`; prune references and featured choices                      |
| `PUT /api/profiles/:id/content/:section/order`                      | `{version, ids: [...]}`; exact complete, unique ordering                |
| `GET /api/profiles/:id/layout`                                      | Owner-only layout                                                       |
| `PUT /api/profiles/:id/layout`                                      | `{version, order?, visibility?, featured?, goal?, privacy?}`            |
| `GET /api/profiles/:id/completion`                                  | Owner-only goal, score and actionable recommendations                   |
| `POST /api/profiles/:id/projects/:itemId/media`                     | `{version, image: base64OrDataURL, alt?}`                               |
| `GET /api/profiles/:id/media/:mediaId`                              | Access-controlled rewritten WebP                                        |
| `POST /api/profiles/:id/links`                                      | Link creation; optional stable `id`, initial version zero               |
| `PUT /api/profiles/:id/links/:linkId`                               | Independent link `version` and changed fields                           |
| `DELETE /api/profiles/:id/links/:linkId`                            | Independent link `{version}`                                            |
| `GET /api/people?q=&skill=&location=&availability=&page=1&limit=20` | Public people, total and public facets                                  |

Example project creation:

```json
{
  "version": 4,
  "item": {
    "id": "8ebaa41a-b0cf-47e1-85ed-286ee34cbf17",
    "title": "پروژهٔ دانشجویی",
    "problem": "مسئله‌ای که حل کردم",
    "visibility": "HIDDEN"
  }
}
```

Public items need their section's title field. Public recommendations additionally need text and a valid external source; public contacts require a valid type/value. Public evidence needs a title and HTTP(S) URL. Owners cannot set a verification status. Recommendations and evidence are labelled self-reported; a source is not an endorsement or proof of authenticity. Hidden drafts can be incomplete. HTTP(S) links are rendered safely and never fetched by the server for previews.

## Editor and public page

The existing short creation flow remains. Opening a free slug creates nothing. Explicit creation starts a private draft and reveals the key once. The editor offers all sections, keyboard-operable up/down buttons, item and section visibility, featured choices, evidence, partial dates, related-item selection, project images, link editing and live preview. Header settings control public city/contact/availability information. English-name editing remains absent.

Both header and content autosave use a 600 ms debounce and two-second maximum while typing. Profile mutations share a sequential version queue; new edits survive late acknowledgments. Only successful responses clear pending values. Per-tab local drafts survive connection failures and reloads. `409` requires explicit comparison/reconciliation; item updates carry changed fields only. Link conflicts have separate comparison controls. Publication waits for header, item and link saves. After first publication, saved public edits appear immediately. Logout clears pending drafts across tabs and closes owner access.

The public page preserves Vazirmatn, RTL and the purple/white theme, starts with three cards per section, and fetches more on request. Case studies expand inline. Hidden/empty sections are absent. Images load lazily. The public page has no deep project routes. Completion uses goal-specific weights for identity, work, skills, evidence and reachable presence; one substantial student/independent/volunteer project can replace employment history. The score is capped and never used in search ranking.

## Media

Only static JPEG, PNG and WebP inputs up to 5 MiB are accepted. libvips actually decodes and re-encodes to WebP, applies orientation and strips metadata. Inputs are limited to 8,000 pixels per dimension and 16 megapixels; outputs fit 1,600 pixels. Two decode workers bound concurrent processing. Animated PNG/WebP chunks and multipage images are rejected. No URL fetcher is used for evidence, PDF or video links.

`PROFILE_MEDIA_DIR` defaults to `profile-media`, outside the static roots. Production Compose mounts the persistent `profile_media` volume at `/app/profile-media`. Image GET checks current parent/item/section visibility or owner access on every request. Failed uploads and version conflicts remove files; successful removal of a project/image removes its unreferenced file. A process crash between filesystem write and MongoDB commit can leave a file; operational orphan reconciliation is still needed after such a crash. Soft-deleted profiles keep their referenced files for restoration.

Docker dependency bases install `libvips-dev` for builds and `libvips42` at runtime. The root Dockerfile uses the updated pinned base digests. See the [libvips API](https://www.libvips.org/API/current/) for decoder and encoder behavior.

## People search and indexes

`publicSearch` is a derived object embedded in each personal profile. Header/content/privacy/publication mutations replace it in the same atomic write. Private or deleted profiles have no searchable payload. Contact sections, testimonial text and URLs are excluded. Search reads MongoDB directly, independently of the web-page search index.

Indexes:

- `people_public_text`: text over `publicSearch.name`, `.title`, `.text`, with `default_language: none`.
- `people_public_city`: `{type: 1, isPublic: 1, publicSearch.location: 1}`.
- `people_public_skill`: `{type: 1, isPublic: 1, publicSearch.skills: 1}`.

There is no compound multikey index across multiple arrays. Arabic kaf/yeh and whitespace normalize to Persian forms. Skill filters retain `C`, `C++`, and `C#`; text indexing/querying uses matching encoded tokens for the meaningful punctuation. Counts and facets use the same public filters. No completion or alleged-verification score affects ranking. Query pages are 1–100, with 1–50 results per page. `/people` is registered before `/:slug`.

## Migration and rollback

No read-time document migration runs. Existing profiles become searchable on their next authorized update. For existing profiles that must immediately appear in people search, run the explicit idempotent reindex using the application's normal encryption key and the intended MongoDB URI:

```sh
./server --profiles-reindex --dry-run
./server --profiles-reindex --apply
```

Dry-run inspects documents without changing their content or derived search. Storage initialization ensures named indexes. Apply only writes `publicSearch` guarded by each observed version; a concurrent change stops the command with an error so it can be safely rerun. Slugs, keys, content and profile versions are not rewritten. Back up the database and media volume before operational rollout.

Rollback: retain `content`, link `version`/`visibility`, and the media volume. Disable new writes, restore the previous application image, and remove only the three named people indexes and the derived `publicSearch` field if desired. Do not delete advanced content to roll back. **An old image does not understand new link visibility or advanced privacy overrides**; do not expose old public routes for profiles that rely on those controls until public access has been restricted. A database/media backup is the complete rollback boundary.

## Verification

See [the verification report](ADVANCED_PROFILES_VERIFICATION.md) for actual results and unresolved failures. Commands:

```sh
cmake -S tests/profile -B build/profile-advanced
cmake --build build/profile-advanced -j4
ctest --test-dir build/profile-advanced --output-on-failure
node --test tests/profile/test_autosave.mjs tests/profile/test_content_autosave.mjs

docker compose -f docker-compose.profile-test.yml build app tests
docker compose -f docker-compose.profile-test.yml up -d mongo redis app
docker compose -f docker-compose.profile-test.yml ps
docker compose -f docker-compose.profile-test.yml run --rm tests

PROFILE_TEST_BASE_URL=http://127.0.0.1:3019 python3 tests/profile/advanced-api-check.py
PROFILE_TEST_BASE_URL=http://127.0.0.1:3019 node tests/profile/advanced-browser-check.cjs
```

Browser tests require Playwright/Chromium. Existing integration suites additionally require `pymongo` and explicit `PROFILE_TEST_MONGODB_URI=mongodb://127.0.0.1:27039`, `PROFILE_TEST_DATABASE=search-engine`. Fixtures use only the isolated database. Shut down this test project with `docker compose -f docker-compose.profile-test.yml down -v` after collecting results; its volume contains only test media.
