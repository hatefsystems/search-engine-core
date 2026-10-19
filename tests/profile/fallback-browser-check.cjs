// Fast real-browser coverage for the head bootstrap, without C++ or MongoDB.
const {chromium}=require('playwright');
const assert=require('node:assert/strict');
const http=require('node:http');
const fs=require('node:fs');
const path=require('node:path');
const publicDir=path.resolve(__dirname,'../../public');
const out='build/profile/public-screenshots';fs.mkdirSync(out,{recursive:true});
const name='نمونه <img src=x onerror="window.injected=true">';
const escape=value=>value.replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;').replaceAll('"','&quot;');
const fixture=mode=>`<!doctype html><html lang="fa" dir="rtl"><head><meta charset="utf-8">
<link rel="stylesheet" href="/assets/css/profile-public.css">
${mode==='late'?'':'<script src="/assets/js/profile-public-bootstrap.js"></script><script>window.bootstrapRanBeforeBody=!document.getElementById("public-profile-root");</script><script type="module" src="/assets/js/profile-public.js" data-public-profile-module></script>'}
</head><body class="public-profile-page">
${mode==='late'?'':`<template id="public-profile-data">${mode==='invalid'?'invalid-json':escape(JSON.stringify({name,sections:{},links:[]}))}</template>`}
<div id="public-profile-root" data-profile-render="loading"><div class="profile-loading" role="status">در حال بارگذاری…</div></div>
<template id="public-profile-fallback"><div class="public-profile"><section class="pp-hero"><h1>${escape(name)}</h1></section></div></template>
</body></html>`;
const server=http.createServer((req,res)=>{
 try{
  const url=new URL(req.url,'http://localhost');res.setHeader('Cache-Control','no-store');
  if(url.pathname==='/fixture'){res.setHeader('Content-Type','text/html; charset=utf-8');return res.end(fixture(url.searchParams.get('mode')));}
  const file=path.resolve(publicDir,'.'+url.pathname);
  if(!file.startsWith(publicDir+path.sep)){res.writeHead(403);return res.end();}
  res.setHeader('Content-Type',({'.js':'application/javascript','.css':'text/css'})[path.extname(file)]||'application/octet-stream');
  res.end(fs.readFileSync(file));
 }catch{res.writeHead(404);res.end();}
});
(async()=>{
 await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
 const base=`http://127.0.0.1:${server.address().port}`;
 let browser,page,release,scenario;const errors=[];
 try{
  browser=await chromium.launch({headless:true,executablePath:process.env.PROFILE_CHROMIUM_EXECUTABLE||undefined,args:['--no-sandbox','--disable-dev-shm-usage']});
  for(const mode of ['abort','invalid','late','delayed']){
   scenario=mode;page=await browser.newPage({viewport:{width:390,height:800}});page.on('pageerror',error=>errors.push(error.message));
   if(mode==='abort')await page.route(url=>url.pathname==='/assets/js/profile-public.js',route=>route.abort());
   if(mode==='delayed'){
    const gate=new Promise(resolve=>release=resolve);
    await page.route(url=>url.pathname==='/assets/js/profile-public.js',async route=>{await gate;await route.continue();});
   }
   await page.goto(`${base}/fixture?mode=${mode}`,{waitUntil:mode==='delayed'?'commit':'domcontentloaded'});
   if(mode==='late')await page.addScriptTag({url:base+'/assets/js/profile-public-bootstrap.js'});
   if(mode==='delayed'){
    await page.locator('#public-profile-root[data-profile-render=loading] [role=status]').waitFor();
    assert.equal(await page.locator('h1').count(),0);
    release();release=null;
    await page.locator('#public-profile-root[data-profile-render=interactive]').waitFor();
    await page.evaluate(()=>document.dispatchEvent(new Event('public-profile-render-failed')));
    assert.equal(await page.locator('#public-profile-root').getAttribute('data-profile-render'),'interactive');
   }else await page.locator('#public-profile-root[data-profile-render=fallback] h1').waitFor();
   if(mode!=='late')assert.equal(await page.evaluate(()=>window.bootstrapRanBeforeBody),true);
   assert.equal(await page.locator('#public-profile-root h1').textContent(),name);
   assert.equal(await page.locator('img').count(),0);
   assert.equal(await page.evaluate(()=>window.injected),undefined);
   assert.equal(await page.locator('.profile-loading').count(),0);
   await page.close();page=null;
  }
  assert.deepEqual(errors,[]);
  console.log('PASS head bootstrap: aborted module, renderer error, late bootstrap, delayed success, safe literal markup and no interactive downgrade.');
 }catch(error){
  if(page){
   const state=await page.evaluate(()=>({ready:document.readyState,render:document.getElementById('public-profile-root')?.dataset.profileRender,sourceType:document.getElementById('public-profile-fallback')?.tagName,html:document.documentElement.outerHTML})).catch(()=>null);
   fs.writeFileSync(`${out}/fallback-error.json`,JSON.stringify({scenario,state,errors,error:String(error)},null,2));
   await page.screenshot({path:`${out}/fallback-error.png`,fullPage:true}).catch(()=>{});
  }
  throw error;
 }finally{release?.();await browser?.close();await new Promise(resolve=>server.close(resolve));}
})().catch(error=>{console.error(error);process.exitCode=1;});
