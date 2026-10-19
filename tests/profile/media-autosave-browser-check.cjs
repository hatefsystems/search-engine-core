// Real editor modules; fixture API by default, or an explicitly configured isolated backend.
const {chromium}=require('playwright');
const assert=require('node:assert/strict');
const {start}=require('./workspace-fixture.cjs');
(async()=>{
 const server=process.env.PROFILE_TEST_BASE_URL?null:start(0);
 if(server)await new Promise(resolve=>server.listening?resolve():server.once('listening',resolve));
 const base=process.env.PROFILE_TEST_BASE_URL||`http://127.0.0.1:${server.address().port}`;
 const browser=await chromium.launch({headless:true,executablePath:process.env.PROFILE_CHROMIUM_EXECUTABLE||undefined,args:['--no-sandbox']});
 const context=await browser.newContext({viewport:{width:1440,height:1050}}),page=await context.newPage();
 let id='fixture',key,editorURL=base,endpoint='/api/profiles/fixture';
 try {
 if(!server){
  const slug='آزمون.ذخیره.تصویر.'+Date.now();
  const response=await context.request.post(base+'/api/profiles',{headers:{Origin:base},data:{type:'PERSON',slug,name:'ه',isPublic:false}});
  assert.equal(response.status(),201);const created=await response.json();id=created.data.id;key=created.ownerToken;endpoint='/api/profiles/'+id;
  editorURL=base+'/profiles/'+encodeURIComponent(slug)+'/edit';
  let version=created.data.version;
  for(const [section,item] of [['certifications',{id:'certifications-one',name:'AWS Solutions Architect',visibility:'PUBLIC'}],['projects',{id:'projects-one',title:'پروژه',visibility:'PUBLIC'}]]){
   const result=await context.request.post(`${base}/api/profiles/${id}/content/${section}`,{headers:{Origin:base,Authorization:'Bearer '+key},data:{version,item}});
   assert.equal(result.status(),200);version=(await result.json()).data.version;
  }
 }
 endpoint='/api/profiles/'+id;
 const errors=[],requests=[];page.on('pageerror',e=>errors.push(e.message));page.on('request',r=>{if(r.method()!=='GET')requests.push(r.url());});
 const nav=section=>page.locator(`#editor-section-nav [data-section="${section}"]`).click();
 const data=async()=>(await (await context.request.get(base+endpoint)).json()).data;
 const png=Buffer.from('iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+j5l8AAAAASUVORK5CYII=','base64');
 const upload=async kind=>{
  const response=page.waitForResponse(r=>r.url().endsWith('/'+kind)&&r.request().method()==='POST');
  await page.locator(`#${kind}-file`).setInputFiles({name:'image.png',mimeType:'image/png',buffer:png});
  assert.equal((await response).status(),200);
  await page.waitForFunction(kind=>{const image=document.getElementById('editor-'+kind);return !image.hidden&&image.complete&&image.naturalWidth>0&&!document.getElementById(kind+'-file').closest('.media-editor').hasAttribute('aria-busy');},kind);
 };
  await page.goto(editorURL);await nav('certifications');
  await page.locator('[data-field=name] input').fill('');
  await page.locator('.item-save-error:not([hidden])').waitFor();
  assert.equal(await page.locator('[data-field=name] input').getAttribute('aria-invalid'),'true');
  assert.equal((await data()).sections.certifications[0].name,'AWS Solutions Architect');
  await nav('projects');await page.locator('[data-field=title] input').fill('پروژهٔ سالم');
  await page.waitForFunction(()=>!JSON.stringify(localStorage).includes('پروژهٔ سالم'));
  assert.equal((await data()).sections.projects[0].title,'پروژهٔ سالم');
  const failedRequests=requests.filter(p=>p.includes('/content/certifications/')).length;
  await nav('basic');const titleSaved=page.waitForResponse(r=>r.url().endsWith(endpoint)&&r.request().method()==='PUT');
  await page.locator('#profile-title').fill('عنوان سالم');
  for(const kind of ['avatar','cover'])await upload(kind);
  await titleSaved;assert.equal((await data()).title,'عنوان سالم');
  assert.ok((await data()).avatarUrl);assert.ok((await data()).coverImageUrl);
  assert.equal(requests.filter(p=>p.includes('/content/certifications/')).length,failedRequests);
  assert.match(await page.locator('#save-status').textContent(),/ذخیره‌نشده/);
  assert.equal(await page.locator('#save-status').getAttribute('data-saved'),'false');
  await page.reload();await nav('certifications');
  await page.locator('.item-save-error:not([hidden])').waitFor();
  assert.equal(await page.locator('[data-field=name] input').inputValue(),'');
  assert.equal(await page.locator('#conflict').isVisible(),false);
  await page.locator('#publish').click();
  assert.equal((await data()).isPublic,false);assert.equal(await page.locator('.item-save-error').isVisible(),true);
  await nav('basic');
  const before=await data();await upload('avatar');
  assert.notEqual((await data()).avatarUrl,before.avatarUrl);
  assert.equal((await data()).coverImageUrl,before.coverImageUrl);
  // Failed upload leaves the last acknowledged image intact.
  await page.route('**'+endpoint+'/avatar',route=>route.fulfill({status:400,contentType:'application/json',body:JSON.stringify({error:{message:'تصویر نامعتبر است.'}})}));
  const previous=(await data()).avatarUrl;
  await page.locator('#avatar-file').setInputFiles({name:'bad.png',mimeType:'image/png',buffer:Buffer.from('invalid')});
  await page.locator('#notice').filter({hasText:'تصویر نامعتبر است.'}).waitFor();
  assert.equal((await data()).avatarUrl,previous);
  await page.unroute('**'+endpoint+'/avatar');
  await page.locator('[data-clear=avatarUrl]').click();
  await page.waitForFunction(()=>document.getElementById('editor-avatar').hidden&&!document.getElementById('avatar-file').closest('.media-editor').hasAttribute('aria-busy'));
  assert.equal((await data()).avatarUrl||'','');
  // Broken URLs show a usable fallback; clearing removes the broken-image warning.
  const state=await data();
  await page.route('**'+state.coverImageUrl,route=>route.fulfill({status:404,contentType:'text/plain',body:'missing image'}));
  await page.reload();await page.locator('.cover-thumbnail .image-load-error:not([hidden])').waitFor();
  assert.equal(await page.locator('#conflict').isVisible(),false);
  await page.locator('[data-clear=coverImageUrl]').click();
  await page.waitForFunction(()=>document.querySelector('.cover-thumbnail .image-load-error').hidden);
  await nav('certifications');await page.locator('[data-field=name] input').fill('گواهی‌نامهٔ اصلاح‌شده');
  await page.waitForFunction(()=>document.getElementById('save-status').dataset.saved==='true');
  assert.equal(await page.locator('.item-save-error:not([hidden])').count(),0);
  assert.equal((await data()).sections.certifications[0].name,'گواهی‌نامهٔ اصلاح‌شده');
  assert.deepEqual(errors,[]);
  console.log('Passed: isolated item errors, independent avatar/cover/basic saves, partial media responses, draft reload, publish guard, replace/delete, failed upload, missing-image fallback.');
 }finally{
  if(key)await context.request.delete(base+endpoint,{headers:{Origin:base,Authorization:'Bearer '+key}});
  await browser.close();if(server)await new Promise(resolve=>server.close(resolve));
 }
})().catch(error=>{console.error(error);process.exitCode=1;});
