// Explicit test server only. Creates one uniquely named temporary profile, then deletes it.
const {chromium} = require('playwright');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const base = process.env.PROFILE_TEST_BASE_URL;
assert(base,'Set PROFILE_TEST_BASE_URL to an isolated test server.');
const screenshots = path.resolve(__dirname,'../../build/profile/editor-screenshots');
fs.mkdirSync(screenshots,{recursive:true});
(async()=>{
    const browser = await chromium.launch({headless:true,args:['--no-sandbox','--no-proxy-server']});
    const context = await browser.newContext();
    const page = await context.newPage();
    const errors=[]; page.on('pageerror',error=>errors.push(error.message));
    const slug='آزمون.مرورگر.'+Date.now();
    const url=`${base}/${encodeURIComponent(slug)}`;
    let id,key;
    const saved=async(p=page)=>p.waitForFunction(()=>document.getElementById('save-status').textContent==='ذخیره شد');
    try {
        for(const width of [360,768,1280]){
            await page.setViewportSize({width,height:1000});
            const response=await page.goto(url);assert.equal(response.status(),404);
            assert.equal(await page.locator('html').getAttribute('dir'),'rtl');
            assert.ok(await page.locator('text=ساخت این صفحه').isVisible());
            assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
            await page.screenshot({path:path.join(screenshots,`invitation-${width}.png`),fullPage:true});
        }
        await page.getByRole('link',{name:'ساخت این صفحه'}).click();
        await page.getByRole('button',{name:'شروع ساخت صفحه'}).click();
        await page.waitForSelector('#workspace:not([hidden])');
        id=await page.locator('body').getAttribute('data-profile-id');
        key=await page.locator('#new-key').textContent();assert.equal(key.length,64);
        const cookies=await context.cookies();const cookie=cookies.find(c=>c.name===`profile_owner_${id}`);
        assert.ok(cookie?.httpOnly);assert.equal(cookie.sameSite,'Strict');
        assert.equal(await page.locator('input[name=englishName]').count(),0);
        await page.locator('#profile-name').fill('ه');await saved();
        await page.reload();await page.waitForSelector('#workspace:not([hidden])');
        assert.equal(await page.locator('#profile-name').inputValue(),'ه');
        assert.equal(await page.locator('#key-panel').isVisible(),false);
        const localSecrets=await page.evaluate(k=>location.href.includes(k)||JSON.stringify(localStorage).includes(k),key);assert.equal(localSecrets,false);
        await page.locator('#profile-name').fill('هاتف آزمایشی');
        await page.locator('#profile-title').fill('توسعه‌دهندهٔ C++ و JavaScript');
        await page.locator('#profile-company').fill('شرکت نمونه');
        await page.locator('#profile-bio').fill('این معرفی آزمایشی است.');
        await page.locator('#profile-location').fill('تهران');
        await page.locator('#profile-availability').selectOption('AVAILABLE');
        await page.locator('#add-skill').click();
        await page.locator('#skills-editor input').fill('C++ / طراحی سیستم‌های نرم‌افزاری');await saved();
        for(const width of [360,768,1280]){
            await page.setViewportSize({width,height:1000});
            await page.evaluate(()=>document.fonts.ready);
            assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
            await page.screenshot({path:path.join(screenshots,`editor-${width}.png`),fullPage:true});
            if(width===360){await page.locator('#preview-tab').click();assert.ok(await page.locator('#preview').isVisible());await page.screenshot({path:path.join(screenshots,'preview-360.png'),fullPage:true});await page.locator('#form-tab').click();}
        }
        // Offline edits are retained and restored after a reload.
        await context.setOffline(true);await page.locator('#profile-bio').fill('متن نگهداری‌شده هنگام قطع اتصال');
        await page.waitForTimeout(850);
        assert.ok(await page.evaluate(()=>Object.keys(localStorage).some(k=>k.startsWith('hatef.profile.pending.'))));
        await context.setOffline(false);await page.reload();await page.waitForSelector('#workspace:not([hidden])');await saved();
        assert.equal(await page.locator('#profile-bio').inputValue(),'متن نگهداری‌شده هنگام قطع اتصال');
        await page.locator('#profile-bio').fill('');await page.locator('#skills-editor button').click();await saved();
        await page.reload();await page.waitForSelector('#workspace:not([hidden])');assert.equal(await page.locator('#profile-bio').inputValue(),'');assert.equal(await page.locator('#skills-editor input').count(),0);
        // Two tabs start from the same version; the second must request explicit reconciliation.
        const secondPromise=context.waitForEvent('page');
        await page.evaluate(()=>window.open(location.href));
        const second=await secondPromise;second.on('pageerror',e=>errors.push(e.message));
        await second.waitForLoadState();await second.waitForSelector('#workspace:not([hidden])');
        await page.locator('#profile-company').fill('ویرایش زبانهٔ اول');await saved();
        await second.locator('#profile-title').fill('ویرایش زبانهٔ دوم');await second.waitForSelector('#conflict:not([hidden])');
        assert.equal(await second.locator('#profile-title').inputValue(),'ویرایش زبانهٔ دوم');
        await second.locator('#reload-version').click();await second.waitForSelector('#conflict-review:not([hidden])');
        await second.locator('#keep-local').click();await saved(second);
        await page.reload();await page.waitForSelector('#workspace:not([hidden])');
        assert.equal(await page.locator('#profile-company').inputValue(),'ویرایش زبانهٔ اول');
        assert.equal(await page.locator('#profile-title').inputValue(),'ویرایش زبانهٔ دوم');
        await second.close();
        await page.locator('#publish').click();await page.waitForFunction(()=>document.getElementById('publication-label').textContent==='صفحهٔ منتشرشده');
        const publicPage=await context.newPage();assert.equal((await publicPage.goto(url)).status(),200);await publicPage.close();
        await page.locator('#profile-title').fill('پس از انتشار');await saved();
        await page.locator('#logout').click();await page.waitForSelector('#login-panel:not([hidden])');
        assert.equal(await page.evaluate(()=>Object.keys(localStorage).some(k=>k.startsWith('hatef.profile.pending.'))),false);
        await page.locator('#owner-key').fill('wrong');await page.getByRole('button',{name:'ورود به صفحهٔ من'}).click();await page.waitForSelector('#notice:not([hidden])');assert.equal(await page.locator('#workspace').isVisible(),false);
        await page.locator('#owner-key').fill(key);await page.getByRole('button',{name:'ورود به صفحهٔ من'}).click();await page.waitForSelector('#workspace:not([hidden])');
        assert.equal(await page.locator('#profile-title').inputValue(),'پس از انتشار');
        assert.deepEqual(errors,[]);
        console.log('Browser passed: responsive invitation/editor/preview, one-character reload, offline recovery, clearing, two-tab conflict, publication, key login/logout.');
    } finally {
        if(id&&key) await context.request.delete(`${base}/api/profiles/${id}`,{headers:{Authorization:`Bearer ${key}`}});
        await browser.close();
    }
})().catch(error=>{console.error(error);process.exitCode=1;});
