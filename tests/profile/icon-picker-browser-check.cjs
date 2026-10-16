const {chromium}=require('playwright');
const assert=require('node:assert/strict'), fs=require('node:fs'), http=require('node:http');
(async()=>{
 const server=http.createServer((req,res)=>{
  if(req.url==='/picker.js'){res.setHeader('Content-Type','text/javascript');return res.end(fs.readFileSync('public/assets/js/profile-icon-picker.js'));}
  if(req.url==='/picker.css'){res.setHeader('Content-Type','text/css');return res.end(fs.readFileSync('public/assets/css/profile-content.css'));}
  res.setHeader('Content-Type','text/html');res.end('<html dir="rtl"><meta charset="utf-8"><link rel="stylesheet" href="/picker.css"><body><div id="mount"></div><script type="module">import {mountIconPicker,iconUrl} from "/picker.js";window.item={title:"زیرساخت ابری",iconMode:"auto",media:[{id:"owned",alt:"تصویر شخصی"}]};window.changes=0;window.picker=mountIconPicker({item:window.item,profileId:"profile",onChange:()=>window.changes++});document.querySelector("#mount").append(picker);window.iconUrl=iconUrl;window.mountIconPicker=mountIconPicker;</script></body></html>');
 });server.listen(0,'127.0.0.1');await new Promise(r=>server.once('listening',r));
 const browser=await chromium.launch({headless:true,executablePath:process.env.PROFILE_CHROMIUM_EXECUTABLE||undefined,args:['--no-sandbox','--disable-dev-shm-usage']});
 const page=await browser.newPage();const errors=[];page.on('pageerror',e=>errors.push(e.message));let suggestions=0,queries=[],delayed=false,lowConfidence=false,offline=false,searchGate=null;
 const icon=id=>({id,labelFa:id});
 await page.route('**/assets/icons/**',route=>route.fulfill({contentType:'image/svg+xml',body:'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24"><path d="M0 0h20v20"/></svg>'}));
 await page.route('**/api/profiles/**',route=>route.fulfill({contentType:'image/svg+xml',body:'<svg xmlns="http://www.w3.org/2000/svg"/>'}));
 await page.route('**/api/icons/**',async route=>{
  const url=new URL(route.request().url());if(offline)return route.fulfill({status:503,json:{error:'unavailable'}});
  if(url.pathname.endsWith('/suggest')) {suggestions++;if(delayed)await new Promise(r=>setTimeout(r,300));return route.fulfill({json:{suggestions:[icon('lucide:cloud'),icon('lucide:scale'),icon('lucide:wrench')],lowConfidence}});}
  if(url.pathname.endsWith('/categories'))return route.fulfill({json:{categories:[{category:'law',labelFa:'حقوق و قرارداد'}]}});
  queries.push(url.searchParams.toString());if(searchGate)await searchGate;return route.fulfill({json:{items:[icon('lucide:scale'),icon('lucide:wrench')],nextOffset:url.searchParams.get('offset')==='0'?24:null}});
 });
 try{
  await page.goto(`http://127.0.0.1:${server.address().port}`);await page.waitForFunction(()=>window.item?.iconId==='lucide:cloud');
  await page.getByRole('button',{name:'lucide:scale',exact:true}).click();assert.equal(await page.evaluate(()=>item.iconMode),'manual');
  assert.equal(await page.getByRole('button',{name:'lucide:scale',exact:true}).getAttribute('aria-pressed'),'true');assert.ok((await page.getByRole('button',{name:'lucide:scale',exact:true}).getAttribute('class')).includes('is-selected'));
  const before=suggestions;await page.evaluate(()=>{item.title='تعمیرات';picker.notify();});await page.waitForTimeout(700);assert.equal(suggestions,before);assert.equal(await page.evaluate(()=>item.iconId),'lucide:scale');
  await page.getByText('جستجو در آیکن‌ها',{exact:true}).click();await page.locator('details .icon-results button').first().waitFor();
  assert.equal(await page.getByLabel('منبع',{exact:true}).count(),0);assert.equal(await page.getByLabel('سبک',{exact:true}).count(),0);assert.equal(await page.getByRole('option',{name:'حقوق و قرارداد',exact:true}).count(),1);await page.getByLabel('دسته',{exact:true}).selectOption('law');await page.getByLabel('جستجوی آیکن',{exact:true}).fill('وکالت');await page.waitForTimeout(450);
  assert.ok(queries.some(q=>q.includes('category=law')&&!q.includes('style=')&&!q.includes('source=')));
  assert.equal(await page.locator('details button[data-icon-id="lucide:scale"]').getAttribute('aria-pressed'),'true');
  await page.getByRole('button',{name:'بعدی',exact:true}).click();await page.waitForTimeout(100);assert.ok(queries.at(-1).includes('offset=24'));
  assert.equal(await page.locator('details button[data-icon-id="lucide:scale"]').getAttribute('aria-pressed'),'true');
  await page.getByText('جستجو در آیکن‌ها',{exact:true}).click();
  let releaseSearch;searchGate=new Promise(resolve=>{releaseSearch=resolve;});
  const reopenedSearch=page.waitForRequest(request=>request.url().includes('/api/icons/search'));
  await page.getByText('جستجو در آیکن‌ها',{exact:true}).click();await reopenedSearch;
  const choices=page.locator('details .icon-results button');await choices.first().focus();await page.keyboard.press('ArrowRight');assert.equal(await choices.nth(1).evaluate(e=>e===document.activeElement),true);
  await page.evaluate(()=>{window.choiceBeforeRefresh=document.activeElement;});releaseSearch();searchGate=null;
  await page.waitForFunction(()=>document.activeElement!==window.choiceBeforeRefresh&&document.activeElement?.dataset.iconId==='lucide:wrench');
  await page.keyboard.press('Enter');assert.equal(await page.evaluate(()=>item.iconId),'lucide:wrench');await choices.first().click();
  await page.getByLabel('روش انتخاب آیکن',{exact:true}).selectOption('custom');assert.equal(await page.evaluate(()=>iconUrl(item,'profile')),'/api/profiles/profile/media/owned');
  await page.getByLabel('روش انتخاب آیکن',{exact:true}).selectOption('none');assert.equal(await page.evaluate(()=>iconUrl(item,'profile')),'');assert.equal(await page.locator('.icon-results [aria-pressed="true"]').count(),0);
  assert.equal(await page.evaluate(()=>iconUrl({iconMode:'manual',iconId:'javascript:alert(1)'},'p')),'');assert.equal(await page.evaluate(()=>iconUrl({iconMode:'custom',iconMediaId:'other',media:[]},'p')),'');
  delayed=true;await page.getByLabel('روش انتخاب آیکن',{exact:true}).selectOption('auto');await choices.first().click();await page.waitForTimeout(400);assert.equal(await page.evaluate(()=>item.iconId),'lucide:scale');assert.equal(await page.evaluate(()=>item.iconMode),'manual');
  delayed=false;lowConfidence=true;await page.getByLabel('روش انتخاب آیکن',{exact:true}).selectOption('auto');await page.waitForTimeout(200);assert.equal(await page.evaluate(()=>item.iconId),'lucide:scale');
  offline=true;await page.evaluate(()=>{item.title='عنوان تازه';picker.notify();});await page.waitForTimeout(650);assert.equal(await page.evaluate(()=>item.iconId),'lucide:scale');assert.ok((await page.getByRole('status').textContent()).includes('در دسترس نیست'));
  for(const width of [360,390,768,1280]){await page.setViewportSize({width,height:850});assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),`overflow ${width}`);}
  offline=false;await page.evaluate(()=>{picker.remove();window.picker=mountIconPicker({item,profileId:'profile',onChange:()=>changes++});document.querySelector('#mount').append(picker);});await page.getByText('جستجو در آیکن‌ها',{exact:true}).click();await page.locator('details .icon-results button').first().waitFor();assert.equal(await page.locator('details button[data-icon-id="lucide:scale"]').getAttribute('aria-pressed'),'true');
  if(process.env.ICON_PICKER_SCREENSHOT){await page.setViewportSize({width:850,height:750});offline=false;await page.getByLabel('روش انتخاب آیکن',{exact:true}).selectOption('manual');await page.getByLabel('جستجوی آیکن',{exact:true}).fill('قلب');await page.waitForTimeout(500);await choices.first().click();await page.screenshot({path:process.env.ICON_PICKER_SCREENSHOT,fullPage:true});}
  assert.deepEqual(errors,[]);console.log('PASS icon picker: auto/manual/custom/none, Persian categories, selected state, pagination, keyboard, unsafe URLs, stale/low-confidence/offline responses and RTL widths');
 }finally{await browser.close();server.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
