# Public profile and owner preview, October 2026

Implements the eight October 1 reference screens on top of master
`7202c9fdddf7f3ae77d4e5e0fe3c4c726c93cf63`.

## Presentation

- RTL Vazirmatn layout, purple orbit background, sticky navigation, responsive
  portrait/name/availability hero and technology strip.
- Featured project case study, work timeline, project/publication cards, skills,
  education, certificates, recommendations, services, achievements, languages,
  contact methods, open source contributions and cooperation opportunities.
- Real data determines which sections, navigation entries and calls to action
  appear. Empty/hidden sections do not create blank placeholders. Skill meters
  show labelled discrete levels, not invented percentages.
- Project artwork uses actual uploaded media. Without it, a vector placeholder is
  displayed. Avatar and cover use the saved profile images. Reference portraits,
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

The C++ public route still performs the authoritative public projection. It emits
an inert, HTML-escaped JSON template with a bounded first batch of content and
already-public featured records. The initial server-rendered semantic content and
JSON-LD remain available to crawlers and when JavaScript is unavailable. The new
visual presentation progressively replaces that fallback. Neither private drafts
nor owner credentials are embedded in the public page.

Public pagination deliberately omits credentials, even when the visitor is also
signed in as the owner. Preview pagination uses its already-filtered draft.
Resume export loads all public pages, opens the details for browser print / Save
as PDF, then restores the original expansion state. Contact, navigation, sharing,
load-more, details and mobile menu controls are functional. External links accept
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
