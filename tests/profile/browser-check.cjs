// Run after exporting HTML fixtures from test_public_profile; see README.md.
const { chromium } = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const http = require('node:http');

const root = path.resolve(__dirname, '../..');
const previews = process.env.PROFILE_PREVIEW_DIR;
assert(previews, 'Set PROFILE_PREVIEW_DIR to the exported C++ test fixtures.');
const screenshots = path.join(root, 'build/profile/screenshots');
fs.mkdirSync(screenshots, { recursive: true });

const server = http.createServer((req, res) => {
    const pathname = new URL(req.url, 'http://localhost').pathname;
    if (pathname === '/fixtures/avatar.svg' || pathname === '/fixtures/cover.svg') {
        res.setHeader('Content-Type', 'image/svg+xml');
        res.end(pathname.includes('avatar')
            ? '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100"><rect width="100" height="100" fill="#ebe4f7"/><circle cx="50" cy="35" r="18" fill="#7b689f"/><path d="M12 100Q12 58 50 58Q88 58 88 100" fill="#7b689f"/></svg>'
            : '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 800 220"><rect width="800" height="220" fill="#7563c7"/><circle cx="160" cy="-40" r="220" fill="#9d88d6"/><circle cx="730" cy="200" r="160" fill="#59418f"/></svg>');
        return;
    }
    let file;
    if (pathname.startsWith('/assets/')) {
        // Match StaticFileController's legacy-first mapping and bundled fallback.
        file = path.join(root, 'public', pathname.slice(8));
        if (!fs.existsSync(file)) file = path.join(root, 'public/assets', pathname.slice(8));
    } else {
        file = path.join(previews, path.basename(pathname));
    }
    if (!fs.existsSync(file) || !fs.statSync(file).isFile()) {
        res.writeHead(404).end();
        return;
    }
    res.setHeader('Content-Type', {
        '.html': 'text/html; charset=utf-8', '.css': 'text/css',
        '.js': 'text/javascript', '.woff2': 'font/woff2',
    }[path.extname(file)] || 'application/octet-stream');
    res.end(fs.readFileSync(file));
});

(async () => {
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    let browser;
    try {
        browser = await chromium.launch({ headless: true, args: ['--no-sandbox'] });
        const page = await browser.newPage();
        const errors = [];
        page.on('pageerror', error => errors.push(error.message));
        for (const width of [360, 768, 1280]) {
            await page.setViewportSize({ width, height: 1000 });
            for (const fixture of ['full', 'minimal', 'broken', 'long', 'escaped']) {
                await page.goto(`http://127.0.0.1:${server.address().port}/${fixture}.html`);
                await page.evaluate(() => document.fonts.ready);
                const geometry = await page.evaluate(() => ({
                    viewport: document.documentElement.clientWidth,
                    content: document.documentElement.scrollWidth,
                    card: document.querySelector('.profile-page').getBoundingClientRect().width,
                    direction: getComputedStyle(document.documentElement).direction,
                    font: document.fonts.check('16px "Vazirmatn FD"'),
                }));
                assert(geometry.content <= geometry.viewport, `${fixture} overflows at ${width}px`);
                assert(geometry.card <= 800, 'Card exceeds 800px');
                assert.equal(geometry.direction, 'rtl');
                assert(geometry.font, 'Persian font did not load');
                if (fixture === 'broken') {
                    assert(await page.locator('[data-profile-image]').evaluateAll(images => images.every(image => image.hidden)));
                }
                if (fixture === 'full') {
                    assert(await page.locator('[data-profile-image]').evaluateAll(images => images.every(image => image.naturalWidth > 0)));
                    await page.keyboard.press('Tab');
                    assert.ok(await page.evaluate(() => ['A','BUTTON'].includes(document.activeElement.tagName) && document.activeElement.matches(':focus-visible')));
                    const link = page.locator('a[href]').first();
                    await link.focus();
                    assert.ok(await link.evaluate(el => el.matches(':focus-visible')));
                }
                await page.screenshot({ path: path.join(screenshots, `${fixture}-${width}.png`), fullPage: true });
            }
        }
        assert.deepEqual(errors, []);
        console.log('Passed 15 browser scenarios: 5 fixtures at 360px, 768px, and 1280px.');
        console.log(`Screenshots: ${screenshots}`);
    } finally {
        if (browser) await browser.close();
        await new Promise(resolve => server.close(resolve));
    }
})().catch(error => { console.error(error); process.exitCode = 1; });
