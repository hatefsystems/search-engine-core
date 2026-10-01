import {el, safeLink, renderItem, sectionLabels, titleFields, enumLabels, dateText} from './profile-content-ui.js';

const icons={brand:'m3 8 8-6 2 3-2 9-8 3Zm11 2 7-6v13l-8 5Z',home:'M3 10 12 3l9 7v11h-6v-7H9v7H3Z',menu:'M4 6h16M4 12h16M4 18h16',arrow:'M19 12H5m6-6-6 6 6 6',send:'m22 2-7 20-4-9-9-4Zm0 0L11 13',download:'M12 3v12m-5-5 5 5 5-5M4 16v5h16v-5',location:'M20 10c0 6-8 12-8 12S4 16 4 10a8 8 0 1 1 16 0ZM9 10a3 3 0 1 0 6 0 3 3 0 1 0-6 0',layers:'m12 2 10 6-10 6L2 8Zm-10 11 10 6 10-6M2 18l10 6 10-6',projects:'M3 5h6l2 3h10v13H3Z',experiences:'M3 7h18v14H3ZM8 7V3h8v4M3 12h18M10 11v3h4v-3',skills:'m12 2 10 10-10 10L2 12Z',education:'m2 8 10-5 10 5-10 5Zm4 3v7q6 5 12 0v-7M22 8v9',certifications:'m12 2 3 2 4 1 1 4 2 3-2 3-1 4-4 1-3 2-3-2-4-1-1-4-2-3 2-3 1-4 4-1Zm-4 10 3 3 5-6',publications:'M5 2h10l5 5v15H5ZM14 2v6h6M8 12h9M8 16h9',openSource:'m8 5-6 7 6 7m8-14 6 7-6 7M14 2l-4 20',services:'M12 2 3 7v10l9 5 9-5V7Zm-9 5 9 5 9-5M12 12v10',achievements:'M7 2h10v9a5 5 0 0 1-10 0ZM7 5H2v4q0 5 5 5M17 5h5v4q0 5-5 5M12 16v6M7 22h10',languages:'M12 2a10 10 0 1 0 0 20 10 10 0 1 0 0-20ZM2 12h20M12 2q-8 10 0 20M12 2q8 10 0 20',recommendations:'M3 3h18v14H9l-6 5ZM7 8h10M7 12h7',contacts:'M2 4h20v16H2Zm0 0 10 9L22 4',availability:'M8 12a4 4 0 1 0 0-8 4 4 0 1 0 0 8ZM1 22v-3q0-6 7-6t7 6v3M16 4q7 0 7 5t-7 5M18 16q5 0 5 6',about:'M12 3v1M12 8v13M3 12a9 9 0 1 0 18 0 9 9 0 1 0-18 0',external:'M14 3h7v7m0-7L10 14M10 3H3v18h18v-7'};
function icon(name){const svg=document.createElementNS('http://www.w3.org/2000/svg','svg');svg.setAttribute('viewBox','0 0 24 24');svg.setAttribute('aria-hidden','true');svg.classList.add('pp-icon');const p=document.createElementNS(svg.namespaceURI,'path');p.setAttribute('d',icons[name]||icons.layers);svg.append(p);return svg;}
const labels={...sectionLabels,experiences:'سوابق کاری',projects:'پروژه‌های منتخب',publications:'مقالات و نوشته‌ها',skills:'مهارت‌های کلیدی'};
const headerFields=['id','slug','name','displayName','englishName','title','tagline','company','bio','professionalSummary','location','availabilityStatus','avatarUrl','coverImageUrl','githubUrl','linkedinUrl','portfolioUrl'];
// This is the public projection for unsaved owner drafts. Only this allowlisted
// object crosses into the preview frame. The server still enforces public access.
export function projectPublicProfile(source={},links=[]){
 const p=Object.fromEntries(headerFields.filter(k=>source[k]!=null).map(k=>[k,source[k]]));
 const layout=source.contentLayout||{}, privacy=source.privacy||{};
 p.sections={};
 for(const [key,items] of Object.entries(source.sections||{})){
  p.sections[key]=layout.visibility?.[key]==='HIDDEN'?[]:items.filter(i=>i.visibility==='PUBLIC').map(i=>structuredClone(i));
 }
 if(!Object.hasOwn(p.sections,'skills'))p.sections.skills=(source.skillsWithLevel?.length?source.skillsWithLevel:(source.skills||[]).map(name=>({name}))).map((s,i)=>({id:'legacy-skill-'+i,visibility:'PUBLIC',name:s.name,proficiencyLevel:s.level||'',category:s.category||''}));
 if(privacy.showLocation===false)delete p.location;
 if(privacy.showAvailability===false){delete p.availabilityStatus;p.sections.availability=[];}
 if(p.sections.contacts)p.sections.contacts=p.sections.contacts.filter(i=>!(i.type==='EMAIL'&&!privacy.showEmail)&&!(i.type==='PHONE'&&!privacy.showPhone));
 if(Object.hasOwn(p.sections,'about')){delete p.bio;delete p.professionalSummary;}
 if(Object.hasOwn(p.sections,'availability'))delete p.availabilityStatus;
 for(const items of Object.values(p.sections))for(const i of items)for(const [field,target] of Object.entries({skillIds:'skills',experienceIds:'experiences',projectIds:'projects',certificationIds:'certifications'}))if(i[field])i[field]=i[field].filter(id=>p.sections[target]?.some(v=>v.id===id));
 p.featured=(source.featured||layout.featured||[]).filter(ref=>p.sections[ref.section]?.some(i=>i.id===ref.id));
 p.sectionOrder=source.sectionOrder||layout.order||[];
 p.links=links.filter(l=>l.isActive!==false&&l.visibility!=='HIDDEN'&&l.privacy==='PUBLIC'&&safeLink(l.url)).map(l=>({id:l.id,title:l.title,url:safeLink(l.url),description:l.description||''}));
 if(!Object.hasOwn(p.sections,'contacts')){
  p.sections.contacts=[];
  for(const [key,type,allowed] of [['email','EMAIL',privacy.showEmail],['phone','PHONE',privacy.showPhone]])if(allowed&&source[key])p.sections.contacts.push({id:'legacy-'+key,label:enumLabels[type],type,value:source[key],visibility:'PUBLIC'});
 }
 return p;
}
function imageUrl(value){if(typeof value!=='string'||!value||/[\\\r\n\t]/.test(value))return '';return value.startsWith('/')&&!value.startsWith('//')?value:safeLink(value);}
function picture(url,alt,cls){const wrap=el('div','',cls),src=imageUrl(url);if(src){const img=el('img');img.src=src;img.alt=alt;img.decoding='async';img.onerror=()=>{img.remove();wrap.classList.add('pp-image-fallback');};wrap.append(img);}return wrap;}
function action(text,kind,href,primary=false){const a=el(href?'a':'button',text,`pp-button${primary?' pp-primary':''}`);if(href)a.href=href;else a.type='button';a.append(icon(kind));return a;}
function tags(values,cls='pp-tags'){const box=el('div','',cls);for(const v of (values||[]).slice(0,12))box.append(el('span',v));return box;}
function technologyIcon(name){
 const map={'kubernetes':['kubernetes','#326CE5'],'docker':['docker','#2496ED'],'terraform':['terraform','#844FBA'],'gitlab ci/cd':['gitlab','#FC6D26'],'gitlab':['gitlab','#FC6D26'],'prometheus':['prometheus','#E6522C'],'grafana':['grafana','#F46800'],'linux':['linux','#16162e'],'python':['python','#3776AB']};
 const value=map[String(name).toLowerCase()];if(!value)return icon('layers');
 const mark=el('span','','pp-tech-brand');mark.setAttribute('aria-hidden','true');mark.style.maskImage=`url('/assets/images/tech/${value[0]}.svg')`;mark.style.backgroundColor=value[1];return mark;
}
function contactUrl(i){const value=String(i.value||'').trim();if(i.type==='EMAIL'&&/^[^\s@<>]+@[^\s@<>]+\.[^\s@<>]+$/.test(value))return 'mailto:'+value;if(i.type==='PHONE'&&/^\+?[\d ()-]{3,30}$/.test(value))return 'tel:'+value.replace(/[ ()-]/g,'');return safeLink(value);}
function heading(key,title){const h=el('h2',title||labels[key]);h.prepend(icon(key));return h;}
function card(key,item,data){
 const node=renderItem(key,item,data.id,data.sections);node.classList.add('pp-card',`pp-card-${key}`);node.id=`item-${key}-${item.id}`;
 const h=node.querySelector('h3');h.dir='auto';node.querySelector('summary').textContent=key==='projects'?'مطالعهٔ کامل پروژه':'مشاهدهٔ جزئیات';
 const meta=[item.organizationName||item.issuingOrganization||item.publisher||item.institutionName!==item[titleFields[key]]&&item.institutionName||item.category||item.authorTitle||'',item.startDate?.year?[dateText(item.startDate),item.isCurrent||item.isOngoing?'اکنون':dateText(item.endDate)].filter(Boolean).join(' تا '):dateText(item.issueDate||item.publicationDate||item.date)].filter(Boolean).join(' · ');
 if(meta)h.after(el('p',meta,'pp-meta'));
 if(item.technologies?.length)node.querySelector('details').before(tags(item.technologies));
 if(key==='projects'||key==='publications'||key==='openSource'){
  const url=item.media?.[0]?`/api/profiles/${encodeURIComponent(data.id)}/media/${encodeURIComponent(item.media[0].id)}`:'';
  const visual=picture(url,item.media?.[0]?.alt||item.title||item.repositoryName,'pp-project-art');if(!url){visual.append(icon(key),el('span',item.organization||item.publisher||item.platform||labels[key]));}node.prepend(visual);
 }
 if(key==='experiences'||key==='education'||key==='certifications'){
  const mark=el('span',Array.from(item.organizationName||item.institutionName||item.issuingOrganization||'')[0]||'','pp-mark');if(!mark.textContent)mark.append(icon(key));node.prepend(mark);
 }
 if(key==='skills'||key==='languages'){
  const level=key==='skills'?item.proficiencyLevel:item.proficiency;
  const steps=key==='skills'?['BEGINNER','INTERMEDIATE','ADVANCED','EXPERT']:['BASIC','CONVERSATIONAL','PROFESSIONAL','FLUENT','NATIVE'];
  if(steps.includes(level)){const bar=el('div','', 'pp-level');bar.setAttribute('role','img');bar.setAttribute('aria-label',enumLabels[level]);const fill=el('span');fill.style.width=`${(steps.indexOf(level)+1)/steps.length*100}%`;bar.append(fill);node.querySelector('details').before(bar,el('span',enumLabels[level],'pp-level-label'));}
 }
 if(key==='contacts'){const url=contactUrl(item);if(url){const a=action('ارتباط','arrow',url);node.querySelector('details').before(a);}}
 if(key==='services'||key==='availability'||key==='achievements')node.prepend(icon(key));
 if(key==='recommendations'){node.prepend(el('span','“','pp-quote'));const summary=node.querySelector('.content-summary');if(summary)node.prepend(summary);}
 return node;
}

