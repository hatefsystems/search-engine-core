# Profile workspace validation

The workspace implements the RTL section navigation, master/detail forms, media
controls, purple styling, and live preview from Figma file
`1I1jlnkZIrSdCj9mPdqVi5` and the supplied reference screens. Avatar and cover
images come from the user's profile. Sample portraits, logos, and statistics in
the references are not hardcoded into real profiles.

## Autosave regression tests

```sh
node --test tests/profile/test_autosave.mjs tests/profile/test_content_autosave.mjs
```

## Browser and responsive checks

Install Playwright in an isolated directory, then run from the repository root:

```sh
npm install --prefix /tmp/profile-ui-tests --ignore-scripts playwright@1.58.2
/tmp/profile-ui-tests/node_modules/.bin/playwright install --with-deps chromium
NODE_PATH=/tmp/profile-ui-tests/node_modules node tests/profile/workspace-browser-check.cjs
```

Set `PROFILE_CHROMIUM_EXECUTABLE` to use an existing Chromium binary. The test
starts and stops its own local fixture server. Screenshots are written to
`build/profile/workspace-screenshots/`.

The fixture serves the real Inja template, JavaScript, CSS, and fonts. It
derives the 14 section schemas from the C++ source and supplies an isolated,
in-memory, versioned API. The test covers every schema field, RTL column order,
item creation, copying, ordering and deletion, search, privacy, ongoing dates,
offline recovery, shared header/content saves, two-tab conflict reconciliation,
media upload/clear, links, publishing, logout, and JavaScript errors. Layout
checks run at 1920, 1440, 1280, 1024, 768, 390, and 320 pixels, including mobile
preview tabs.

This suite verifies browser behavior, not C++ endpoint validation,
authentication, MongoDB persistence, or real image processing. Use the existing
`editor-browser-check.cjs` and `advanced-browser-check.cjs` suites against a
running backend for integration coverage. Screenshots support visual review;
they are not a pixel-diff assertion against Figma.

The `Profile workspace UI` GitHub Actions workflow runs both local suites and
uploads the screenshots when the branch is pushed or a matching PR is updated.

## Typography and text enlargement

```sh
NODE_PATH=/tmp/profile-ui-tests/node_modules node tests/profile/typography-browser-check.cjs
```

This checks computed form values (at least 16px), functional
labels/help/statuses (at least 14px), selected status color contrast, keyboard
privacy controls, unclipped text, navigation overflow and footer clearance at
seven widths from 320 to 1920 CSS pixels. Each width is also checked with a 200%
root text size. The 320px viewport additionally exercises the reflow width of a
1280px desktop at 400% zoom. Text resizing and viewport reflow are separate
checks; neither is a complete WCAG audit or a real-device Safari test.

Screenshots use isolated sample data and the production template/styles/modules,
with the fixture API, and are saved to `build/profile/typography-screenshots/`.
The mobile save bar stays in document flow so enlarged controls do not cover the
form. The desktop save bar remains fixed with additional scroll clearance.
