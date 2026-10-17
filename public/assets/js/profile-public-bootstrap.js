// Installed before the module request so network/import failures also recover.
// The noscript body is server-escaped public HTML, never owner-supplied markup.
(()=>{
 let failed=false;
 let moduleLoaded=false;
 const root=document.getElementById('public-profile-root');
 const source=document.getElementById('public-profile-fallback');
 const fallback=()=>{
  if(!root||!source||root.dataset.profileRender==='interactive')return;
  root.replaceChildren(...Array.from(source.childNodes,n=>document.importNode(n,true)));
  root.dataset.profileRender='fallback';
 };
 const recover=()=>{failed=true;if(document.readyState!=='loading')fallback();};
 const scheduleFallback=()=>{
  window.setTimeout(()=>{
   if(root?.dataset.profileRender==='interactive'||moduleLoaded||failed)return;
   fallback();
  },2000);
 };
 const moduleScript=document.querySelector('script[data-public-profile-module]');
 if(moduleScript){
  moduleScript.addEventListener('load',()=>{moduleLoaded=true;},{once:true});
  moduleScript.addEventListener('error',recover,{once:true});
 }
 document.addEventListener('error',event=>{
  const target=event.target;
  if(target instanceof HTMLScriptElement&&target.matches?.('script[data-public-profile-module]'))recover();
 },true);
 document.addEventListener('public-profile-render-failed',recover);
 window.addEventListener('error',event=>{if(event.filename?.includes('/assets/js/profile-public.js'))recover();});
 document.addEventListener('DOMContentLoaded',()=>{
  if(failed||!document.getElementById('public-profile-data'))fallback();
  else scheduleFallback();
 },{once:true});
 window.addEventListener('load',()=>{if(!moduleLoaded&&!failed)fallback();},{once:true});
})();
