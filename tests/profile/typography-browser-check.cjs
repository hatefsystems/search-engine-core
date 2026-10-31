// Computed typography, contrast and reflow against the production editor assets.
// This fixture does not exercise the C++ endpoints or MongoDB persistence.
const {chromium} = require('playwright');
const assert = require('node:assert/strict');
const {start} = require('./workspace-fixture.cjs');
const fs = require('node:fs');
const out = 'build/profile/typography-screenshots';
(async () => {
  const server = start(0, {advancedSections:false});
  await new Promise(r => server.listening ? r() : server.once('listening', r));
  const browser = await chromium.launch({headless:true, executablePath:process.env.PROFILE_CHROMIUM_EXECUTABLE || undefined, args:['--no-sandbox','--disable-dev-shm-usage']});
  const page = await browser.newPage(), errors = [];
  page.on('pageerror', e => errors.push(e.message));
  page.on('response', r => { if (r.status() >= 400) errors.push(`${r.status()} ${r.url()}`); });
  fs.mkdirSync(out, {recursive:true});
  try {
    await page.goto(`http://127.0.0.1:${server.address().port}`);
    await page.locator('#profile-name').waitFor();
    await page.waitForFunction(() => !!document.querySelector('#profile-name').value);
    await page.evaluate(() => document.fonts.ready);
    assert.ok(await page.evaluate(() => document.fonts.check('16px "Vazirmatn FD"')));
    for (const width of [1920,1440,1280,1024,768,390,320]) {
      await page.setViewportSize({width,height:1080});
      for (const scale of [1,2]) {
        // Resize text independently of viewport and pixel dimensions. This catches
        // fixed-height clipping that a screenshot/deviceScaleFactor cannot detect.
        await page.evaluate(scale => document.documentElement.style.fontSize = `${100 * scale}%`, scale);
        const sizes = await page.evaluate(() => {
          const size = s => [...document.querySelectorAll(s)].filter(n => n.getClientRects().length).map(n => ({selector:s, id:n.id, size:parseFloat(getComputedStyle(n).fontSize)}));
          return {
            values:size('.identity-panel input, .identity-panel select, .identity-panel textarea'),
            functional:size('.identity-panel label, .hint, .privacy-name, .privacy-state, #save-status, .editing-badge, .cover-controls .button, .avatar-controls .button, .editor-savebar .button, .section-nav .nav-label'),
          };
        });
        for (const value of sizes.values) assert.ok(value.size >= 16 * scale, `${width}/${scale} input ${JSON.stringify(value)}`);
        for (const value of sizes.functional) assert.ok(value.size >= 14 * scale, `${width}/${scale} text ${JSON.stringify(value)}`);
        const layout = await page.evaluate(() => ({page:document.documentElement.scrollWidth, viewport:innerWidth, nav:document.querySelector('.section-nav').scrollWidth, navWidth:document.querySelector('.section-nav').clientWidth}));
        assert.ok(layout.page <= layout.viewport + 1, `${width}/${scale} horizontal overflow: ${JSON.stringify(layout)}`);
        assert.ok(layout.nav <= layout.navWidth + 1, `${width}/${scale} navigation overflow`);
        const clipped = await page.locator('.identity-panel label, .privacy-name, .privacy-state, .hint, .section-nav .nav-label, .editor-savebar .button').evaluateAll(nodes => nodes.filter(n => n.getClientRects().length).filter(n => n.scrollWidth > n.clientWidth + 1 || n.scrollHeight > n.clientHeight + 1).map(n => ({text:n.textContent.trim(),width:n.clientWidth,content:n.scrollWidth,height:n.clientHeight,contentHeight:n.scrollHeight})));
        assert.deepEqual(clipped, [], `${width}/${scale} clipped text`);
        await page.locator('.privacy-choice').last().evaluate(n => n.scrollIntoView({block:'center'}));
        assert.ok(await page.evaluate(() => [...document.querySelectorAll('.privacy-choice')].at(-1).getBoundingClientRect().bottom <= document.querySelector('.editor-savebar').getBoundingClientRect().top), `${width}/${scale} savebar overlaps final privacy control`);
      }
    }
    await page.evaluate(() => document.documentElement.style.removeProperty('font-size'));
    await page.setViewportSize({width:1440,height:1080});
    // Contrast from the computed colors and real opaque ancestor backgrounds.
    const ratios = await page.evaluate(() => {
      const rgb = s => s.match(/[\d.]+/g).map(Number);
      const lum = c => c.slice(0,3).map(x => {x/=255;return x<=.04045 ? x/12.92 : ((x+.055)/1.055)**2.4;}).reduce((s,x,i) => s+x*[.2126,.7152,.0722][i],0);
      const contrast = (a,b) => (Math.max(lum(a),lum(b))+.05)/(Math.min(lum(a),lum(b))+.05);
      return ['.basic-heading .hint','.editing-badge','.privacy-state','.cover-editor > .hint','#save-status','.sidebar-launch-card #release-notification-hint'].flatMap(selector => [...document.querySelectorAll(selector)].filter(n => n.getClientRects().length).map(n => {
        let bg=[255,255,255];
        for (let p=n;p;p=p.parentElement) {const c=rgb(getComputedStyle(p).backgroundColor);if (c.length===3 || c[3]===1) {bg=c;break;}}
        return {selector,text:n.textContent.trim(),ratio:contrast(rgb(getComputedStyle(n).color),bg)};
      }));
    });
    for (const item of ratios) assert.ok(item.ratio >= 4.5, `contrast ${JSON.stringify(item)}`);
    // Exercise both explicit privacy states and keyboard use after resizing.
    const email = page.getByRole('checkbox', {name:'ایمیل',exact:true});
    await email.focus();const before=await email.isChecked();await page.keyboard.press('Space');
    assert.equal(await email.isChecked(), !before);
    await page.waitForFunction(() => document.getElementById('save-status').dataset.saved === 'true');
    // Isolated demo data, no account session or private contact details.
    await page.locator('#profile-name').fill('کاربر نمونه');
    await page.locator('#profile-title').fill('مهندس نرم‌افزار و پلتفرم');
    await page.locator('#profile-tagline').fill('ساخت محصولاتی ساده، قابل اعتماد و کاربردی');
    await page.locator('#profile-bio').fill('به طراحی و توسعهٔ نرم‌افزار علاقه‌مندم. در این صفحه می‌توانید با تخصص و زمینه‌های همکاری من آشنا شوید.');
    await page.getByRole('checkbox', {name:'ایمیل',exact:true}).uncheck();
    await page.getByRole('checkbox', {name:'تلفن',exact:true}).uncheck();
    await page.waitForFunction(() => document.getElementById('save-status').dataset.saved === 'true');
    await page.evaluate(() => document.activeElement.blur());
    for (const width of [1440,390]) {
      await page.setViewportSize({width,height:width===1440?1600:1080});
      await page.evaluate(() => scrollTo(0,0));
      await page.screenshot({path:`${out}/profile-editor-${width}.png`,fullPage:width!==1440});
    }
    assert.deepEqual(errors, []);
    console.log('PASS typography: computed 16px inputs/14px functional text, 7 widths at 100% and 200% text size, reflow, no clipped labels, footer clearance, status contrast, keyboard privacy. Fixture API only.');
  } finally {await browser.close();await new Promise(r => server.close(r));}
})().catch(error => {console.error(error);process.exitCode=1;});
