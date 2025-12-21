---
name: Fix Dark Mode System Preference Detection
overview: ""
todos: []
---

# Fix Dark Mode System Preference Detection

## Problem

The homepage dark mode doesn't work when Windows is set to light mode because `public/script.js` doesn't detect system preference (`prefers-color-scheme`) during initialization.

## Solution

Update the theme initialization in `public/script.js` to match the working implementation in `public/sponsor.js`:

### Changes to `public/script.js` (lines 34-53)

**Current broken code:**

```javascript
const THEME_KEY = 'theme';
const savedTheme = localStorage.getItem(THEME_KEY);
if (savedTheme === 'light') docEl.classList.add('light');
if (savedTheme === 'dark') docEl.classList.remove('light');
setThemeMetaColor();
```

**Fixed code:**

```javascript
const THEME_KEY = 'theme';

// Add system preference detection
function getSystemTheme() {
  return window.matchMedia && window.matchMedia('(prefers-color-scheme: light)').matches ? 'light' : 'dark';
}

// Initialize theme from saved preference OR system preference
const savedTheme = localStorage.getItem(THEME_KEY);
const initialTheme = savedTheme || getSystemTheme();

// Apply initial theme
if (initialTheme === 'light') {
  docEl.classList.add('light');
} else {
  docEl.classList.remove('light');
}
setThemeMetaColor();
```

### Additional Enhancement

Add system theme change listener (lines after toggle button initialization):

```javascript
// Listen for system theme changes when no manual preference is set
const mq = window.matchMedia('(prefers-color-scheme: light)');
mq.addEventListener?.('change', (e) => {
  const manual = localStorage.getItem(THEME_KEY);
  if (!manual) {
    const systemTheme = e.matches ? 'light' : 'dark';
    if (systemTheme === 'light') {
      docEl.classList.add('light');
    } else {
      docEl.classList.remove('light');
    }
    setThemeMetaColor();
    if (themeToggle) {
      themeToggle.textContent = systemTheme === 'light' ? '☀️' : '🌙';
      themeToggle.setAttribute('aria-pressed', String(systemTheme === 'light'));
    }
  }
});
```

## Testing

After the fix:

1. Clear browser localStorage (or test in incognito)
2. Set Windows to light mode → homepage should be light theme by default
3. Set Windows to dark mode → homepage should be dark theme by default
4. Toggle button should work correctly in both cases
5. Manual selection should persist in localStorage and override system preference