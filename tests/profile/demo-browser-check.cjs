// Creates a disposable professional profile, verifies the real editor, takes
// screenshots, then deletes the profile. --fixture is explicitly not an API test.
const {chromium}=require('playwright');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const demo=require('./demo-profile.cjs');
(async()=>{
 let server;const fixture=process.argv.includes('--fixture');
 if(fixture){const f=require('./workspace-fixture.cjs');f.seedEmpty('demo-profile');server=f.start(0);await new Promise(r=>server.listening?r():server.once('listening',r));}
 const base=fixture?`http://127.0.0.1:${server.address().port}`:process.env.PROFILE_TEST_BASE_URL;
 assert(base,'Set an isolated PROFILE_TEST_BASE_URL, or explicitly use --fixture.');
 const out='build/profile/demo-screenshots';fs.mkdirSync(out,{recursive:true});
 let browser,context,id,key;const errors=[];
 try{
  browser=await chromium.launch({headless:true,executablePath:process.env.PROFILE_CHROMIUM_EXECUTABLE||undefined,args:['--no-sandbox','--disable-dev-shm-usage']});
  context=await browser.newContext({viewport:{width:1440,height:1080}});const page=await context.newPage();page.on('pageerror',e=>errors.push(e.message));
  if(!fixture){
   const shell=await context.request.get(base+'/assets/profile-preview.html');
   assert.equal(shell.status(),200);assert.equal(shell.headers()['x-frame-options'],'SAMEORIGIN');
   assert.equal(shell.headers()['content-security-policy'],"frame-ancestors 'self'");
   const stylesheet=await context.request.get(base+'/assets/css/profile-public.css');
   assert.equal(stylesheet.status(),200);assert.equal(stylesheet.headers()['x-frame-options'],'DENY');
  }
  const api=async(path,method='GET',body)=>{const response=await context.request.fetch(base+path,{method,data:body,headers:{Origin:new URL(base).origin,...(key?{Authorization:'Bearer '+key}:{})}});const result=await response.json();assert.ok(response.ok(),`${method} ${path}: ${response.status()} ${JSON.stringify(result)}`);return result;};
  const slug='نمونه.هاتف.'+Date.now();
  if(fixture)id='demo-profile';else{const result=await api('/api/profiles','POST',{type:'PERSON',slug,name:'',isPublic:false});id=result.data.id;key=result.ownerToken;await api(`/api/profiles/${id}/session`,'POST',{key});}
  const endpoint=`/api/profiles/${id}`;let state=(await api(endpoint)).data;
  const schema=(await api(endpoint+'/content-schema')).data.sections;
  state=(await api(endpoint,'PUT',{version:state.version,...demo.header})).data;
  for(const [section,items] of Object.entries(demo.sections))for(const item of items){
   const defaults=schema.find(s=>s.key===section).defaults;
   state=(await api(endpoint+'/content/'+section,'POST',{version:state.version,item:{...defaults,...item}})).data;
  }
  state=(await api(endpoint+'/layout','PUT',{version:state.version,goal:'PORTFOLIO',featured:[{section:'experiences',id:'demo-exp-quickhands'},{section:'projects',id:'demo-project-hatef'}],order:['experiences','projects','skills','certifications','education','openSource','services','achievements','languages','contacts','availability','about','publications','recommendations']})).data;
  for(const [index,link] of demo.links.entries())await api(endpoint+'/links','POST',{id:(index+1).toString(16).padStart(24,'0'),...link});
  const editorUrl=fixture?base+'/?id='+id:base+'/profiles/'+encodeURIComponent(slug)+'/edit';
  await page.goto(editorUrl);await page.locator('#editor-section-nav button').first().waitFor();
  const nav=section=>page.locator(`#editor-section-nav [data-section="${section}"]`).click();
  const saved=()=>page.waitForFunction(()=>document.querySelector('#save-status').textContent==='ذخیره شد');
  const shot=async(name,fullPage=false)=>{await page.evaluate(()=>document.fonts.ready);await page.evaluate(()=>window.scrollTo(0,0));await page.screenshot({path:`${out}/${name}.png`,fullPage});};
  // Uploaded test artwork is generated in the browser. It is not a portrait or
  // certification badge and does not assert anyone's identity or verification.
  await nav('basic');
  await page.locator('#profile-bio').fill(demo.sections.about[0].description+' ');await saved();assert.equal((await api(endpoint)).data.sections.about[0].description,demo.sections.about[0].description+' ');await page.locator('#profile-bio').fill(demo.sections.about[0].description);await saved();
  await page.locator('#profile-availability').selectOption('BUSY');await saved();assert.equal((await api(endpoint)).data.sections.availability[0].status,'BUSY');await page.locator('#profile-availability').selectOption('AVAILABLE');await saved();
  for(const kind of ['avatar','cover']){
   const buffer=Buffer.from(await page.evaluate(kind=>{
    const c=document.createElement('canvas');c.width=kind==='avatar'?400:1400;c.height=kind==='avatar'?400:500;const x=c.getContext('2d');
    const g=x.createLinearGradient(0,0,c.width,c.height);g.addColorStop(0,'#151039');g.addColorStop(.6,'#5424b8');g.addColorStop(1,'#a17cff');x.fillStyle=g;x.fillRect(0,0,c.width,c.height);
    if(kind==='avatar'){x.fillStyle='#fff';x.font='bold 130px sans-serif';x.textAlign='center';x.fillText('HR',200,245);}else{for(let i=0;i<8;i++){x.strokeStyle=`rgba(210,190,255,${.12+i*.06})`;x.lineWidth=16;x.beginPath();x.ellipse(350+i*95,250,110,290,-.6,0,Math.PI*2);x.stroke();}}
    return c.toDataURL('image/png').split(',')[1];
   },kind),'base64');
   await page.locator(`#${kind}-file`).setInputFiles({name:`demo-${kind}.png`,mimeType:'image/png',buffer});await page.waitForFunction(()=>!document.querySelector('#profile-form').inert);await saved();
   await page.locator(`#editor-${kind}`).waitFor({state:'visible'});
  }
  await shot('01-basic-desktop');
  const rounds=[['experiences','roleTitle'],['projects','title'],['skills','name'],['education','institutionName'],['certifications','name'],['publications','title'],['openSource','repositoryName'],['services','title'],['achievements','title'],['languages','name'],['recommendations','authorName'],['contacts','label'],['about','title']];
  for(let index=0;index<rounds.length;index++){
   const [section,field]=rounds[index];await nav(section);
   const expected=demo.sections[section][0][field];const input=page.locator(`.content-item-form [data-field="${field}"] input`);
   await input.fill(expected+' ');await saved();assert.equal((await api(endpoint)).data.sections[section][0][field],expected+' ');
   await input.fill(expected);await saved();await page.reload();await input.waitFor();assert.equal(await input.inputValue(),expected);
   assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
   await shot(`${String(index+2).padStart(2,'0')}-${section}-desktop`);
  }
  await nav('availability');await page.locator('[data-field=status] select').selectOption('BUSY');await saved();assert.equal((await api(endpoint)).data.sections.availability[0].status,'BUSY');await page.locator('[data-field=status] select').selectOption('AVAILABLE');await saved();await shot('15-availability-desktop');
  await nav('links');await page.getByLabel('عنوان لینک',{exact:true}).fill('کد منبع هاتف');await saved();await shot('16-links-desktop');
  for(const [width,name] of [[1920,'wide'],[768,'tablet'],[390,'mobile']]){
   await page.setViewportSize({width,height:1080});await nav('basic');await shot(`17-basic-${name}`,width<1000);await nav('experiences');await shot(`18-experience-${name}`,width<1000);
   assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
   if(width===390){await page.locator('#preview-tab').click();await shot('19-preview-mobile',true);await page.locator('#form-tab').click();}
  }
  await page.setViewportSize({width:1440,height:1080});await nav('basic');await page.locator('#publish').click();await page.waitForFunction(()=>document.querySelector('#publication-label').textContent==='صفحهٔ منتشرشده');
  const owner=(await api(endpoint)).data;
  for(const section of Object.keys(demo.sections))assert.equal(owner.sections[section].length,demo.sections[section].length);
  if(!fixture){
   const publicResponse=await browser.newContext();const publicPage=await publicResponse.newPage();
   await publicPage.setViewportSize({width:1440,height:1080});const response=await publicPage.goto(base+'/'+encodeURIComponent(slug));assert.equal(response.status(),200);await publicPage.locator('.public-profile').waitFor();
   assert.equal(await publicPage.getByText('نویسندهٔ آزمایشی',{exact:true}).count(),0);
   await publicPage.evaluate(()=>document.fonts.ready);
   assert.ok(await publicPage.locator('#section-projects .pp-card').count());
   assert.equal(await publicPage.locator('#section-recommendations').count(),0);
   const preview=page.frameLocator('#public-preview');await preview.locator('#preview-name').waitFor();
   assert.equal(await preview.locator('#preview-name').textContent(),await publicPage.locator('#preview-name').textContent());
   assert.equal(await preview.locator('#preview-bio').textContent(),await publicPage.locator('#preview-bio').textContent());
   await publicPage.locator('#section-skills [data-more-section]').click();
   assert.equal(await publicPage.locator('#section-skills .pp-card').count(),6);
   await publicPage.locator('#section-skills [data-more-section]').click();
   assert.equal(await publicPage.locator('#section-skills .pp-card').count(),8);
   for(const width of [1440,1920,768,390,320]){
    await publicPage.setViewportSize({width,height:1080});assert.ok(await publicPage.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
    await publicPage.screenshot({path:`${out}/20-public-${width}.png`,fullPage:true});
    if(width===1440||width===390)await publicPage.screenshot({path:`${out}/22-public-hero-${width}.png`});
   }
   await publicPage.setViewportSize({width:1440,height:1080});
   await page.locator('#expand-preview').click();await page.locator('#public-preview-dialog[open]').waitFor();
   await preview.locator('#preview-name').waitFor();await page.screenshot({path:`${out}/21-owner-preview.png`});await page.keyboard.press('Escape');
   await publicResponse.close();
  }
  assert.deepEqual(errors,[]);
  fs.writeFileSync(`${out}/validation.json`,JSON.stringify({mode:fixture?'fixture UI only':'real C++ API + MongoDB',sections:Object.keys(demo.sections).length,items:Object.values(demo.sections).reduce((n,a)=>n+a.length,0),screenshots:fs.readdirSync(out).filter(f=>f.endsWith('.png')).length,unknownFields:'Synthetic publication and recommendation are labelled and hidden.',result:'PASS'},null,2));
  console.log(`PASS demo: ${fixture?'fixture UI':'real API persistence'}, 14 sections, round-trip edits, uploaded images, responsive screenshots, private synthetic samples, publish.`);
 }finally{
  if(id&&key&&context)await context.request.delete(base+`/api/profiles/${id}`,{headers:{Authorization:'Bearer '+key}}).catch(()=>{});
  if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));
 }
})().catch(error=>{console.error(error);process.exitCode=1;});
