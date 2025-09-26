// Hatef homepage interactions
// - Theme toggle with localStorage
// - Search suggestions with debounce and keyboard nav
// - Recent searches chips (last 5)
// - Keyboard shortcuts: / focus, Enter submit, Esc clear suggestions

(function () {
  'use strict';

  const docEl = document.documentElement;
  const themeToggle = document.getElementById('theme-toggle');
  const form = document.getElementById('search-form') || document.querySelector('.header-search-form');
  const input = document.getElementById('q') || document.querySelector('.header-search-input');
  const list = document.getElementById('suggestions');
  const recentWrap = document.getElementById('recent');
  const yearEl = document.getElementById('year');
  const themeColorMeta = document.getElementById('theme-color');

  // ---------- Utilities ----------
  const clamp = (n, min, max) => Math.max(min, Math.min(max, n));
  const debounce = (fn, wait = 200) => {
    let t = null;
    return (...args) => {
      clearTimeout(t);
      t = setTimeout(() => fn.apply(null, args), wait);
    };
  };

  const setThemeMetaColor = () => {
    const isLight = docEl.classList.contains('light');
    themeColorMeta?.setAttribute('content', isLight ? '#f8fafc' : '#0b0f14');
  };

  // ---------- Theme ----------
  const THEME_KEY = 'theme'; // 'light' | 'dark'
  const savedTheme = localStorage.getItem(THEME_KEY);
  if (savedTheme === 'light') docEl.classList.add('light');
  if (savedTheme === 'dark') docEl.classList.remove('light');
  setThemeMetaColor();

  themeToggle?.addEventListener('click', () => {
    const isLight = docEl.classList.toggle('light');
    localStorage.setItem(THEME_KEY, isLight ? 'light' : 'dark');
    themeToggle.setAttribute('aria-pressed', String(isLight));
    themeToggle.textContent = isLight ? '☀️' : '🌙';
    setThemeMetaColor();
  });
  // Initialize toggle state icon
  if (themeToggle) {
    const isLight = docEl.classList.contains('light');
    themeToggle.setAttribute('aria-pressed', String(isLight));
    themeToggle.textContent = isLight ? '☀️' : '🌙';
  }

  // ---------- Copyright year ----------
  if (yearEl) yearEl.textContent = String(new Date().getFullYear());

  // ---------- Search functionality (works on both home and search pages) ----------
  if (form && input) {
    // ---------- Suggestions ----------
    const SUGGESTIONS = [
      'latest tech news',
      'open source search engine',
      'privacy friendly browsers',
      'web performance tips',
      'css grid examples',
      'javascript debounce function',
      'learn rust language',
      'linux command cheat sheet',
      'best static site generators',
      'http caching explained',
      'docker compose basics',
      'keyboard shortcuts list'
    ];

    let activeIndex = -1;

    function renderSuggestions(items) {
      if (!list) return; // Skip if suggestions list doesn't exist (e.g., on search page)
      list.innerHTML = '';
      if (!items.length) {
        hideSuggestions();
        return;
      }
      const frag = document.createDocumentFragment();
      items.forEach((text, i) => {
        const li = document.createElement('li');
        li.id = `sugg-${i}`;
        li.role = 'option';
        li.textContent = text;
        li.tabIndex = -1;
        li.addEventListener('mousedown', (e) => {
          // mousedown fires before blur; prevent blur losing active list
          e.preventDefault();
          selectSuggestion(text);
        });
        frag.appendChild(li);
      });
      list.appendChild(frag);
      list.hidden = false;
      input.setAttribute('aria-expanded', 'true');
    }

    function hideSuggestions() {
      if (!list) return; // Skip if suggestions list doesn't exist (e.g., on search page)
      list.hidden = true;
      input.setAttribute('aria-expanded', 'false');
      input.setAttribute('aria-activedescendant', '');
      activeIndex = -1;
    }

    const filterSuggestions = debounce((q) => {
      const query = q.trim().toLowerCase();
      if (!query) {
        hideSuggestions();
        return;
      }
      const filtered = SUGGESTIONS.filter((s) => s.includes(query)).slice(0, 8);
      renderSuggestions(filtered);
    }, 200);

    function selectSuggestion(text) {
      input.value = text;
      hideSuggestions();
      input.focus();
    }

    function moveActive(delta) {
      if (!list) return; // Skip if suggestions list doesn't exist (e.g., on search page)
      const items = Array.from(list.children);
      if (!items.length) return;
      activeIndex = clamp(activeIndex + delta, 0, items.length - 1);
      items.forEach((el, i) => el.setAttribute('aria-selected', String(i === activeIndex)));
      const active = items[activeIndex];
      input.setAttribute('aria-activedescendant', active.id);
    }

    if (input) {
      input.addEventListener('input', (e) => filterSuggestions(e.target.value));
      input.addEventListener('blur', () => setTimeout(hideSuggestions, 120));

      input.addEventListener('keydown', (e) => {
        if (e.key === 'ArrowDown') { e.preventDefault(); moveActive(1); }
        else if (e.key === 'ArrowUp') { e.preventDefault(); moveActive(-1); }
        else if (e.key === 'Enter') {
          if (list) {
            const items = Array.from(list.children);
            if (activeIndex >= 0 && items[activeIndex]) {
              e.preventDefault();
              selectSuggestion(items[activeIndex].textContent || '');
            }
          }
        } else if (e.key === 'Escape') {
          hideSuggestions();
          input.select();
        }
      });
    }

    // ---------- Recent searches ----------
    const RECENT_KEY = 'recent-searches';
    function getRecent() {
      try {
        const raw = localStorage.getItem(RECENT_KEY);
        const arr = raw ? JSON.parse(raw) : [];
        return Array.isArray(arr) ? arr : [];
      } catch { return []; }
    }
    function setRecent(arr) { localStorage.setItem(RECENT_KEY, JSON.stringify(arr.slice(0, 5))); }
    function addRecent(q) {
      if (!q) return;
      const list = getRecent();
      const without = list.filter((x) => x.toLowerCase() !== q.toLowerCase());
      without.unshift(q);
      setRecent(without);
      renderRecent();
    }
    function renderRecent() {
      if (!recentWrap) return;
      const recent = getRecent();
      recentWrap.innerHTML = '';
      recent.forEach((q) => {
        const b = document.createElement('button');
        b.type = 'button';
        b.className = 'chip';
        b.textContent = q;
        b.setAttribute('aria-label', `Use recent search ${q}`);
        b.addEventListener('click', () => { input.value = q; input.focus(); filterSuggestions(q); });
        recentWrap.appendChild(b);
      });
    }
    renderRecent();

    // ---------- Form submit ----------
    if (form) {
      form.addEventListener('submit', (e) => {
        const q = (input.value || '').trim();
        if (!q) return; // allow empty to submit to server if desired
        
        // Only add to recent searches if we have recentWrap (home page)
        if (recentWrap) {
          addRecent(q);
        }
        
        // Only prevent default and redirect if we're on home page (has suggestions)
        if (list) {
          const url = `/search?q=${encodeURIComponent(q)}`;
          window.location.href = url;
          e.preventDefault();
        }
        // If no list (search page), let the form submit normally
      });
    }
  }

  // ---------- Shortcuts ----------
  window.addEventListener('keydown', (e) => {
    const tag = (e.target && e.target.tagName) || '';
    const typingInInput = tag === 'INPUT' || tag === 'TEXTAREA';
    if (e.key === '/' && !typingInInput && input) {
      e.preventDefault();
      input.focus();
    }
  });
})();


