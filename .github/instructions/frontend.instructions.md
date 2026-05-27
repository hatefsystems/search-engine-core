---
description: Templates, locales, CSS, JS, and base_url for frontend
applyTo: 'templates/**/*,public/**/*,locales/**/*'
---

# Frontend Integration

## Paths

- Static: `/public/` (StaticFileController, 1-year cache for JS/CSS)
- Templates: `/templates/` (Inja)
- Locales: `/locales/` — use language subfolders (`en/`, `fa/`), not files in root. Keep `languages.json` in root.

## Localization Loading

```cpp
// Load from language-specific folder
std::string localesPath = "locales/" + lang + "/crawling-notification.json";
std::string localeContent = loadFile(localesPath);
if (localeContent.empty()) {
    localesPath = "locales/en/crawling-notification.json";
    localeContent = loadFile(localesPath);
}
```

Locales checklist: `locales/en/`, `locales/fa/`; descriptive filenames (common.json, sponsor.json); fallback to English.

## Links

Use `{{ base_url }}` for all internal links. Never hardcode localhost or port.

```html
<a href="{{ base_url }}/crawl-request">Crawl Request</a>
```

## CSS

Reuse existing classes; define shared values in `:root`; use BEM and utility classes.

## JavaScript (CSP)

No inline handlers (`onclick`, etc.). Use `data-*` attributes and `addEventListener()`.

```html
<button data-copy-text="text" class="copy-btn">Copy</button>
```

Use `addEventListener()` and event delegation for dynamic elements. No `onclick`, `onload`, `onsubmit`, etc. in HTML.
