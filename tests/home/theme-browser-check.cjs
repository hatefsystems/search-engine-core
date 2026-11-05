// Real homepage assets with a static fixture, without the C++ backend.
const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');
const root = path.resolve(__dirname, '../..');

function homepage(lang) {
  if (lang === 'static') return fs.readFileSync(path.join(root, 'public/home.html'), 'utf8');
  const t = JSON.parse(fs.readFileSync(path.join(root, `locales/${lang}/home.json`), 'utf8'));
  t.language = { code: lang, direction: lang === 'fa' ? 'rtl' : 'ltr' };
  return fs.readFileSync(path.join(root, 'templates/home.inja'), 'utf8')
    .replace(/\{#[\s\S]*?#\}/g, '')
    .replace(/\{% if t.language.code == "fa" %\}([\s\S]*?)\{% else %\}([\s\S]*?)\{% endif %\}/g,
      (_, fa, en) => lang === 'fa' ? fa : en)
    .replace(/\{\{ base_url \}\}/g, '')
    .replace(/\{\{ t\.([\w.]+) \}\}/g, (_, key) => {
      const value = key.split('.').reduce((value, part) => value?.[part], t);
      assert.notEqual(value, undefined, `Missing locale key: ${lang}/${key}`);
      return String(value).replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/</g, '&lt;');
    });
}

const server = http.createServer((req, res) => {
  const url = new URL(req.url, 'http://localhost');
  if (url.pathname === '/') {
    res.setHeader('Content-Type', 'text/html; charset=utf-8');
    return res.end(homepage(url.searchParams.get('lang') || 'en'));
  }
  const file = { '/styles.css': 'styles.css', '/script.js': 'script.js',
    '/assets/styles.css': 'styles.css', '/assets/script.js': 'script.js' }[url.pathname];
  if (!file) return res.writeHead(404).end();
  res.setHeader('Content-Type', file.endsWith('.css') ? 'text/css' : 'application/javascript');
  res.end(fs.readFileSync(path.join(root, 'public', file)));
});

async function checkTheme(page, theme) {
  const isLight = theme === 'light';
  const expected = {
    background: isLight ? 'rgb(248, 250, 252)' : 'rgb(11, 15, 20)',
    text: isLight ? 'rgb(15, 23, 42)' : 'rgb(229, 231, 235)',
    surface: isLight ? '#ffffff' : '#111827',
    scheme: theme,
    meta: isLight ? '#f8fafc' : '#0b0f14',
    light: isLight, dark: !isLight,
    pressed: String(isLight), icon: isLight ? '☀️' : '🌙',
  };
  await page.waitForFunction(theme => document.documentElement.classList.contains(theme), theme);
  assert.deepEqual(await page.evaluate(() => {
    const html = document.documentElement;
    const style = getComputedStyle(html);
    const button = document.getElementById('theme-toggle');
    return {
      background: getComputedStyle(document.body).backgroundColor,
      text: getComputedStyle(document.body).color,
      surface: style.getPropertyValue('--surface').trim(), scheme: style.colorScheme,
      meta: document.getElementById('theme-color').content,
      light: html.classList.contains('light'), dark: html.classList.contains('dark'),
      pressed: button.getAttribute('aria-pressed'), icon: button.textContent,
    };
  }), expected);
}

(async () => {
  let browser;
  let cases = 0;
  try {
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    const base = `http://127.0.0.1:${server.address().port}`;
    browser = await chromium.launch({ headless: true, args: ['--no-sandbox'],
      ...(process.env.THEME_CHROMIUM_EXECUTABLE ? { executablePath: process.env.THEME_CHROMIUM_EXECUTABLE } : {}) });
    for (const lang of ['en', 'fa', 'static']) {
      for (const colorScheme of ['light', 'dark', 'no-preference']) {
        for (const saved of [null, 'light', 'dark', 'invalid']) {
          const context = await browser.newContext({ colorScheme });
          const page = await context.newPage();
          const errors = [];
          page.on('pageerror', error => errors.push(error.message));
          await page.goto(`${base}/?lang=${lang}`);
          await page.evaluate(saved => {
            if (saved === null) localStorage.removeItem('theme');
            else localStorage.setItem('theme', saved);
          }, saved);
          await page.reload();
          const initial = ['light', 'dark'].includes(saved) ? saved : (colorScheme === 'dark' ? 'dark' : 'light');
          const opposite = initial === 'light' ? 'dark' : 'light';
          await checkTheme(page, initial);
          await page.locator('#theme-toggle').click();
          await checkTheme(page, opposite);
          assert.equal(await page.evaluate(() => localStorage.getItem('theme')), opposite);
          await page.reload();
          await checkTheme(page, opposite);
          await page.locator('#theme-toggle').focus();
          await page.keyboard.press('Space');
          await checkTheme(page, initial);
          assert.deepEqual(errors, []);
          await context.close();
          cases++;
        }
      }
    }
    // A system change is followed until the user explicitly chooses a theme.
    const context = await browser.newContext({ colorScheme: 'light' });
    const page = await context.newPage();
    await page.goto(base);
    await checkTheme(page, 'light');
    await page.emulateMedia({ colorScheme: 'dark' });
    await checkTheme(page, 'dark');
    await page.locator('#theme-toggle').click();
    await page.emulateMedia({ colorScheme: 'light' });
    await page.emulateMedia({ colorScheme: 'dark' });
    await checkTheme(page, 'light');
    await context.close();
    // Older browsers and blocked storage must not disable the manual toggle.
    for (const unavailable of ['matchMedia', 'storage', 'both']) {
      const context = await browser.newContext({ colorScheme: 'light' });
      await context.addInitScript(unavailable => {
        if (unavailable !== 'storage') Object.defineProperty(window, 'matchMedia', { value: undefined });
        if (unavailable !== 'matchMedia') Object.defineProperty(window, 'localStorage', {
          get() { throw new DOMException('Storage blocked', 'SecurityError'); }
        });
      }, unavailable);
      const page = await context.newPage();
      const errors = [];
      page.on('pageerror', error => errors.push(error.message));
      await page.goto(base);
      await checkTheme(page, 'light');
      await page.locator('#theme-toggle').click();
      await checkTheme(page, 'dark');
      await page.locator('#theme-toggle').click();
      await checkTheme(page, 'light');
      assert.deepEqual(errors, []);
      await context.close();
    }
    console.log(`PASS: ${cases} locale/system/saved-theme combinations, system changes, keyboard, unavailable APIs`);
  } finally {
    await browser?.close();
    await new Promise(resolve => server.close(resolve));
  }
})().catch(error => { console.error(error); process.exitCode = 1; });
