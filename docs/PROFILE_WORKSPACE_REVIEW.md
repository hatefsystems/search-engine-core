# Profile editor review, 2026-09-28

Reviewed the merged master commit `f155770f139b62e128e5b209f7c8ae3ce6d98024`
against the nine supplied reference images. The merge contained exactly the
previously delivered workspace files.

## Visual findings

The desktop uses the reference's RTL arrangement: section navigation on the right,
editor in the center, live preview on the left, with Vazirmatn and purple accents.
The mobile layout uses separate editing and preview tabs.

This is not a pixel-identical reproduction of the generated reference images.
The references include sample portraits, organization/certificate logos, custom
icons, verification marks, project artwork, richer tag controls and several
combined-section layouts. The implementation retains individual section forms,
plain-text content fields, partial-date controls, and the API's supported privacy
states. No verification badges, third-party endorsements, GitHub star counts or
reference-person statistics are fabricated.

This review improves text readability, sidebar density, date grouping, character
counts, featured labels, and organization/date/technology summaries in preview.
The sample cover and HR monogram are generated test artwork, not the user's photo.

## Functional fixes

Structured `about` and `availability` content overrides the old header fields in
the public data model. The basic form previously continued editing those old
fields, so the public result could differ from the saved form. Basic introduction
and availability edits now update the corresponding structured records, and the
preview respects their visibility. A new record in an explicitly emptied section
remains private until its owner selects public visibility.

The preview now recognizes the ADVANCED skill level. The second-tab integration
test explicitly opens Projects, because a new tab correctly starts on Basic.

## API coverage

The existing backend already provides schema, CRUD for all 14 sections, complete
ordering, layout/featured/privacy, completion, profile publication, owner sessions,
avatar/cover uploads, project media, and versioned links. No new endpoint is
required for the implemented forms.

The isolated integration workflow builds the actual C++ application, runs seven
C++ test targets, starts MongoDB and Redis in disposable Docker services, and runs
135 API assertions plus the basic and advanced browser suites. It does not use the
Node fixture for backend validation. Tests cover ownership, stale versions,
visibility, references, media handling, persistence, public rendering and search.

The fixture suite separately covers all schema fields, autosave/offline/conflict
behavior and seven widths: 320, 390, 768, 1024, 1280, 1440 and 1920 pixels.

## Populated sample and screenshots

`tests/profile/demo-browser-check.cjs` creates 27 records across 14 sections using
the API, exercises edits through the real UI, reloads to verify persistence,
uploads test avatar/cover PNGs, captures desktop/tablet/mobile screenshots, checks
publication, and deletes the temporary profile. Professional content is based on
the supplied background. Unknown publication and recommendation records are
explicitly marked synthetic and remain hidden.

Run against a disposable backend:

```sh
PROFILE_TEST_BASE_URL=http://127.0.0.1:3019 node tests/profile/demo-browser-check.cjs
```

The explicit `--fixture` flag supports UI-only review without a backend. The
generated `validation.json` distinguishes those modes. Screenshots and diagnostic
logs are retained in the workflow artifact; no real production profile is edited.

Latest code validation run:
https://github.com/hatefsystems/search-engine-core/actions/runs/36419907767
