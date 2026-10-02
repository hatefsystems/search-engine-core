// Shared presentation, draft privacy and interaction checks. Backend rendering is
// additionally checked by demo-browser-check.cjs against C++/MongoDB in CI.
const {chromium}=require('playwright');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const {start}=require('./workspace-fixture.cjs');
const demo=require('./demo-profile.cjs');
(async()=>{
 const server=start(0);await new Promise(r=>server.listening?r():server.once('listening',r));
 const browser=await chromium.launch({headless:true,executablePath:process.env.PROFILE_CHROMIUM_EXECUTABLE||undefined,args:['--no-sandbox','--disable-dev-shm-usage']});
 const page=await browser.newPage({viewport:{width:1440,height:1080}}),errors=[];page.on('pageerror',e=>errors.push(e.message));
 const out='build/profile/public-screenshots';fs.mkdirSync(out,{recursive:true});
 try{
  await page.goto(`http://127.0.0.1:${server.address().port}/assets/profile-preview.html`);
  const source={...demo.header,id:'presentation-fixture',sections:demo.sections,privacy:{showEmail:false,showPhone:false,showLocation:true,showAvailability:true},ownerToken:'SECRET-KEY-NEVER-RENDER',email:'SECRET-EMAIL',phone:'SECRET-PHONE',contentLayout:{order:['experiences','skills','projects','education','certifications','publications','recommendations','services','achievements','languages','contacts','openSource','availability'],featured:[{section:'projects',id:'demo-project-hatef'}]}};
  source.sections=structuredClone(source.sections);
  source.sections.projects.push({...source.sections.projects[0],id:'hidden-project',title:'SECRET-PROJECT',visibility:'HIDDEN'});
  source.sections.experiences[0].projectIds=['hidden-project'];
  source.sections.contacts.push({id:'email',type:'EMAIL',label:'SECRET-CONTACT',value:'secret@example.com',visibility:'PUBLIC'});
  const projection=await page.evaluate(async source=>{
   const m=await import('/assets/js/profile-public.js');window.testPublic=m;const p=m.projectPublicProfile(source,[]);window.testData=p;m.renderPublicProfile(document.getElementById('public-preview-root'),p,{preview:true});return p;
  },source);
  assert.ok(!JSON.stringify(projection).includes('SECRET'));assert.deepEqual(projection.sections.experiences[0].projectIds,[]);
  await page.locator('.public-profile').waitFor();await page.evaluate(()=>document.fonts.ready);
  await page.waitForFunction(()=>document.querySelector('.pp-menu a[aria-current=location]')?.hash==='#profile-home');
  assert.equal(await page.locator('#section-publications').count(),0);assert.equal(await page.locator('#section-recommendations').count(),0);
  assert.equal(await page.locator('#preview-bio').textContent(),demo.sections.about[0].description);
  assert.equal(await page.locator('#section-skills .pp-card').count(),8);
  assert.equal(await page.locator('details,summary,[data-more-section],.pp-level,.pp-project-art').count(),0);
  assert.ok(await page.locator('#section-experiences .pp-prose-field').count());
  assert.equal(await page.locator('#preview-bio').count(),1);
  const expectedFacts=await page.locator('#section-experiences .pp-position-facts').first().innerText();
  await page.evaluate(()=>{const p=structuredClone(window.testData);for(const [key,items] of Object.entries(p.sections))p.sections[key]=items.map(item=>Object.fromEntries(Object.entries(item).sort(([a],[b])=>a.localeCompare(b))));window.testPublic.renderPublicProfile(document.getElementById('public-preview-root'),p,{preview:true});});
  assert.equal(await page.locator('#section-experiences .pp-position-facts').first().innerText(),expectedFacts);
  assert.equal(await page.locator('#section-experiences .pp-timeline').first().textContent(),'اسفند ۱۴۰۳ تا اکنون');
  assert.equal(await page.locator('#section-experiences .pp-position-facts [data-field=organizationName]').first().textContent(),'QuickHands Inc.');
  assert.equal(await page.locator('#section-skills .pp-current-badge').count(),8);
  assert.equal(await page.getByText('همچنان ادامه دارد',{exact:true}).count(),0);
  assert.equal(await page.getByText('راه‌حل شما',{exact:true}).count(),0);
  const visibleSectionIds=await page.locator('.pp-section:not([hidden])').evaluateAll(nodes=>nodes.map(n=>n.id));
  assert.equal(await page.locator('.pp-section-index').count(),0);
  for(const id of visibleSectionIds)assert.equal(await page.locator(`#profile-navigation a[href="#${id}"]`).count(),1);
  await page.locator('.pp-more-toggle').click();
  await page.keyboard.press('Escape');assert.equal(await page.locator('.pp-more-toggle').getAttribute('aria-expanded'),'false');
  await page.locator('.pp-more-toggle').click();
  await page.locator('#profile-navigation a[href="#section-certifications"]').click();
  await page.waitForFunction(()=>document.querySelector('#profile-navigation a[aria-current=location]')?.hash==='#section-certifications');
  assert.equal(await page.locator('.pp-more-toggle.has-active-section').count(),1);
  assert.equal(await page.locator('.pp-more-toggle').getAttribute('aria-expanded'),'false');
  const headingTop=await page.locator('#section-certifications').evaluate(n=>n.getBoundingClientRect().top);
  assert.ok(headingTop>=(await page.locator('.pp-nav').boundingBox()).y+(await page.locator('.pp-nav').boundingBox()).height);
  // Scroll without clicking: active state follows the content, not the last link.
  await page.locator('#section-projects').evaluate(n=>window.scrollTo(0,n.getBoundingClientRect().top+scrollY-document.querySelector('.pp-nav').offsetHeight-24));
  await page.waitForFunction(()=>document.querySelector('#profile-navigation a[aria-current=location]')?.hash==='#section-projects');
  const sticky=await page.locator('.pp-nav').boundingBox();assert.ok(sticky.y>=0&&sticky.y<=12);
  assert.equal(await page.locator('#profile-navigation a[aria-current]').count(),1);
  await page.screenshot({path:`${out}/sticky-projects.png`});
  await page.evaluate(()=>scrollTo(0,0));
  assert.deepEqual(await page.evaluate(()=>[window.testPublic.publicDate({calendar:'persian',year:1403}),window.testPublic.publicDate({calendar:'gregory',year:2024,month:2,day:3})]),['۱۴۰۳','۳ فوریه ۲۰۲۴ میلادی']);
  await page.evaluate(()=>window.print=()=>window.printRequested=true);await page.locator('[data-resume]').click();assert.equal(await page.evaluate(()=>window.printRequested),true);
  for(const width of [1920,1440,1280,1024,768,390,320]){
   await page.setViewportSize({width,height:1080});assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),`overflow at ${width}`);
   await page.screenshot({path:`${out}/public-${width}.png`,fullPage:true});if([1440,390].includes(width))await page.screenshot({path:`${out}/hero-${width}.png`});
   if(width<=1100){await page.locator('.pp-menu-toggle').click();assert.equal(await page.locator('.pp-menu-toggle').getAttribute('aria-expanded'),'true');await page.locator('.pp-menu a[href="#section-projects"]').click();assert.equal(await page.locator('.pp-menu-toggle').getAttribute('aria-expanded'),'false');await page.evaluate(()=>scrollTo(0,0));}
  }
  // Real JPEG photographs, loaded by the actual renderer, exercise both white
  // washes without relying on generated mockups or an external image service.
  await page.route('**/assets/test-cover-*.jpg',route=>route.fulfill({contentType:'image/jpeg',path:require('node:path').join(__dirname,'../../public/coming-soon/assets/images',route.request().url().includes('light')?'2.jpg':'slide1.jpg')}));
  for(const cover of ['light','dark']){
   await page.evaluate(cover=>{const p=structuredClone(window.testData);p.coverImageUrl=`/assets/test-cover-${cover}.jpg`;window.testPublic.renderPublicProfile(document.getElementById('public-preview-root'),p,{preview:true});scrollTo(0,0);},cover);
   await page.waitForFunction(()=>document.querySelector('.pp-backdrop img')?.naturalWidth>0);
   for(const width of [1440,768,390,320]){
    await page.setViewportSize({width,height:1080});await page.evaluate(async()=>{await document.fonts.ready;await document.querySelector('.pp-backdrop img').decode();scrollTo(0,0);});
    await page.waitForFunction(()=>document.querySelector('.pp-menu a[aria-current=location]')?.hash==='#profile-home');
    await page.waitForFunction(()=>Math.abs(document.querySelector('.pp-backdrop').getBoundingClientRect().height-(document.querySelector('.pp-hero').offsetTop+document.querySelector('.pp-hero').offsetHeight+60))<2);
    const box=await page.locator('.pp-backdrop').boundingBox();assert.equal(box.x,0);assert.equal(box.width,width);assert.equal(box.y,0);
    assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
    await page.screenshot({path:`${out}/cover-${cover}-${width}.png`});
   }
  }
  await page.unroute('**/assets/test-cover-*.jpg');
  await page.route('**/assets/test-cover-broken.jpg',route=>route.fulfill({status:404,body:''}));
  await page.evaluate(()=>{const p=structuredClone(window.testData);p.coverImageUrl='/assets/test-cover-broken.jpg';window.testPublic.renderPublicProfile(document.getElementById('public-preview-root'),p,{preview:true});});
  await page.locator('.pp-backdrop.pp-image-fallback').waitFor();
  assert.equal(await page.locator('.pp-backdrop img').count(),0);
  // Real asynchronous pagination contract, including a retry and draft replacement.
  let requests=0;
  await page.route('**/api/profiles/presentation-fixture/content/skills?*',async route=>{
   requests++;await new Promise(r=>setTimeout(r,150));
   if(requests===1)return route.fulfill({status:503,json:{success:false}});
   const offset=Number(new URL(route.request().url()).searchParams.get('offset'));
   await route.fulfill({json:{success:true,data:{items:demo.sections.skills.slice(offset,offset+100),total:8}}});
  });
  await page.evaluate(()=>{const p=structuredClone(window.testData);p.totals=Object.fromEntries(Object.entries(p.sections).map(([k,v])=>[k,v.length]));p.sections.skills=p.sections.skills.slice(0,3);window.testPublic.renderPublicProfile(document.getElementById('public-preview-root'),p);});
  await page.getByText('دریافت بخشی از اطلاعات انجام نشد.',{exact:true}).waitFor();
  await page.locator('#section-skills').getByRole('button',{name:'تلاش دوباره برای دریافت اطلاعات'}).click();
  await page.waitForFunction(()=>document.querySelectorAll('#section-skills .pp-card').length===8);
  assert.equal(await page.locator('details,summary,[data-more-section]').count(),0);
  await page.evaluate(()=>{
   const p=structuredClone(window.testData);p.name='<img src=x onerror=alert(1)>';p.sections.about[0].description='<script>alert(1)</script>';p.avatarUrl='javascript:alert(1)';p.coverImageUrl='javascript:alert(1)';p.githubUrl='javascript:alert(1)';p.sections.projects[0].links=[{url:'javascript:alert(1)',title:'bad'}];window.testPublic.renderPublicProfile(document.getElementById('public-preview-root'),p,{preview:true});
  });
  assert.equal(await page.locator('a[href^="javascript:"],img[src^="javascript:"]').count(),0);assert.equal(await page.locator('#preview-name img').count(),0);
  await page.evaluate(()=>window.testPublic.renderPublicProfile(document.getElementById('public-preview-root'),{name:'پروفایل تازه',sections:{}},{preview:true}));
  assert.equal(await page.locator('.pp-section:not([hidden])').count(),0);assert.equal(await page.locator('.pp-cta').count(),0);
  assert.deepEqual(errors,[]);console.log('PASS public UI: seven widths, shared renderer, privacy projection, hidden references, automatic pagination/retry, menu, fully visible content, printable resume, empty state and unsafe URLs.');
 }finally{await browser.close();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1;});
