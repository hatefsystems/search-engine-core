# Agent instructions

## Frontend typography and readability

Apply these rules to new and modified UI in `templates/` and `public/`. Preserve
existing behavior and localization. Use the shared tokens in
`public/assets/css/typography.css`; do not create independent compact text
scales. The pixel equivalents below assume the browser's default 16px root, not
a fixed root size.

| Role                                         | Size                               | Line height |
| -------------------------------------------- | ---------------------------------- | ----------- |
| Page title                                   | 1.5–1.75rem (24–28px)              | 1.5–1.6     |
| Section title                                | 1.125–1.25rem (18–20px)            | 1.6         |
| Main editor heading                          | 1.375rem (22px), 1.25rem on mobile | 1.6         |
| Body copy and form values                    | 1rem (16px)                        | 1.75–1.9    |
| Labels, buttons and navigation               | .875–1rem (14–16px)                | 1.6–1.7     |
| Help, errors, privacy states and save status | .875rem (14px)                     | 1.8         |
| Nonessential metadata and character counters | .75rem (12px)                      | 1.6         |

- Keep the browser root size unchanged. Use `rem` and shared role tokens for
  text. Never shrink functional text to 8–11px to make a mobile layout fit.
- Keep form values at 1rem on desktop and mobile. Check computed styles after
  all stylesheets and media queries load, including the identity panel
  overrides.
- Use at least 4.5:1 contrast for normal text, including help, placeholders and
  status text, against its actual background. Size increases alone do not fix
  contrast. This product scale is not a WCAG minimum-font-size requirement.
- Use single columns, wrapping, flexible heights and spacing when space is
  tight. Do not hide essential labels or states. Reserve 12px for genuinely
  secondary metadata; all instructions and privacy states remain at least 14px.
- Display explicit public/private text alongside privacy controls. Preserve
  their accessible name, keyboard operation and visible focus. Email and phone
  require explicit user consent before public display.
- Keep the sidebar rollout intact: one active basic section, six locked items,
  no inner menu scrolling, completion only for currently editable fields, and
  the upcoming-sections card. Preserve the two media editing rows.
- Keep save/draft/publish wording consistent with actual persistence behavior.
  Do not imply a draft or publication boundary that the API does not implement.
- Fixed save bars must leave the final controls visible and focusable. Verify at
  320px and 390px, desktop, and 200% text enlargement. Check for clipped labels,
  horizontal page scrolling and footer overlap. Also check 400% desktop zoom
  reflow using a 320 CSS-pixel viewport.
- Load and inspect the real templates, CSS, JavaScript and local Persian font.
  Label fixture API screenshots accurately; they do not prove backend
  integration.
- Run the affected browser suites and
  `node tests/profile/typography-browser-check.cjs` for typography changes.
  Refresh stylesheet query versions when changing long-cached production assets.

See `.cursor/rules/frontend.mdc` and
`.github/instructions/frontend.instructions.md` for existing frontend
conventions.