export function renderPublicProfile(root,input,{preview=false}={}){
 const data=structuredClone(input);data.sections||={};data.links||=[];
 const sections=data.sections, totals=data.totals||Object.fromEntries(Object.entries(sections).map(([k,v])=>[k,v.length]));
 const name=data.displayName||data.name||'نام شما';
 const about=Object.hasOwn(sections,'about')?sections.about[0]?.description||'':data.professionalSummary||data.bio||'';
 const available=Object.hasOwn(sections,'availability')?sections.availability[0]?.status:data.availabilityStatus;
 const social=[];
 for(const [key,label] of [['githubUrl','GitHub'],['linkedinUrl','LinkedIn'],['portfolioUrl','وب‌سایت']])if(safeLink(data[key]))social.push({label,url:safeLink(data[key])});
 for(const c of sections.contacts||[]){const url=contactUrl(c);if(url)social.push({label:c.label||enumLabels[c.type],url});}
 const hasContact=social.length>0||data.links.length>0;
 const contentTarget=sections.contacts?.length?'#section-contacts':data.links.length?'#section-links':'#profile-social';
 const rootNode=el('div','','public-profile');rootNode.id='profile-home';
 const status=el('p','','pp-status');status.setAttribute('role','status');status.setAttribute('aria-live','polite');
 const skip=el('a','رفتن به محتوای پروفایل','pp-skip');skip.href='#profile-content';rootNode.append(skip);
 const nav=el('header','','pp-nav');
 const brand=el('a','هاتف','pp-brand');brand.href='/';brand.prepend(icon('brand'));nav.append(brand);
 const menu=el('nav','','pp-menu');menu.id='profile-navigation';menu.setAttribute('aria-label','بخش‌های پروفایل');
 for(const [key,label] of [['home','خانه'],['about','دربارهٔ من'],['experiences','سوابق کاری'],['projects','پروژه‌ها'],['services','خدمات'],['skills','مهارت‌ها'],['publications','مقالات'],['contacts','تماس']]){
  if(key!=='home'&&!totals[key]&&!(key==='about'&&about))continue;
  const a=el('a',label);a.href=key==='home'?'#profile-home':'#section-'+key;menu.append(a);
 }
 const toggle=action('فهرست','menu');toggle.classList.add('pp-menu-toggle');toggle.setAttribute('aria-controls',menu.id);toggle.setAttribute('aria-expanded','false');toggle.onclick=()=>{const open=toggle.getAttribute('aria-expanded')!=='true';toggle.setAttribute('aria-expanded',String(open));menu.classList.toggle('is-open',open);};
 menu.onclick=()=>{menu.classList.remove('is-open');toggle.setAttribute('aria-expanded','false');};
 nav.append(menu,toggle);if(hasContact)nav.append(action('تماس با من','send',contentTarget,true));rootNode.append(nav);
 const main=el('main','','pp-main');main.id='profile-content';main.tabIndex=-1;
 const hero=el('section','','pp-hero');hero.setAttribute('aria-labelledby','preview-name');
 const cover=imageUrl(data.coverImageUrl);if(cover){const bg=picture(cover,'','pp-hero-cover');bg.setAttribute('aria-hidden','true');hero.append(bg);}
 const avatar=picture(data.avatarUrl,`تصویر ${name}`,'pp-avatar');avatar.prepend(el('span',Array.from(name)[0],'pp-monogram'));const avatarImage=avatar.querySelector('img');if(avatarImage)avatarImage.id='preview-avatar';
 const intro=el('div','','pp-intro');const badge=el('span',enumLabels[available]||'','pp-availability');badge.id='preview-availability';badge.hidden=!available;badge.dataset.status=available||'';intro.append(badge);
 const h1=el('h1');h1.id='preview-name';h1.dir='auto';const parts=name.split(' ');h1.append(el('span',parts.shift()),document.createTextNode(parts.length?' '+parts.join(' '):''));intro.append(h1);
 if(data.englishName)intro.append(el('p',data.englishName,'pp-english'));
 if(data.title||data.tagline)intro.append(el('p',data.title||data.tagline,'pp-title'));
 const bio=el('p',about,'pp-bio');bio.id='preview-bio';bio.hidden=!about;intro.append(bio);
 if(data.location){const loc=el('p',data.location,'pp-location');loc.prepend(icon('location'));intro.append(loc);}
 const actions=el('div','','pp-hero-actions');if(hasContact)actions.append(action('تماس با من','send',contentTarget,true));
 const resume=action('دریافت رزومه','download');resume.dataset.resume='';actions.append(resume);intro.append(actions);hero.append(avatar,intro);main.append(hero);
 if(sections.skills?.length){const tech=el('section','','pp-tech');tech.append(heading('skills','فناوری‌ها و مهارت‌های اصلی من'));const list=el('div','','pp-tech-list');for(const s of sections.skills.slice(0,7)){const t=el('a',s.name,'pp-tech-token');t.href='#section-skills';t.prepend(technologyIcon(s.name));list.append(t);}if(totals.skills>7)list.append(action('سایر مهارت‌ها','arrow','#section-skills'));tech.append(list);main.append(tech);}
 const grid=el('div','','pp-sections');
 const aboutBox=el('section','','pp-section pp-about');aboutBox.id='section-about';aboutBox.hidden=!about;aboutBox.append(heading('about'),el('p',about));const aboutId=el('div');aboutId.id='preview-about';aboutId.hidden=!about;aboutId.append(aboutBox);if(about)grid.append(aboutId);
 const ref=(data.featured||[]).find(r=>r.section==='projects')||(sections.projects?.[0]?{section:'projects',id:sections.projects[0].id}:null);
 const featured=ref&&(data.featuredItems||[]).find(i=>i.id===ref.id)||ref&&sections.projects?.find(i=>i.id===ref.id);
 if(featured){const feature=el('section','','pp-section pp-featured');const featuredCard=card('projects',featured,data);featuredCard.id='featured-'+featured.id;feature.append(el('p','مطالعهٔ موردی منتخب','pp-eyebrow'),featuredCard);if(featured.outcomes?.length)feature.append(tags(featured.outcomes.slice(0,3),'pp-outcomes'));grid.prepend(feature);}
 const order=[...new Set([...(data.sectionOrder||[]),...Object.keys(labels)])];
 const loaders=[];
 for(const key of order){if(key==='about'||!sections[key]?.length)continue;
  const block=el('section','',`pp-section pp-section-${key}`);block.id='section-'+key;block.append(heading(key));
  const list=el('div','','pp-card-list');let offset=Math.min(3,sections[key].length);for(const item of sections[key].slice(0,offset))list.append(card(key,item,data));block.append(list);
  const more=action('مشاهدهٔ موارد بیشتر','arrow');more.dataset.moreSection=key;more.hidden=offset>=totals[key];
  const loadMore=async()=>{if(offset>=totals[key])return;more.disabled=true;try{
   let items=sections[key].slice(offset,offset+3);
   if(!preview){const response=await fetch(`/api/profiles/${encodeURIComponent(data.id)}/content/${encodeURIComponent(key)}?offset=${offset}&limit=3`,{credentials:'omit',cache:'no-store'});if(!response.ok)throw Error();const result=await response.json();items=result.data.items;totals[key]=result.data.total;}
   if(!items.length&&offset<totals[key])throw Error();
   for(const item of items)list.append(card(key,item,data));offset+=items.length;more.hidden=offset>=totals[key];more.textContent='مشاهدهٔ موارد بیشتر';
  }catch{more.textContent='بارگیری انجام نشد؛ تلاش مجدد';throw Error('بارگیری کامل رزومه انجام نشد. دوباره تلاش کنید.');}finally{more.disabled=false;}};
  more.onclick=()=>loadMore().catch(()=>{});loaders.push(async()=>{while(offset<totals[key])await loadMore();});block.append(more);grid.append(block);
 }
 main.append(grid);
 if(data.links.length){const links=el('section','','pp-section');links.id='section-links';links.append(heading('contacts','پیوندها'));const list=el('div','','pp-contact-links');for(const link of data.links){const url=safeLink(link.url);if(!url)continue;const a=action(link.title||url,'external',url);a.target='_blank';a.rel='noopener noreferrer';list.append(a);}links.append(list);main.append(links);}
 if(hasContact){const cta=el('section','','pp-cta');cta.append(heading('availability',available==='AVAILABLE'?'آمادهٔ همکاری هستید؟':'بیایید در ارتباط باشیم'),el('p',available==='AVAILABLE'?'برای گفتگو دربارهٔ فرصت‌های همکاری با من در تماس باشید.':'راه‌های ارتباطی و پیوندهای من را ببینید.'),action('تماس با من','send',contentTarget,true));main.append(cta);}
 const footer=el('footer','','pp-footer');footer.append(el('strong','هاتف'));const socials=el('div','','pp-social');socials.id='profile-social';for(const s of social){const a=el('a',s.label);a.href=s.url;a.rel='noopener noreferrer';if(s.url.startsWith('http'))a.target='_blank';socials.append(a);}footer.append(socials);
 const share=action('اشتراک‌گذاری','external');share.onclick=async()=>{try{if(preview){status.textContent='پیش‌نمایش خصوصی قابل اشتراک‌گذاری نیست. ابتدا صفحه را منتشر کنید.';return;}if(navigator.share)await navigator.share({title:name,url:location.href});else await navigator.clipboard.writeText(location.href);status.textContent='آدرس صفحه آمادهٔ اشتراک‌گذاری است.';}catch(e){if(e.name!=='AbortError')status.textContent='آدرس صفحه را از نوار مرورگر کپی کنید.';}};footer.append(share,el('small','ساخته‌شده با هاتف'));main.append(footer,status);rootNode.append(main);
 resume.onclick=async()=>{resume.disabled=true;const previouslyOpen=new Set([...rootNode.querySelectorAll('details[open]')]);try{for(const load of loaders)await load();rootNode.querySelectorAll('details').forEach(d=>d.open=true);window.print();}catch(e){status.textContent=e.message;}finally{rootNode.querySelectorAll('details').forEach(d=>d.open=previouslyOpen.has(d));resume.disabled=false;}};
 // Preserve navigation and expanded details while live draft updates arrive.
 const openIds=[...root.querySelectorAll('article:has(details[open])')].map(n=>n.id);
 root.replaceChildren(rootNode);for(const id of openIds){const detail=document.getElementById(id)?.querySelector('details');if(detail)detail.open=true;}
 document.title=`${name} | پروفایل حرفه‌ای هاتف`;
 return rootNode;
}

const payload=document.getElementById('public-profile-data');
if(payload){try{const data=JSON.parse(payload.content.textContent);const host=document.createElement('div');renderPublicProfile(host,data);document.querySelector('.profile-page').replaceWith(host);document.body.classList.add('public-profile-page');document.querySelectorAll('link[href="/assets/css/profile-header.css"],link[href="/assets/css/profile-content.css"]').forEach(n=>n.remove());}catch(error){console.error('Profile presentation failed',error);}}
