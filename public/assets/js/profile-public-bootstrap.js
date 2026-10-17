// Installed before the module request so network/import failures also recover.
// The noscript body is server-escaped public HTML, never owner-supplied markup.
(()=>{
 let failed=false;
 const fallback=()=>{
  const root=document.getElementById('public-profile-root'),source=document.getElementById('public-profile-fallback');
  if(!root||!source||root.dataset.profileRender==='interactive')return;
  root.replaceChildren(...Array.from(source.childNodes,n=>document.importNode(n,true)));
  root.dataset.profileRender='fallback';
 };
 const recover=()=>{failed=true;if(document.readyState!=='loading')fallback();};
 document.addEventListener('error',event=>{if(event.target.matches?.('script[data-public-profile-module]'))recover();},true);
 document.addEventListener('public-profile-render-failed',recover);
 window.addEventListener('error',event=>{if(event.filename?.includes('/assets/js/profile-public.js'))recover();});
 document.addEventListener('DOMContentLoaded',()=>{if(failed||!document.getElementById('public-profile-data'))fallback();},{once:true});
})();
