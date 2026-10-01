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
  assert.equal(await page.locator('#section-publications').count(),0);assert.equal(await page.locator('#section-recommendations').count(),0);
  assert.equal(await page.locator('#preview-bio').textContent(),demo.sections.about[0].description);
  await page.locator('#section-skills [data-more-section]').click();assert.equal(await page.locator('#section-skills .pp-card').count(),6);
  await page.locator('#section-skills [data-more-section]').click();assert.equal(await page.locator('#section-skills .pp-card').count(),8);
  await page.locator('#section-projects summary').first().click();assert.equal(await page.locator('#section-projects details').first().getAttribute('open'),'');
  // Printable resume includes all cards, not just the initial three.
  await page.evaluate(()=>window.print=()=>window.printRequested=true);await page.locator('[data-resume]').click();assert.equal(await page.evaluate(()=>window.printRequested),true);
  assert.equal(await page.locator('#section-skills .pp-card').count(),8);
  await page.evaluate(()=>{document.querySelectorAll('details').forEach(d=>d.open=false);window.testPublic.renderPublicProfile(document.getElementById('public-preview-root'),window.testData,{preview:true});});
  for(const width of [1920,1440,1280,1024,768,390,320]){
   await page.setViewportSize({width,height:1080});assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth),`overflow at ${width}`);
   await page.screenshot({path:`${out}/public-${width}.png`,fullPage:true});if([1440,390].includes(width))await page.screenshot({path:`${out}/hero-${width}.png`});
   if(width<900){await page.locator('.pp-menu-toggle').click();assert.equal(await page.locator('.pp-menu-toggle').getAttribute('aria-expanded'),'true');await page.locator('.pp-menu a[href="#section-projects"]').click();assert.equal(await page.locator('.pp-menu-toggle').getAttribute('aria-expanded'),'false');await page.evaluate(()=>scrollTo(0,0));}
  }
  await page.evaluate(()=>{
   const p=structuredClone(window.testData);p.name='<img src=x onerror=alert(1)>';p.sections.about[0].description='<script>alert(1)</script>';p.avatarUrl='javascript:alert(1)';p.githubUrl='javascript:alert(1)';p.sections.projects[0].links=[{url:'javascript:alert(1)',title:'bad'}];window.testPublic.renderPublicProfile(document.getElementById('public-preview-root'),p,{preview:true});
  });
  assert.equal(await page.locator('a[href^="javascript:"],img[src^="javascript:"]').count(),0);assert.equal(await page.locator('#preview-name img').count(),0);
  await page.evaluate(()=>window.testPublic.renderPublicProfile(document.getElementById('public-preview-root'),{name:'پروفایل تازه',sections:{}},{preview:true}));
  assert.equal(await page.locator('.pp-section:not([hidden])').count(),0);assert.equal(await page.locator('.pp-cta').count(),0);
  assert.deepEqual(errors,[]);console.log('PASS public UI: seven widths, shared renderer, privacy projection, hidden references, pagination, menu, details, printable resume, empty state and unsafe URLs.');
 }finally{await browser.close();await new Promise(r=>server.close(r));}
})().catch(e=>{console.error(e);process.exitCode=1;});
