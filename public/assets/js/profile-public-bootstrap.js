// Runs in the head: body nodes must be resolved only when recovery is needed.
// Clone parsed, server-escaped template content; never reinterpret DOM text as HTML.
(()=>{
 let failed=false;
 let timer;
 const fallback=()=>{
  const root=document.getElementById('public-profile-root');
  const source=document.getElementById('public-profile-fallback');
  if(!root||!(source instanceof HTMLTemplateElement)||root.dataset.profileRender==='interactive'||root.dataset.profileRender==='fallback')return;
  root.replaceChildren(source.content.cloneNode(true));
  root.dataset.profileRender='fallback';
  window.clearTimeout(timer);
 };
 const recover=()=>{failed=true;if(document.readyState!=='loading')fallback();};
 document.addEventListener('error',event=>{
  const target=event.target;
  if(target instanceof HTMLScriptElement&&target.matches?.('script[data-public-profile-module]'))recover();
 },true);
 document.addEventListener('public-profile-render-failed',recover);
 window.addEventListener('error',event=>{if(event.filename?.includes('/assets/js/profile-public.js'))recover();});
 const ready=()=>{
  if(failed||!document.getElementById('public-profile-data'))fallback();
  else timer=window.setTimeout(fallback,2000);
 };
 if(document.readyState==='loading')document.addEventListener('DOMContentLoaded',ready,{once:true});
 else ready();
 window.addEventListener('load',fallback,{once:true});
})();
