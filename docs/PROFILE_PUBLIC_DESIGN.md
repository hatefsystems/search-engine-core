# Public profile and owner preview, October 2026

Implements the eight October 1 reference screens on top of master
`7202c9fdddf7f3ae77d4e5e0fe3c4c726c93cf63`.

## Presentation

- RTL Vazirmatn layout, purple orbit background, sticky navigation, responsive
  portrait/name/availability hero.
- Highlighted project case study, work history, project/publication cards, skills,
  education, certificates, recommendations, services, achievements, languages,
  contact methods, open source contributions and cooperation opportunities.
- Real data determines which sections, navigation entries and calls to action
  appear. Empty/hidden sections do not create blank placeholders. Skills show text levels and categories, without numeric meters.
- Project artwork uses actual uploaded media. Without it, cards use a compact text layout. Avatar and cover use the saved profile images. Reference portraits,
  employer logos, endorsements and sample numerical claims are not hardcoded.
- Eight technology icons from Simple Icons are bundled locally under CC0; their
  license is in `public/assets/images/tech/LICENSE.md`. The orbit illustration and
  product mark are code-native SVG. Figma conversion was unnecessary for these
  supplied image references; no Figma file was created.

## Shared rendering and privacy

`profile-public.js` renders both the visitor page and the editor's iframe preview.
The preview can expand into a keyboard-dismissable dialog. It follows the iframe
width so the same responsive rules apply in both contexts. It receives only an
allowlisted public projection of the current draft, including unsaved edits and
link changes. Messages require the same origin and the expected parent/frame.
Private contacts, hidden sections/items and their references are removed first.
Logout clears the preview data. The static server permits same-origin framing
only for the dedicated preview shell, with both X-Frame-Options and CSP
frame-ancestors; other static assets retain their deny policy. The real-backend
test asserts these response headers to catch iframe policy regressions.

The C++ public route performs the authoritative public projection. It emits
an inert, HTML-escaped JSON template with a bounded first batch of content and
already-public featured records. The initial server-rendered semantic content and
JSON-LD remain available to crawlers and when JavaScript is unavailable. The new
visual presentation progressively replaces that fallback. Neither private drafts
nor owner credentials are embedded in the public page.

Public pagination deliberately omits credentials, even when the visitor is also
signed in as the owner. Preview pagination uses its already-filtered draft.
Resume export waits for all public records before browser print / Save
as PDF. Contact, navigation, sharing and mobile menu controls are functional. External links accept
only HTTP(S), or validated email/phone destinations for contact cards.

## Verification

Local:

```sh
node --test tests/profile/test_autosave.mjs tests/profile/test_content_autosave.mjs
node tests/profile/workspace-browser-check.cjs
node tests/profile/public-browser-check.cjs
```

Browser dependencies are isolated from the application. The public suite checks
320, 390, 768, 1024, 1280, 1440 and 1920 pixels, privacy projection, hidden
references, pagination, keyboard-accessible controls, print preparation, missing
data, malicious text and unsafe URL schemes. The editor regression suite covers
all fourteen section schemas and the existing autosave/conflict/offline flows.

The profile workflow builds the actual C++ app, runs seven short C++ test targets,
135 API checks, basic/advanced editor integration and the populated profile test
against disposable MongoDB/Redis services. The latter checks the real public page
at five widths, public/preview identity and introduction parity, public pagination
and the expanded owner preview. No production data is changed.

Visual assets are an implementation of the supplied layout, not a claim of pixel
identity: a profile's actual content, media, section order and visibility determine
the resulting card density and page height.

## October 1 readability revision

All populated public fields are directly visible. Collapsed details and manual
load-more controls have been removed. The initial server payload stays bounded;
remaining records load automatically in batches of up to 100, without credentials.
Requests are sequential, abort when a preview render is replaced, and expose a
retry action only on failure. Printing waits for complete data. Contact records
are included in full in the public payload so the combined contact/link section
never silently loses contact methods.

The introduction is shown once. Project highlights appear on their full project
card rather than duplicating that card. Experience records use a date/organization
column alongside readable responsibilities and outcomes. Skills are grouped by
category with text levels rather than percentage-like meters. Empty artwork boxes
are removed. Real image galleries and evidence links remain accessible. The cover
replaces the default orbit decoration. The layout uses dark headings and readable
body text, content-sized cards and one combined contact/link section.

Regression coverage includes fully visible public fields, automatic network
pagination and recovery from a failed request, print preparation, hidden records,
seven viewport widths and populated real-backend public/preview parity.

## October 2 galleries and reading revision

Projects and work experiences each own an ordered image gallery. The editor shows
thumbnails, editable captions, previous/next controls and deletion. The API accepts
only uploads for that item, strips image metadata and checks profile/section/item
visibility on every fetch. Copying an item starts with an empty gallery.

The public layout uses grouped title/organization/date metadata, written month
names at the saved precision, compact skill rows and all populated project fields
in story order. Image captions are visible beside the related content. A sticky
desktop section index and mobile menu include every displayed section and track
the active section. An optional, separately editable tagline communicates value
below the professional title. Certificate dates and source links remain visible
when supplied; no verification claims are invented.

The expanded API tests cover ownership, caption/order persistence, removal and
visibility for both gallery types. The populated browser scenario uploads seven
related images, edits/reorders/deletes/reloads them, compares public and preview
captions, and waits for successful image decoding before screenshots. Illustrative
diagrams are explicitly labelled as samples; the Hatef project uses a real
screenshot of the running test editor.
