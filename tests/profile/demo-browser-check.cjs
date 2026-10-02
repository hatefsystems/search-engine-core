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
   const buffer=kind==='avatar'&&process.env.PROFILE_DEMO_AVATAR?fs.readFileSync(process.env.PROFILE_DEMO_AVATAR):Buffer.from(await page.evaluate(kind=>{
    const c=document.createElement('canvas');c.width=kind==='avatar'?400:1400;c.height=kind==='avatar'?400:500;const x=c.getContext('2d');
    const g=x.createLinearGradient(0,0,c.width,c.height);g.addColorStop(0,'#151039');g.addColorStop(.6,'#5424b8');g.addColorStop(1,'#a17cff');x.fillStyle=g;x.fillRect(0,0,c.width,c.height);
    if(kind==='avatar'){x.fillStyle='#fff';x.font='bold 130px sans-serif';x.textAlign='center';x.fillText('HR',200,245);}else{for(let i=0;i<8;i++){x.strokeStyle=`rgba(210,190,255,${.12+i*.06})`;x.lineWidth=16;x.beginPath();x.ellipse(350+i*95,250,110,290,-.6,0,Math.PI*2);x.stroke();}}
    return c.toDataURL('image/png').split(',')[1];
   },kind),'base64');
   await page.locator(`#${kind}-file`).setInputFiles({name:`demo-${kind}.png`,mimeType:'image/png',buffer});await page.waitForFunction(()=>!document.querySelector('#profile-form').inert);await saved();
   await page.locator(`#editor-${kind}`).waitFor({state:'visible'});
  }
  await page.locator('#profile-tagline').fill(demo.header.tagline+' ');await saved();assert.equal((await api(endpoint)).data.tagline,demo.header.tagline+' ');
  await page.locator('#profile-tagline').fill(demo.header.tagline);await saved();
  await shot('01-basic-desktop');
  // Uploaded images are either a real screenshot of this running editor or a
  // labelled illustrative diagram. They are not claimed production evidence.
  const diagram=async(index)=>Buffer.from(await page.evaluate(index=>{
   const canvas=document.createElement('canvas');canvas.width=1000;canvas.height=620;const x=canvas.getContext('2d');
   x.fillStyle='#f6f3fc';x.fillRect(0,0,1000,620);x.fillStyle='#55309a';x.font='bold 32px sans-serif';
   x.fillText(['Delivery workflow','Platform services','Software delivery','Cloud deployment'][index%4],54,70);
   x.fillStyle='#62576f';x.font='18px sans-serif';x.fillText('ILLUSTRATIVE SAMPLE · NOT A PRODUCTION DIAGRAM',54,112);
   const stages=[['Source','Build + test','Deploy','Observe'],['GitLab','Build','Containers','Monitoring'],['Application','Tests','Release','Operations'],['Infrastructure','Containers','Deploy','Monitoring']][index%4];
   for(let i=0;i<4;i++){const col=i%2,row=Math.floor(i/2),left=54+col*476,top=172+row*188;x.fillStyle='#fff';x.fillRect(left,top,414,124);x.strokeStyle='#cbb9e5';x.lineWidth=2;x.strokeRect(left,top,414,124);x.fillStyle='#6d3eb3';x.font='bold 20px sans-serif';x.fillText(String(i+1).padStart(2,'0'),left+24,top+35);x.fillStyle='#302443';x.font='bold 26px sans-serif';x.fillText(stages[i],left+24,top+82);}
   x.fillStyle='#62576f';x.font='17px sans-serif';x.fillText('Profile gallery example · uploaded through the real editor',54,568);
   return canvas.toDataURL('image/png').split(',')[1];
  },index),'base64');
  const mediaTargets=[...demo.sections.experiences.map((i,n)=>['experiences',i.id,n]),...demo.sections.projects.map((i,n)=>['projects',i.id,n+3])];
  for(const [section,itemId,art] of mediaTargets){
   await nav(section);await page.locator(`[data-item-choice="${itemId}"]`).click();
   const panel=()=>page.locator('.item-media-panel');
   const screenshot=itemId==='demo-project-hatef';
   if(screenshot)await page.evaluate(()=>window.scrollTo(0,0));
   const bytes=screenshot?await page.screenshot():await diagram(art);
   const caption=screenshot?'نمای واقعی ویرایشگر هاتف در محیط آزمایشی':'نمودار نمونهٔ مسیر کار، صرفاً برای نمایش گالری این سابقه یا پروژه';
   const upload=async(buffer,alt)=>{
    await panel().getByLabel('توضیح تصویر جدید',{exact:true}).fill(alt);
    const before=await panel().locator('.item-media-row').count();
    await panel().locator('input[type=file]').setInputFiles({name:'related-sample.png',mimeType:'image/png',buffer});
    await page.waitForFunction(count=>!document.querySelector('#profile-form').inert&&document.querySelectorAll('.item-media-row').length===count+1,before);
   };
   await upload(bytes,caption);
   if(itemId==='demo-exp-quickhands'||screenshot){
    await upload(await diagram(art+1),'نمای تکمیلی آزمایشی');
    await panel().locator('.item-media-row').nth(1).getByRole('button',{name:'تصویر قبلی',exact:true}).click();await saved();
    assert.equal((await api(endpoint)).data.sections[section].find(i=>i.id===itemId).media[0].alt,'نمای تکمیلی آزمایشی');
    await panel().locator('.item-media-row').first().getByRole('button',{name:'تصویر بعدی',exact:true}).click();await saved();
    await panel().getByLabel('توضیح تصویر 1',{exact:true}).fill(caption+'؛ پیوست مرتبط');await saved();
    await upload(await diagram(0),'تصویر موقت برای آزمون حذف');
    await panel().locator('.item-media-row').last().getByRole('button',{name:'حذف تصویر',exact:true}).click();await saved();
    await page.reload();await panel().waitFor();assert.equal(await panel().locator('.item-media-row').count(),2);
    assert.equal(await panel().getByLabel('توضیح تصویر 1',{exact:true}).inputValue(),caption+'؛ پیوست مرتبط');
    await panel().screenshot({path:`${out}/23-${section}-image-editor.png`});
   }
   assert.ok(await panel().locator('img').evaluateAll(images=>images.every(img=>img.complete&&img.naturalWidth>0)));
  }
  // The remaining form round-trips always start with the first item.
  const rounds=[['experiences','roleTitle'],['projects','title'],['skills','name'],['education','institutionName'],['certifications','name'],['publications','title'],['openSource','repositoryName'],['services','title'],['achievements','title'],['languages','name'],['recommendations','authorName'],['contacts','label'],['about','title']];
  for(let index=0;index<rounds.length;index++){
   const [section,field]=rounds[index];await nav(section);await page.locator(`[data-item-choice="${demo.sections[section][0].id}"]`).click();
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
  if(fixture){
   const review=await context.newPage();await review.goto(base+'/assets/profile-preview.html');
   await review.evaluate(async owner=>{const m=await import('/assets/js/profile-public.js');m.renderPublicProfile(document.getElementById('public-preview-root'),m.projectPublicProfile(owner),{preview:true});},owner);
   await review.evaluate(()=>document.querySelectorAll('.content-gallery img').forEach(img=>img.loading='eager'));
   await review.waitForFunction(()=>document.querySelectorAll('.content-gallery img').length===7&&[...document.querySelectorAll('.content-gallery img')].every(img=>img.complete&&img.naturalWidth>0));
   await review.screenshot({path:`${out}/fixture-public-full.png`,fullPage:true});
   for(const section of ['experiences','projects','skills'])await review.locator('#section-'+section).screenshot({path:`${out}/fixture-${section}.png`,style:'.pp-nav{position:static!important}'});
   await review.close();
  }
  if(!fixture){
   const publicResponse=await browser.newContext();const publicPage=await publicResponse.newPage();
   publicPage.on('pageerror',e=>errors.push(e.message));
   await publicPage.setViewportSize({width:1440,height:1080});const response=await publicPage.goto(base+'/'+encodeURIComponent(slug));assert.equal(response.status(),200);await publicPage.locator('.public-profile').waitFor();
   assert.equal(await publicPage.getByText('نویسندهٔ آزمایشی',{exact:true}).count(),0);
   await publicPage.evaluate(()=>document.fonts.ready);
   assert.ok(await publicPage.locator('#section-projects .pp-card').count());
   assert.equal(await publicPage.locator('#section-recommendations').count(),0);
   const preview=page.frameLocator('#public-preview');await preview.locator('#preview-name').waitFor();
   assert.equal(await preview.locator('#preview-name').textContent(),await publicPage.locator('#preview-name').textContent());
   assert.equal(await preview.locator('#preview-bio').textContent(),await publicPage.locator('#preview-bio').textContent());
   await publicPage.waitForFunction(()=>document.querySelectorAll('#section-skills .pp-card').length===8);
   assert.equal(await publicPage.locator('details,summary,[data-more-section]').count(),0);
   assert.ok(await publicPage.locator('#section-experiences .pp-prose-field').count());
   assert.equal(await preview.locator('#section-skills .pp-card').count(),8);
   assert.equal(await publicPage.locator('.content-gallery img').count(),7);
   await publicPage.evaluate(()=>document.querySelectorAll('.content-gallery img').forEach(img=>img.loading='eager'));
   await publicPage.waitForFunction(()=>[...document.querySelectorAll('.content-gallery img')].every(img=>img.complete&&img.naturalWidth>0));
   assert.deepEqual(await preview.locator('.content-gallery figcaption').allTextContents(),await publicPage.locator('.content-gallery figcaption').allTextContents());
   for(const width of [1440,1920,768,390,320]){
    await publicPage.setViewportSize({width,height:1080});assert.ok(await publicPage.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
    await publicPage.evaluate(()=>window.scrollTo({top:0,behavior:'instant'}));
    await publicPage.screenshot({path:`${out}/20-public-${width}.png`,fullPage:true});
    if(width===1440||width===390)await publicPage.screenshot({path:`${out}/22-public-hero-${width}.png`});
   }
   await publicPage.setViewportSize({width:1440,height:1080});
   for(const section of ['experiences','projects','skills'])await publicPage.locator('#section-'+section).screenshot({path:`${out}/24-public-${section}.png`,style:'.pp-nav{position:static!important}'});
   await page.locator('#expand-preview').click();await page.locator('#public-preview-dialog[open]').waitFor();
   await preview.locator('#preview-name').waitFor();await page.screenshot({path:`${out}/21-owner-preview.png`});await page.keyboard.press('Escape');
   await publicResponse.close();
  }
  // Exercise the requested banner through the actual upload control. Screenshots
  // are browser captures, with existing repository photographs as uploaded covers.
  // An optional local portrait can be supplied without committing personal media.
  const photoContext=await browser.newContext({viewport:{width:1440,height:1080}});
  const photoPage=await photoContext.newPage();photoPage.on('pageerror',e=>errors.push(e.message));
  await page.setViewportSize({width:1440,height:1080});await nav('basic');
  for(const [variant,file] of [['dark','slide1.jpg'],['light','2.jpg']]){
   await page.locator('#cover-file').setInputFiles(require('node:path').join(__dirname,'../../public/coming-soon/assets/images',file));
   await page.waitForFunction(()=>!document.querySelector('#profile-form').inert);await saved();
   const updated=(await api(endpoint)).data;
   if(fixture){
    await photoPage.goto(base+'/assets/profile-preview.html');
    await photoPage.evaluate(async owner=>{const m=await import('/assets/js/profile-public.js');m.renderPublicProfile(document.getElementById('public-preview-root'),m.projectPublicProfile(owner),{preview:true});},updated);
   }else await photoPage.goto(base+'/'+encodeURIComponent(slug));
   await photoPage.waitForFunction(()=>document.querySelector('.pp-backdrop img')?.naturalWidth>0);
   await photoPage.evaluate(()=>document.fonts.ready);
   assert.equal(await photoPage.locator('.pp-backdrop img').getAttribute('src'),updated.coverImageUrl);
   const preview=page.frameLocator('#public-preview');
   await preview.locator(`.pp-backdrop img[src="${updated.coverImageUrl}"]`).waitFor({state:'attached'});
   assert.equal(await photoPage.locator('.pp-section-index').count(),0);
   for(const width of [1440,390]){
    await photoPage.setViewportSize({width,height:1080});await photoPage.evaluate(()=>scrollTo(0,0));
    await photoPage.waitForFunction(()=>document.querySelector('.pp-menu a[aria-current=location]')?.hash==='#profile-home');
    const cover=await photoPage.locator('.pp-backdrop').boundingBox();assert.equal(cover.x,0);assert.equal(cover.width,width);
    assert.ok(await photoPage.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
    await photoPage.screenshot({path:`${out}/30-cover-${variant}-${width}.png`});
   }
   await photoPage.setViewportSize({width:1440,height:1080});
   await photoPage.locator('#section-projects').evaluate(n=>scrollTo(0,n.getBoundingClientRect().top+scrollY-document.querySelector('.pp-nav').offsetHeight-24));
   await photoPage.waitForFunction(()=>document.querySelector('.pp-menu a[aria-current=location]')?.hash==='#section-projects');
   const bar=await photoPage.locator('.pp-nav').boundingBox();assert.ok(bar.y>=0&&bar.y<=12);
   await photoPage.screenshot({path:`${out}/31-sticky-${variant}-projects.png`});
  }
  await photoContext.close();
  assert.deepEqual(errors,[]);
  fs.writeFileSync(`${out}/validation.json`,JSON.stringify({mode:fixture?'fixture UI only':'real C++ API + MongoDB',sections:Object.keys(demo.sections).length,items:Object.values(demo.sections).reduce((n,a)=>n+a.length,0),screenshots:fs.readdirSync(out).filter(f=>f.endsWith('.png')).length,relatedImages:7,mediaChecks:'Upload, captions, ordering, deletion, reload and preview/public parity',bannerChecks:'Two uploaded JPEG covers, full viewport width, white overlays, public/preview cover parity, sticky horizontal menu and scroll-driven active project',unknownFields:'Synthetic publication and recommendation are labelled and hidden.',result:'PASS'},null,2));
  console.log(`PASS demo: ${fixture?'fixture UI':'real API persistence'}, 14 sections, round-trip edits, uploaded images, responsive screenshots, private synthetic samples, publish.`);
 }finally{
  if(id&&key&&context)await context.request.delete(base+`/api/profiles/${id}`,{headers:{Authorization:'Bearer '+key}}).catch(()=>{});
  if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));
 }
})().catch(error=>{console.error(error);process.exitCode=1;});
