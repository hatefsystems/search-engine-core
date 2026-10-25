# Hatef legal pages and OAuth publication review

## Public URLs

| Document | Persian                          | English                    |
| -------- | -------------------------------- | -------------------------- |
| Privacy  | `https://hatef.ir/حریم-خصوصی`    | `https://hatef.ir/privacy` |
| Terms    | `https://hatef.ir/شرایط-استفاده` | `https://hatef.ir/terms`   |

The URL path alone selects the language. Query parameters and Accept-Language do
not override it. Each document links to its translation without a query string.
Each language has a self-referencing canonical, reciprocal `hreflang` links and
a Persian `x-default`. The legacy `/privacy.html` and `/terms.html` URLs redirect
permanently to the English documents. Persian routes accept raw UTF-8 and normal
upper/lower-case percent-encoded browser paths.

## Sources and wording decisions

The Persian and English documents cover matching sections and are based on:

- [Project overview](../../README.md): search, profiles, website submissions and
  financial support that does not buy organic ranking.
- [Privacy architecture](../privacy/PRIVACY_ARCHITECTURE.md): separate analytics
  and compliance data, designated encrypted fields and retention targets.
- [Privacy implementation notes](../privacy/IMPLEMENTATION_SUMMARY.md): intended
  controls and deployment dependencies.
- `src/controllers/ProfileController.cpp`: actual view logging, 365-day compliance
  expiry, cleanup endpoint, audit logging and the privacy dashboard's two-year
  analytics target.
- `src/storage/ComplianceStorage.cpp` and `src/storage/ProfileStorage.cpp`:
  cleanup exclusions for investigations and soft deletion of profiles.
- `src/controllers/SearchController.cpp` and `include/routing/RouteRegistry.h`:
  query and request logging. The policy must not promise that searches or full
  IP addresses are never stored.
- `public/assets/js/profile-editor.js` and the content/links editors: browser
  storage can retain access details and unsaved drafts.
- [Google API Services User Data Policy](https://developers.google.com/terms/api-services-user-data-policy).
- [Google Workspace user data and developer policy](https://developers.google.com/workspace/workspace-api-user-data-developer-policy).

The old placeholder policies in `pages/privacy.html` and `public/privacy.html`
were removed because they contained a fictional operator/address and claims
inconsistent with the repository. Their public URL has an explicit redirect.
No legal operator company, mailing address, jurisdiction, hosting location,
certification or production-only security control has been invented.

The policy distinguishes a documented retention target from verified deletion.
A cleanup endpoint is not evidence that a scheduled production cleanup job runs.
Soft deletion is not immediate permanent erasure. The documented legal vault is
not presented as an independently verified production installation.

## Google integrations

This change implements legal documents, not People API, Gmail sending, OAuth token
storage, background jobs or recipient opt-out infrastructure. Those disclosures
are expressly conditional on the optional feature becoming available. Google
permissions must not be requested for an unimplemented feature.

Before enabling an invitation feature:

- Confirm the actual operator identity and that the published support address is
  monitored.
- Limit contact access to names and email addresses through `contacts.readonly`.
- Request `gmail.send` only in context when sending is enabled.
- Review the permitted Gmail use case with Google. A consent screen and policy
  do not guarantee approval of an invitation or acquisition workflow.
- Show recipients and the message before explicit approval.
- Protect tokens at rest and in transit, define access/refresh-token lifetimes,
  remove or invalidate them on disconnect and stop pending sends.
- Publish the actual invitation/contact-cache retention and erasure periods once
  the implementation and infrastructure are known.
- Implement recipient opt-outs, abuse reports, rate limits and cancellation.
- Require explicit separate consent for scheduled/background sending.
- Enforce Google's Limited Use restrictions, including no advertising, data
  brokerage or generalized AI training on Google data.
- List infrastructure providers and any cross-border processing where required.
- Verify the domain and complete applicable OAuth scope verification.
- Obtain appropriate legal review for the jurisdictions actually served.

## Validation

Run the public route checks against a compiled, running C++ server:

```sh
BASE_URL=http://127.0.0.1:3000 bash tests/integration/test_legal_pages.sh
```

These verify all four routes, upper/lower-case percent encoding, language and
security headers, query-independent language selection, canonical URLs, legacy
redirects, the local stylesheet and the bundled Persian font. The core image
build workflow runs these against the compiled candidate image without login.

Run the layout and navigation checks:

```sh
NODE_PATH=/tmp/legal-ui-tests/node_modules node tests/legal/browser-check.cjs
```

The default fixture serves the exact committed HTML, CSS and fonts. It tests
both languages at 1440, 1024, 768, 390 and 320 pixels, no horizontal overflow,
translation/document navigation, anchor links, metadata and local font loading.
It saves desktop, mobile and full-page screenshots. Set `LEGAL_BASE_URL` to run
against a real backend instead. A dedicated GitHub Actions workflow runs it.

Before production release, verify all four deployed URLs, footer links, HTTPS
and the production retention/deletion jobs. Passing tests establishes code and
layout behavior, not legal or Google verification approval.
