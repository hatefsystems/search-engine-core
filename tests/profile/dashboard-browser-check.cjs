// Dashboard interactions against the real template/modules and an isolated fixture API.
const {chromium} = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const {start} = require('./workspace-fixture.cjs');
(async () => {
    const server = start(0);
    await new Promise(resolve => server.listening ? resolve() : server.once('listening', resolve));
    const browser = await chromium.launch({headless:true, executablePath:process.env.PROFILE_CHROMIUM_EXECUTABLE || undefined, args:['--no-sandbox','--disable-dev-shm-usage']});
    const context = await browser.newContext({viewport:{width:1440,height:1080}});
    const page = await context.newPage(), errors = [];
    page.on('pageerror', error => errors.push(error.message));
    const base = `http://127.0.0.1:${server.address().port}`;
    const saved = () => page.waitForFunction(() => document.getElementById('save-status').dataset.saved === 'true');
    const data = async () => (await (await context.request.get(base + '/api/profiles/fixture')).json()).data;
    const out = 'build/profile/workspace-screenshots'; fs.mkdirSync(out, {recursive:true});
    try {
        await page.goto(base);
        await page.locator('#editor-section-nav button').first().waitFor();
        await page.evaluate(() => document.fonts.ready);
        const ring = page.getByRole('progressbar');
        assert.equal(await ring.getAttribute('aria-valuenow'), '67');
        assert.equal(await page.locator('[data-crop=cover]').isDisabled(), true);
        await page.locator('#profile-tagline').fill('طراحی و توسعهٔ پلتفرم‌های پایدار');
        await page.locator('#save-profile').click(); await saved();
        assert.equal((await data()).tagline, 'طراحی و توسعهٔ پلتفرم‌های پایدار');
        assert.equal(await ring.getAttribute('aria-valuenow'), '78');
        // Both crop ratios are encoded into the uploaded bytes, not just CSS.
        for (const kind of ['cover', 'avatar']) {
            await page.locator(`#${kind}-file`).setInputFiles(path.join(__dirname, '../../public/coming-soon/assets/images/slide1.jpg'));
            await page.waitForFunction(() => !document.getElementById('profile-form').inert); await saved();
            const before = (await data())[kind === 'cover' ? 'coverImageUrl' : 'avatarUrl'];
            await page.locator(`[data-crop=${kind}]`).click();
            await page.locator('#image-crop-dialog[open]').waitFor();
            const original = await page.locator('#crop-canvas').evaluate(canvas => canvas.toDataURL());
            await page.locator('#crop-zoom').fill('1.8');
            await page.locator('#crop-x').fill('0.1');
            await page.locator('#crop-y').fill('0.8');
            assert.notEqual(await page.locator('#crop-canvas').evaluate(canvas => canvas.toDataURL()), original);
            const response = page.waitForResponse(r => r.url().endsWith('/' + kind) && r.request().method() === 'POST');
            await page.locator('#apply-crop').click(); assert.equal((await response).status(), 200);
            await page.waitForFunction(() => !document.getElementById('profile-form').inert); await saved();
            const after = (await data())[kind === 'cover' ? 'coverImageUrl' : 'avatarUrl']; assert.notEqual(after, before);
            const dimensions = await page.evaluate(async url => {
                const bitmap = await createImageBitmap(await (await fetch(url)).blob());
                const size = [bitmap.width, bitmap.height]; bitmap.close(); return size;
            }, after);
            assert.deepEqual(dimensions, kind === 'cover' ? [1600, 400] : [640, 640]);
            await page.locator(`[data-crop=${kind}]`).click();
            await page.locator('#image-crop-dialog[open]').waitFor();
            await page.keyboard.press('Escape');
            assert.equal((await data())[kind === 'cover' ? 'coverImageUrl' : 'avatarUrl'], after, 'cancel never replaces media');
        }
        // Escape while canvas encoding is pending cancels the replacement too.
        const beforeCancelledEncoding = (await data()).avatarUrl;
        await page.locator('[data-crop=avatar]').click();
        await page.locator('#image-crop-dialog[open]').waitFor();
        await page.evaluate(() => {
            const canvas = document.getElementById('crop-canvas'), encode = canvas.toBlob;
            canvas.toBlob = (callback, ...args) => { window.releaseCropEncoding = () => { canvas.toBlob = encode; encode.call(canvas, callback, ...args); }; };
        });
        await page.locator('#apply-crop').click(); await page.keyboard.press('Escape');
        await page.locator('#image-crop-dialog').waitFor({state:'hidden'});
        await page.evaluate(() => window.releaseCropEncoding());
        await page.waitForFunction(() => !document.getElementById('apply-crop').disabled);
        assert.equal((await data()).avatarUrl, beforeCancelledEncoding);
        assert.equal(await ring.getAttribute('aria-valuenow'), '100');
        await page.locator('#profile-bio').fill('معرفی کوتاه'); await saved();
        assert.match(await page.locator('#bio-count').textContent(), /۱۱ \/ ۵۰۰/);
        await page.locator('#profile-goal-select').selectOption('FIND_CLIENTS'); await saved();
        const privacy = page.locator('#profile-settings');
        await privacy.getByRole('checkbox', {name:'شهر', exact:true}).uncheck(); await saved();
        await privacy.getByRole('checkbox', {name:'ایمیل', exact:true}).uncheck(); await saved();
        assert.equal((await data()).contentLayout.privacy.showLocation, false);
        await page.reload(); await page.locator('#profile-goal-select').waitFor();
        assert.equal(await page.locator('#profile-goal-select').inputValue(), 'FIND_CLIENTS');
        assert.equal(await privacy.getByRole('checkbox', {name:'شهر', exact:true}).isChecked(), false);
        await page.locator('#open-dashboard-preview').click();
        await page.frameLocator('#public-preview').locator('#preview-name').waitFor();
        assert.equal(await page.frameLocator('#public-preview').locator('#preview-location').count(), 0);
        await page.locator('[data-preview-size=mobile]').click();
        assert.ok((await page.locator('#public-preview').boundingBox()).width <= 360);
        await page.keyboard.press('Escape');
        assert.equal(await page.locator('#open-dashboard-preview').evaluate(node => node === document.activeElement), true);
        for (const width of [1920,1440,1280,1024,768,390,320]) {
            await page.setViewportSize({width, height:1080});
            await page.evaluate(() => scrollTo(0, 0));
            assert.ok(await page.evaluate(() => document.documentElement.scrollWidth <= innerWidth), `overflow at ${width}`);
            const clipped = await page.locator('#profile-form input, #profile-form select, #profile-form textarea').evaluateAll(nodes => nodes.filter(n => n.getClientRects().length).filter(n => {const b = n.getBoundingClientRect(); return b.x < 0 || b.right > innerWidth + 1;}).map(n => n.id));
            assert.deepEqual(clipped, [], `clipped fields at ${width}`);
            await page.screenshot({path:`${out}/dashboard-${width}.png`, fullPage:true});
        }
        // Savebar must not cover the last privacy controls on a narrow screen.
        await page.locator('#profile-settings').scrollIntoViewIfNeeded();
        const overlap = await page.evaluate(() => document.querySelector('#profile-settings').getBoundingClientRect().bottom > document.querySelector('.editor-savebar').getBoundingClientRect().top);
        assert.equal(overlap, false);
        assert.deepEqual(errors, []);
        console.log('PASS dashboard: completion, manual save, crop bytes/ratios/cancel, privacy and goal persistence, preview focus/devices, 7 responsive widths. Fixture API only.');
    } finally {
        await browser.close(); await new Promise(resolve => server.close(resolve));
    }
})().catch(error => { console.error(error); process.exitCode = 1; });
