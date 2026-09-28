const {chromium}=require('playwright');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const base=process.env.PROFILE_TEST_BASE_URL;
assert(base,'An isolated PROFILE_TEST_BASE_URL is required.');
const out='build/profile/advanced-screenshots';fs.mkdirSync(out,{recursive:true});
(async()=>{
 const browser=await chromium.launch({headless:true,args:['--no-sandbox','--no-proxy-server']});
 const context=await browser.newContext();const page=await context.newPage();const errors=[];page.setDefaultTimeout(15000);
 page.on('pageerror',e=>errors.push(e.message));page.on('dialog',d=>d.accept());let id,key;
 const saved=async(p=page)=>p.waitForFunction(()=>document.querySelector('#save-status').textContent==='ذخیره شد');
 const nav=label=>page.locator('.section-nav').getByRole('button',{name:label,exact:true}).click();
 const item=()=>page.locator('.content-item-form').last();
 const ownerData=async()=>{const r=await context.request.get(base+'/api/profiles/'+id);return (await r.json()).data;};
 try {
  const slug='آزمون.جامع.'+Date.now();await page.goto(base+'/profiles/new?slug='+encodeURIComponent(slug));
  await page.locator('#start').click();await page.waitForSelector('#workspace:not([hidden])');
  id=await page.locator('body').getAttribute('data-profile-id');key=await page.locator('#new-key').textContent();
  await page.locator('#profile-name').fill('شخص آزمایشی');await saved();
  await page.waitForSelector('.section-nav');await nav('پروژه‌ها و نمونه‌کارها');
  await page.getByRole('button',{name:'+ افزودن پروژه‌ها و نمونه‌کارها',exact:true}).click();
  await item().getByLabel('عنوان',{exact:true}).fill('پ');await saved();
  await page.reload();await page.waitForSelector('.content-item-form');
  assert.equal(await item().getByLabel('عنوان',{exact:true}).inputValue(),'پ');
  const title='مطالعهٔ موردی طراحی سامانهٔ C++ با نام طولانی '.repeat(3);
  await item().getByLabel('عنوان',{exact:true}).fill(title);
  await item().getByLabel('مسئله چه بود؟',{exact:true}).fill('مشکل واقعی کاربران');
  await item().getByLabel('راه‌حل شما',{exact:true}).fill('راه‌حل قابل بررسی');
  await item().getByLabel('نتایج',{exact:false}).fill('بهبود تجربهٔ کاربر\nکاهش زمان پاسخ');
  await item().getByLabel('نمایش عمومی',{exact:true}).check();await saved();
  assert.ok((await ownerData()).sections.projects[0].visibility==='PUBLIC');
  // Upload through the actual browser form, then remove the private media reference.
  const pixels=await page.evaluate(()=>{const canvas=document.createElement('canvas');canvas.width=2;canvas.height=2;return canvas.toDataURL('image/png').split(',')[1];});
  await item().getByLabel('توضیح تصویر پروژه',{exact:true}).fill('تصویر مرورگر');
  await item().getByLabel('افزودن تصویر پروژه',{exact:true}).setInputFiles({name:'fixture.png',mimeType:'image/png',buffer:Buffer.from(pixels,'base64')});
  await item().getByRole('button',{name:'حذف تصویر: تصویر مرورگر',exact:true}).waitFor();await saved();
  assert.equal((await ownerData()).sections.projects[0].media.length,1);
  await item().getByRole('button',{name:'حذف تصویر: تصویر مرورگر',exact:true}).click();await saved();
  assert.equal((await ownerData()).sections.projects[0].media.length,0);
  // Header and content mutations share a serialized profile version.
  await nav('اطلاعات اصلی و معرفی');await page.locator('#profile-title').fill('طراح و توسعه‌دهنده');await nav('پروژه‌ها و نمونه‌کارها');await item().getByLabel('نقش شما',{exact:true}).fill('طراحی و پیاده‌سازی');await saved();
  assert.equal((await ownerData()).title,'طراح و توسعه‌دهنده');assert.equal((await ownerData()).sections.projects[0].role,'طراحی و پیاده‌سازی');
  for(const [section,label,titleField,title] of [
   ['about','دربارهٔ من','عنوان','معرفی'],['experiences','تجربهٔ کاری','عنوان نقش','داوطلب'],['skills','مهارت‌ها','نام','C++'],
   ['education','تحصیلات و یادگیری','نام دانشگاه یا مرجع یادگیری','یادگیری مستقل'],['certifications','گواهینامه‌ها','نام','گواهینامه'],
   ['publications','آثار و انتشارات','عنوان','مقاله'],['openSource','مشارکت متن‌باز','نام مخزن','کتابخانه'],['services','خدمات','عنوان','مشاوره'],
   ['achievements','دستاوردها','عنوان','جایزه'],['languages','زبان‌ها','نام','فارسی'],['recommendations','توصیه‌نامه‌های مستند','نام نویسندهٔ توصیه‌نامه','نویسنده'],
   ['contacts','راه‌های ارتباطی','عنوان راه ارتباطی','وب‌سایت'],['availability','فرصت‌های همکاری','نوع','CONSULTING']
  ]) {
   await nav(label);await page.getByRole('button',{name:'+ افزودن '+label,exact:true}).click();
   const input=item().getByLabel(titleField,{exact:true});
   if(section==='availability')await input.selectOption(title);else await input.fill(title);
   if(section==='recommendations'){await item().getByLabel('متن توصیه‌نامه',{exact:true}).fill('متن مستند');await item().getByLabel('لینک منبع عمومی',{exact:true}).fill('https://example.com/reference');}
   if(section==='contacts'){await item().getByLabel('نوع',{exact:true}).selectOption('LINK');await item().getByLabel('آدرس یا شماره',{exact:true}).fill('https://example.com');}
   await item().getByLabel('نمایش عمومی',{exact:true}).check();await saved();
   assert.ok((await ownerData()).sections[section].some(i=>i.visibility==='PUBLIC'),section);
  }
  // Links have their own versions and retain incomplete private drafts.
  await nav('لینک‌های صفحه');await page.getByRole('button',{name:'+ افزودن لینک',exact:true}).click();
  const linkBox=()=>page.locator('#links-editor fieldset').last();
  await linkBox().getByLabel('عنوان لینک',{exact:true}).fill('ل');await saved();
  await page.reload();await page.waitForSelector('#links-editor fieldset');
  assert.equal(await linkBox().getByLabel('عنوان لینک',{exact:true}).inputValue(),'ل');
  await linkBox().getByLabel('آدرس کامل',{exact:true}).fill('https://example.com/portfolio');
  await linkBox().getByLabel('توضیح',{exact:true}).fill('لینک آزمایشی');
  await linkBox().getByLabel('نمایش لینک',{exact:true}).selectOption('PUBLIC');await saved();
  assert.equal((await (await context.request.get(base+'/api/profiles/'+id+'/links')).json()).data[0].url,'https://example.com/portfolio');
  await linkBox().getByLabel('توضیح',{exact:true}).fill('');await saved();
  assert.equal((await (await context.request.get(base+'/api/profiles/'+id+'/links')).json()).data[0].description,'');
  await nav('پروژه‌ها و نمونه‌کارها');
  // An offline edit is durable and can be recovered after reload.
  await context.setOffline(true);await item().getByLabel('راه‌حل شما',{exact:true}).fill('تغییر حفظ‌شده در قطع اینترنت');await page.waitForTimeout(800);
  assert.ok(await page.evaluate(()=>Object.keys(localStorage).some(k=>k.startsWith('hatef.profile.pending.content.'))));
  await context.setOffline(false);await page.reload();await page.waitForSelector('.content-item-form');await saved();
  assert.equal(await item().getByLabel('راه‌حل شما',{exact:true}).inputValue(),'تغییر حفظ‌شده در قطع اینترنت');
  await item().getByLabel('راه‌حل شما',{exact:true}).fill('');await saved();assert.equal((await ownerData()).sections.projects[0].solution,'');
  // Same-version tabs require explicit reconciliation and retain the local item draft.
  const second=await context.newPage();await second.goto(page.url());await second.waitForSelector('.content-item-form');
  await item().getByLabel('نقش شما',{exact:true}).fill('نقش زبانهٔ اول');await saved();
  await second.locator('.content-item-form').getByLabel('معماری و روش انجام',{exact:true}).fill('نوشتهٔ زبانهٔ دوم');await second.waitForSelector('#conflict:not([hidden])');
  await second.locator('#reload-version').click();await second.waitForSelector('#conflict-review:not([hidden])');await second.locator('#keep-local').click();await saved(second);
  assert.equal((await ownerData()).sections.projects[0].architecture,'نوشتهٔ زبانهٔ دوم');assert.equal((await ownerData()).sections.projects[0].role,'نقش زبانهٔ اول');await second.close();
  await page.reload();await page.waitForSelector('.section-nav');
  for(const width of [320,360,768,1280]){
   await page.setViewportSize({width,height:950});await page.evaluate(()=>document.fonts.ready);
   assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'editor overflow '+width);
   await page.screenshot({path:`${out}/editor-${width}.png`,fullPage:true});
   if(width<768){await page.locator('#preview-tab').click();assert.ok(await page.locator('#preview').isVisible());assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));await page.screenshot({path:`${out}/preview-${width}.png`,fullPage:true});await page.locator('#form-tab').click();}
  }
  await page.locator('#publish').click();await page.waitForFunction(()=>document.querySelector('#publication-label').textContent==='صفحهٔ منتشرشده');
  const publicPage=await context.newPage();publicPage.on('pageerror',e=>errors.push(e.message));
  for(const width of [320,360,768,1280]){await publicPage.setViewportSize({width,height:950});const response=await publicPage.goto(base+'/'+encodeURIComponent(slug));assert.equal(response.status(),200);assert.ok(await publicPage.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),'public overflow '+width);await publicPage.screenshot({path:`${out}/public-${width}.png`,fullPage:true});}
  await publicPage.goto(base+'/people?q=C%2B%2B');await publicPage.waitForSelector('.people-card');assert.ok(await publicPage.getByRole('link',{name:'شخص آزمایشی',exact:true}).count());
  assert.ok(await publicPage.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));await publicPage.screenshot({path:`${out}/people-1280.png`,fullPage:true});
  await page.locator('#logout').click();await page.waitForSelector('#login-panel:not([hidden])');
  assert.equal(await page.evaluate(()=>Object.keys(localStorage).some(k=>k.startsWith('hatef.profile.pending.'))),false);
  assert.deepEqual(errors,[]);console.log('PASS advanced browser: every section, mixed text, one-character resume, shared saves, offline recovery, clearing, two tabs, privacy, publication, people search, 320/360/768/1280, logout.');
 } finally {if(id&&key)await context.request.delete(base+'/api/profiles/'+id,{headers:{Authorization:'Bearer '+key}});await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
