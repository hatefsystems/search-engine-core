// Real template/modules + isolated in-memory API. No production data is touched.
const {chromium}=require('playwright');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const {start,sections}=require('./workspace-fixture.cjs');
(async()=>{
 const server=start(0);await new Promise(resolve=>server.listening?resolve():server.once('listening',resolve));
 const base=`http://127.0.0.1:${server.address().port}`;
 const out='build/profile/workspace-screenshots';fs.mkdirSync(out,{recursive:true});
 const browser=await chromium.launch({headless:true,executablePath:process.env.PROFILE_CHROMIUM_EXECUTABLE || undefined,args:['--no-sandbox','--disable-dev-shm-usage']});
 const context=await browser.newContext({viewport:{width:1440,height:1050}});const page=await context.newPage();
 const errors=[];page.on('pageerror',e=>errors.push(e.message));page.on('dialog',d=>d.accept());
 const nav=key=>page.locator(`#editor-section-nav [data-section="${key}"]`).click();
 const saved=()=>page.waitForFunction(()=>document.querySelector('#save-status').textContent==='ذخیره شد');
 const data=async()=> (await (await context.request.get(base+'/api/profiles/fixture')).json()).data;
 const preview=page.frameLocator('#public-preview');
 const form=()=>page.locator('.content-item-form');
 const assertLayout=async(width)=>{
  const measurements=await page.evaluate(()=>({viewport:innerWidth,width:document.documentElement.scrollWidth,fields:[...document.querySelectorAll('#profile-form input,#profile-form textarea,#profile-form select')].filter(e=>e.getClientRects().length).map(e=>({label:e.getAttribute('aria-label')||e.name||e.id,x:e.getBoundingClientRect().x,right:e.getBoundingClientRect().right}))}));
  assert.ok(measurements.width<=measurements.viewport,`page overflows at ${width}: ${JSON.stringify(measurements)}`);
  for(const field of measurements.fields)assert.ok(field.x>=0 && field.right<=width+1,`field clipped at ${width}: ${JSON.stringify(field)}`);
 };
 try{
  await page.goto(base);await page.locator('#editor-section-nav button').first().waitFor();await page.evaluate(()=>document.fonts.ready);
  assert.equal(await page.locator('html').getAttribute('dir'),'rtl');
  const geo=await page.locator('.editor-columns').evaluate(e=>[...e.children].map(c=>c.getBoundingClientRect().x));
  assert.ok(geo[0]>geo[1]&&geo[1]>geo[2],'sidebar right, editor center, preview left');
  await page.locator('#profile-name').fill('هاتف آزمایشی');await saved();assert.equal((await data()).name,'هاتف آزمایشی');
  assert.equal(await preview.locator('#preview-name').textContent(),'هاتف آزمایشی');
  await page.locator('#profile-bio').fill('معرفی زنده با متن فارسی و DevOps');await saved();
  assert.equal(await preview.locator('#preview-bio').textContent(),'معرفی زنده با متن فارسی و DevOps');
  await page.locator('#add-section').click();assert.ok(await page.locator('#section-picker').isVisible());await page.keyboard.press('Escape');assert.equal(await page.locator('#section-picker').isVisible(),false);
  // Exercise every schema, preserving all pre-existing API fields.
  for(const def of sections){
   await nav(def.key);
   if(!await form().count())await page.locator('.add-content-item').click();
   const input=form().getByLabel(({roleTitle:'عنوان نقش',title:'عنوان',name:'نام',institutionName:'نام دانشگاه یا مرجع یادگیری',repositoryName:'نام مخزن',authorName:'نام نویسندهٔ توصیه‌نامه',label:'عنوان راه ارتباطی',type:'نوع'})[def.titleField],{exact:true});
   if(def.key==='availability')await input.selectOption('CONSULTING');else await input.fill(`آزمون ${def.key}`);
   await saved();assert.ok((await data()).sections[def.key].length);
   const rendered=await form().locator('[data-field]').evaluateAll(nodes=>nodes.map(n=>n.dataset.field));
   for(const field of Object.keys(def.defaults).filter(k=>k!=='media'))assert.ok(rendered.includes(field),`${def.key}.${field} remains editable`);
   await assertLayout(1440);
   if(!['about','availability'].includes(def.key)){await page.evaluate(()=>window.scrollTo(0,0));await page.screenshot({path:`${out}/${def.key}-1440.png`,fullPage:true});}
  }
  // Structured introduction/availability must render in the header and obey privacy.
  await nav('about');await form().locator('[data-field=description] textarea').fill('معرفی ساخت‌یافتهٔ عمومی');await form().getByLabel('نمایش عمومی',{exact:true}).check();await saved();
  assert.equal(await preview.locator('#preview-bio').textContent(),'معرفی ساخت‌یافتهٔ عمومی');
  await nav('basic');await page.locator('#profile-bio').fill('ویرایش معرفی از فرم اصلی');await saved();assert.equal((await data()).sections.about[0].description,'ویرایش معرفی از فرم اصلی');assert.equal(await preview.locator('#preview-bio').textContent(),'ویرایش معرفی از فرم اصلی');await nav('about');
  await form().getByLabel('نمایش عمومی',{exact:true}).uncheck();await saved();assert.equal(await preview.locator('#preview-about').isVisible(),false);
  await nav('availability');await form().locator('[data-field=status] select').selectOption('AVAILABLE');await form().getByLabel('نمایش عمومی',{exact:true}).check();await saved();assert.equal(await preview.locator('#preview-availability').isVisible(),true);
  await nav('basic');await page.locator('#profile-availability').selectOption('BUSY');await saved();assert.equal((await data()).sections.availability[0].status,'BUSY');
  await nav('experiences');assert.equal(await form().count(),1);assert.equal(await page.locator('.item-choice').count(),2);
  await form().getByRole('button',{name:'کپی',exact:true}).click();await saved();
  assert.equal((await data()).sections.experiences.length,3);
  assert.equal((await data()).sections.experiences[2].visibility,'HIDDEN');
  await form().getByLabel('عنوان نقش',{exact:true}).fill('نسخهٔ کپی');await saved();
  await form().getByRole('button',{name:'بالاتر',exact:true}).click();await saved();assert.equal((await data()).sections.experiences[1].roleTitle,'نسخهٔ کپی');
  await form().getByRole('button',{name:'حذف آیتم',exact:true}).click();await saved();assert.equal((await data()).sections.experiences.length,2);
  await page.getByLabel('جستجو در موارد',{exact:true}).fill('پیدا نمی‌شود');assert.equal(await page.locator('.item-choice:visible').count(),0);await page.getByLabel('جستجو در موارد',{exact:true}).fill('');
  await form().getByLabel('نمایش عمومی',{exact:true}).uncheck();await saved();assert.equal(await preview.locator('#section-experiences').getByText('آزمون experiences',{exact:true}).count(),0);
  await form().getByLabel('نمایش عمومی',{exact:true}).check();await saved();
  await form().getByLabel('همچنان ادامه دارد',{exact:true}).check();await saved();
  for(const control of await form().locator('[data-field=endDate] input, [data-field=endDate] select').all())assert.ok(await control.isDisabled());
  await form().getByLabel('همچنان ادامه دارد',{exact:true}).uncheck();await saved();assert.equal(await form().locator('[data-field=endDate] input').first().isDisabled(),false);
  // Unsaved/offline drafts, then recovery and clearing without losing the selected section.
  await nav('projects');await context.setOffline(true);await form().getByLabel('راه‌حل شما',{exact:true}).fill('نوشتهٔ آفلاین');await page.waitForTimeout(800);
  assert.ok(await page.evaluate(()=>Object.keys(localStorage).some(k=>k.startsWith('hatef.profile.pending.content.'))));
  await context.setOffline(false);await page.reload();await form().waitFor();await saved();assert.equal(await form().getByLabel('راه‌حل شما',{exact:true}).inputValue(),'نوشتهٔ آفلاین');
  await form().getByLabel('راه‌حل شما',{exact:true}).fill('');await saved();assert.equal((await data()).sections.projects[0].solution,'');
  // Header/content saves share one version even when navigation interrupts typing.
  await form().getByLabel('نقش شما',{exact:true}).fill('توسعه‌دهنده');await nav('basic');await page.locator('#profile-title').fill('مهندس پلتفرم');await saved();assert.equal((await data()).sections.projects[0].role,'توسعه‌دهنده');
  // Upload preview uses the actual media response; clearing updates both thumbnails.
  await page.locator('#avatar-file').setInputFiles({name:'avatar.png',mimeType:'image/png',buffer:Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+j5l8AAAAASUVORK5CYII=','base64')});
  await page.locator('#editor-avatar:not([hidden])').waitFor();await saved();assert.ok(await preview.locator('#preview-avatar').isVisible());
  await page.locator('[data-clear=avatarUrl]').click();await saved();assert.equal(await page.locator('#editor-avatar').isVisible(),false);
  await nav('links');await page.getByRole('button',{name:'+ افزودن لینک',exact:true}).click();
  await page.getByLabel('عنوان لینک',{exact:true}).fill('لینک تست');await page.getByLabel('آدرس کامل',{exact:true}).fill('https://example.com');await page.getByLabel('نمایش لینک',{exact:true}).selectOption('PUBLIC');await saved();
  assert.equal(await preview.locator('#section-links a').getAttribute('href'),'https://example.com/');
  await page.reload();await page.getByLabel('عنوان لینک',{exact:true}).waitFor();assert.equal(await page.getByLabel('عنوان لینک',{exact:true}).inputValue(),'لینک تست');
  // Conflicting tabs preserve both drafts and reconcile using the current version.
  await nav('basic');const second=await context.newPage();await second.goto(base);await second.locator('#editor-section-nav button').first().waitFor();await second.locator('[data-section=basic]').click();
  await page.locator('#profile-company').fill('شرکت جدید');await saved();await second.locator('#profile-title').fill('ویرایش زبانهٔ دوم');
  await second.locator('#conflict:not([hidden])').waitFor();await second.locator('#reload-version').click();await second.locator('#keep-local').click();
  await second.waitForFunction(()=>document.querySelector('#save-status').textContent==='ذخیره شد');assert.equal((await data()).company,'شرکت جدید');assert.equal((await data()).title,'ویرایش زبانهٔ دوم');await second.close();await page.reload();
  for(const width of [1920,1440,1280,1024,768,390,320]){
   await page.setViewportSize({width,height:1050});await nav('basic');await assertLayout(width);await page.screenshot({path:`${out}/basic-${width}.png`,fullPage:true});
   await nav('experiences');await assertLayout(width);await page.screenshot({path:`${out}/experience-${width}.png`,fullPage:true});
   if(width<=760){await page.locator('#preview-tab').click();assert.ok(await page.locator('#preview').isVisible());assert.equal(await page.locator('#profile-form').isVisible(),false);await assertLayout(width);await page.screenshot({path:`${out}/preview-${width}.png`,fullPage:true});await page.locator('#form-tab').click();}
  }
  await page.setViewportSize({width:1440,height:1050});await nav('basic');await page.locator('[data-preview-size=mobile]').click();assert.ok((await page.locator('.preview-surface').boundingBox()).width<=360);await page.locator('[data-preview-size=desktop]').click();
  await page.locator('#publish').click();await page.waitForFunction(()=>document.querySelector('#publication-label').textContent==='صفحهٔ منتشرشده');assert.equal((await data()).isPublic,true);
  await page.locator('#logout').click();await page.locator('#login-panel:not([hidden])').waitFor();
  assert.equal(await page.evaluate(()=>Object.keys(localStorage).some(k=>k.startsWith('hatef.profile.pending.'))),false);
  assert.deepEqual(errors,[]);
  console.log('PASS: 14 sections, RTL columns, CRUD/copy/order, privacy, offline/reload, shared saves, conflict, media, links, publish/logout, 7 viewports, no JS errors. API responses are fixtures; backend integration is separate.');
 }finally{await page.screenshot({path:`${out}/last-state.png`}).catch(()=>{});await browser.close();await new Promise(resolve=>server.close(resolve));}
})().catch(e=>{console.error(e);process.exitCode=1;});
