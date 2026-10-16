const assert = require('node:assert/strict');
const fs = require('node:fs/promises');
const path = require('node:path');
const { pathToFileURL } = require('node:url');
const { chromium } = require('playwright');

(async () => {
  const browser = await chromium.launch({ headless: true, args: ['--no-sandbox'] });
  try {
    const page = await browser.newPage({ viewport: { width: 1440, height: 1100 } });
    const errors = [];
    page.on('pageerror', error => errors.push(error.message));
    const open = async (packet) => {
      page.once('dialog', dialog => dialog.accept());
      await page.goto(pathToFileURL(path.join(__dirname, '../scripts/review.html')).href);
      await page.locator('#packetFile').setInputFiles({ name: 'fixture.json', mimeType: 'application/json', buffer: Buffer.from(JSON.stringify(packet)) });
    };
    const download = async () => {
      const promise = page.waitForEvent('download');
      await page.locator('#download').click();
      const result = await promise;
      return JSON.parse(await fs.readFile(await result.path(), 'utf8'));
    };
    await open({mode:'translation', concepts:[{id:'fixture-translation', labelEn:'Beekeeping', descriptionEn:'Keep and maintain beehives.', aliasesEn:['apiculture'], labelFa:'زنبورداری', descriptionFa:'نگهداری کندوهای زنبور عسل.', aliasesFa:['پرورش زنبور عسل'], icons:[], translationStatus:'pending', reviewer:'', humanReviewed:false, qualityFlags:[]}]});
    await page.locator('#reviewer').fill('fixture-ui-reviewer');
    await page.locator('#approve').click();
    assert.match(await page.locator('#message').innerText(), /تأیید بررسی شخصی/);
    assert.equal((await download()).concepts[0].translationStatus, 'pending');
    await page.locator('#human').check();
    await page.locator('#approve').click();
    let packet = await download();
    assert.equal(packet.concepts[0].translationStatus, 'reviewed');
    assert.equal(packet.concepts[0].humanReviewed, true);
    await page.locator('#text').fill('پرورش زنبور عسل');
    packet = await download();
    assert.equal(packet.concepts[0].translationStatus, 'pending');
    assert.equal(packet.concepts[0].humanReviewed, false);

    await open({mode:'annotation', catalogVersion:'fixture-v1', reviewer:'fixture-ui-annotator', cases:[{id:'fixture-case',domain:'کشاورزی',text:'زنبورداری',relevant:[],noSuitableIcon:false,reviewStatus:'pending',notes:''}]});
    assert.equal(await page.locator('#text').getAttribute('readonly'), '');
    assert.equal(await page.locator('#filter').inputValue(), '');
    await page.locator('#catalogFile').setInputFiles({name:'catalog.json',mimeType:'application/json',buffer:Buffer.from(JSON.stringify({mode:'catalog',catalogVersion:'fixture-v1',icons:[{id:'lucide:cloud',labelEn:'cloud',labelFa:'ابر',svg:'<svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="8"/></svg>'},{id:'lucide:bug',labelEn:'bug',labelFa:'حشره',svg:'<svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="6"/></svg>'}]}))});
    assert.equal(await page.locator('.icon').first().locator('.ltr').innerText(),'lucide:bug');
    await page.locator('.icon').first().click();
    await page.locator('#human').check();
    await page.locator('#approve').click();
    assert.match(await page.locator('#message').innerText(), /استقلال از توسعه/);
    await page.locator('#independent').check();
    await page.locator('#blind').check();
    await page.locator('#approve').click();
    packet = await download();
    assert.equal(packet.cases[0].reviewStatus,'reviewed');
    assert.deepEqual(packet.cases[0].relevant,['lucide:bug']);
    assert.equal(packet.isHuman,true);
    await page.locator('#none').check();
    await page.locator('#human').check();
    await page.locator('#approve').click();
    packet = await download();
    assert.equal(packet.cases[0].noSuitableIcon,true);
    assert.deepEqual(packet.cases[0].relevant,[]);
    const output = path.resolve('build/persian-review-ui');
    await fs.mkdir(output,{recursive:true});
    await page.screenshot({path:path.join(output,'annotation-desktop.png'),fullPage:true});
    await page.setViewportSize({width:390,height:844});
    assert.equal(await page.evaluate(()=>document.documentElement.scrollWidth<=window.innerWidth),true);
    await page.screenshot({path:path.join(output,'annotation-mobile.png'),fullPage:true});
    assert.deepEqual(errors,[]);
    console.log('Review workbench: explicit human action, edit invalidation, blind catalog, no-suitable labels and responsive layout passed. Synthetic fixtures only.');
  } finally { await browser.close(); }
})().catch(error=>{console.error(error);process.exit(1);});
