import {renderPublicProfile} from './profile-public.js';
window.addEventListener('message',event=>{
 if(event.origin!==location.origin||event.source!==parent||event.data?.type!=='hatef-profile-preview')return;
 renderPublicProfile(document.getElementById('public-preview-root'),event.data.profile,{preview:true});
});
parent.postMessage({type:'hatef-profile-preview-ready'},location.origin);
