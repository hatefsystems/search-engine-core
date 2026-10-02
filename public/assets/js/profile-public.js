import {el, safeLink, renderItem, sectionLabels, titleFields, enumLabels, labels as fieldLabels} from './profile-content-ui.js';

const icons={brand:'m3 8 8-6 2 3-2 9-8 3Zm11 2 7-6v13l-8 5Z',home:'M3 10 12 3l9 7v11h-6v-7H9v7H3Z',menu:'M4 6h16M4 12h16M4 18h16',arrow:'M19 12H5m6-6-6 6 6 6',send:'m22 2-7 20-4-9-9-4Zm0 0L11 13',download:'M12 3v12m-5-5 5 5 5-5M4 16v5h16v-5',location:'M20 10c0 6-8 12-8 12S4 16 4 10a8 8 0 1 1 16 0ZM9 10a3 3 0 1 0 6 0 3 3 0 1 0-6 0',layers:'m12 2 10 6-10 6L2 8Zm-10 11 10 6 10-6M2 18l10 6 10-6',projects:'M3 5h6l2 3h10v13H3Z',experiences:'M3 7h18v14H3ZM8 7V3h8v4M3 12h18M10 11v3h4v-3',skills:'m12 2 10 10-10 10L2 12Z',education:'m2 8 10-5 10 5-10 5Zm4 3v7q6 5 12 0v-7M22 8v9',certifications:'m12 2 3 2 4 1 1 4 2 3-2 3-1 4-4 1-3 2-3-2-4-1-1-4-2-3 2-3 1-4 4-1Zm-4 10 3 3 5-6',publications:'M5 2h10l5 5v15H5ZM14 2v6h6M8 12h9M8 16h9',openSource:'m8 5-6 7 6 7m8-14 6 7-6 7M14 2l-4 20',services:'M12 2 3 7v10l9 5 9-5V7Zm-9 5 9 5 9-5M12 12v10',achievements:'M7 2h10v9a5 5 0 0 1-10 0ZM7 5H2v4q0 5 5 5M17 5h5v4q0 5-5 5M12 16v6M7 22h10',languages:'M12 2a10 10 0 1 0 0 20 10 10 0 1 0 0-20ZM2 12h20M12 2q-8 10 0 20M12 2q8 10 0 20',recommendations:'M3 3h18v14H9l-6 5ZM7 8h10M7 12h7',contacts:'M2 4h20v16H2Zm0 0 10 9L22 4',availability:'M8 12a4 4 0 1 0 0-8 4 4 0 1 0 0 8ZM1 22v-3q0-6 7-6t7 6v3M16 4q7 0 7 5t-7 5M18 16q5 0 5 6',about:'M12 3v1M12 8v13M3 12a9 9 0 1 0 18 0 9 9 0 1 0-18 0',external:'M14 3h7v7m0-7L10 14M10 3H3v18h18v-7'};
function icon(name){const svg=document.createElementNS('http://www.w3.org/2000/svg','svg');svg.setAttribute('viewBox','0 0 24 24');svg.setAttribute('aria-hidden','true');svg.classList.add('pp-icon');const p=document.createElementNS(svg.namespaceURI,'path');p.setAttribute('d',icons[name]||icons.layers);svg.append(p);return svg;}
const labels={...sectionLabels,experiences:'سوابق کاری',projects:'پروژه‌ها و نمونه‌کارها',publications:'مقالات و نوشته‌ها',skills:'مهارت‌های کلیدی'};
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
function contactUrl(i){const value=String(i.value||'').trim();if(i.type==='EMAIL'&&/^[^\s@<>]+@[^\s@<>]+\.[^\s@<>]+$/.test(value))return 'mailto:'+value;if(i.type==='PHONE'&&/^\+?[\d ()-]{3,30}$/.test(value))return 'tel:'+value.replace(/[ ()-]/g,'');return safeLink(value);}
function heading(key,title){const h=el('h2',title||labels[key]);h.prepend(icon(key));return h;}
const renderSessions=new WeakMap();
// Keep partial dates at their saved precision, without converting calendars.
export function publicDate(value){
 if(!value?.year)return '';
 const months=value.calendar==='gregory'?['ژانویه','فوریه','مارس','آوریل','مه','ژوئن','ژوئیه','اوت','سپتامبر','اکتبر','نوامبر','دسامبر']:['فروردین','اردیبهشت','خرداد','تیر','مرداد','شهریور','مهر','آبان','آذر','دی','بهمن','اسفند'];
 const number=n=>Number(n).toLocaleString('fa-IR',{useGrouping:false});
 return [value.day?number(value.day):'',value.month?months[value.month-1]:'',number(value.year),value.calendar==='gregory'?'میلادی':''].filter(Boolean).join(' ');
}
function dateRange(item){const start=publicDate(item.startDate),end=item.isCurrent||item.isOngoing?'اکنون':publicDate(item.endDate);return start&&end?`${start} تا ${end}`:start?`از ${start}`:end?(end==='اکنون'?'در حال انجام':`تا ${end}`):'';}
const publicFieldLabels={role:'نقش',solution:'راه‌حل اجراشده',problem:'مسئله',outcomes:'نتایج',responsibilities:'مسئولیت‌ها',skillIds:'مهارت‌های مرتبط',projectIds:'پروژه‌های مرتبط',experienceIds:'سوابق مرتبط',certificationIds:'گواهینامه‌های مرتبط',credentialUrl:'اعتبارسنجی مدرک'};
function isolated(text){const bdi=el('bdi',text);bdi.dir='auto';return bdi;}
function card(key,item,data){
 const node=renderItem(key,item,data.id,data.sections);node.classList.add('pp-card',`pp-card-${key}`);node.id=`item-${key}-${item.id}`;
 const title=node.querySelector('h3');title.removeAttribute('dir');title.replaceChildren(isolated(title.textContent));
 const details=node.querySelector('details');details.querySelector('summary').remove();
 const body=el('div','','pp-card-body');body.append(...details.childNodes);details.replaceWith(body);
 const list=body.querySelector('dl'),head=el('header','','pp-card-heading');head.append(title);node.prepend(head);
 const metadata=el('div','','pp-position-facts');
 const summary=node.querySelector('.content-summary');
 if(key==='projects'&&item.subtitle){head.append(el('p',item.subtitle,'pp-subtitle'));if(summary?.textContent===item.subtitle)summary.remove();}
 const fieldOrder=['organizationName','institutionName','issuingOrganization','organization','role','employmentType','location','locationType','startDate','endDate','isCurrent','isOngoing','issueDate','expirationDate','publicationDate','date','publisher','authorTitle','degree','fieldOfStudy','kind','category','proficiencyLevel','proficiency','yearsOfExperience','firstUsedYear','lastUsedYear','isCurrentlyUsing','type','status','projectType','platform','contributionType','deliveryMode','pricingMode','price','currency','availability','contactMethod','problem','solution','architecture','responsibilities','challenges','achievements','outcomes','activities','technologies','skillIds','projectIds','experienceIds','certificationIds'];
 const fieldFor=dt=>Object.keys(fieldLabels).find(k=>fieldLabels[k]===dt.textContent);
 const rank=dt=>{const i=fieldOrder.indexOf(fieldFor(dt));return i<0?fieldOrder.length:i;};
 for(const dt of [...list.querySelectorAll('dt')].sort((a,b)=>rank(a)-rank(b))){
  const dd=dt.nextElementSibling,field=fieldFor(dt),value=item[field];
  if(['organizationProfileId','institutionProfileId','startDate','endDate','isCurrent','isOngoing'].includes(field)||(key==='projects'&&field==='subtitle')||(key==='skills'&&['category','proficiencyLevel','isCurrentlyUsing'].includes(field))){dt.remove();dd.remove();continue;}
  if(field==='location'&&item.locationType==='REMOTE'&&['دورکار','دورکاری'].includes(value)){dt.remove();dd.remove();continue;}
  dt.textContent=publicFieldLabels[field]||dt.textContent;
  // Direction isolation belongs on the value, not the entire description column.
  dd.removeAttribute('dir');
  const target={skillIds:'skills',projectIds:'projects',experienceIds:'experiences',certificationIds:'certifications'}[field];
  if(Array.isArray(value)){
   const ul=el('ul');
   for(const v of value){const li=el('li');if(target){const a=el('a');a.append(isolated((data.sections[target]||[]).find(i=>i.id===v)?.[titleFields[target]]||sectionLabels[target]));a.href=`#item-${target}-${v}`;a.dataset.referenceSection=target;a.dataset.referenceId=v;li.append(a);}else li.append(isolated(v));ul.append(li);}
   dd.replaceChildren(ul);
  }else if(typeof value==='object')dd.replaceChildren(isolated(publicDate(value)));
  else if(!dd.querySelector('a'))dd.replaceChildren(isolated(dd.textContent));
  if(field==='credentialUrl'){const a=dd.querySelector('a');if(a)a.textContent='مشاهده و اعتبارسنجی مدرک';}
  const isHeader=(['experiences','projects','education','certifications'].includes(key)&&['organizationName','issuingOrganization','organization','employmentType','location','locationType','role','projectType'].includes(field));
  if(isHeader){const fact=el('span');fact.dataset.field=field;if(field==='role')fact.append(document.createTextNode('نقش در پروژه: '));fact.append(...dd.childNodes);metadata.append(fact);dt.remove();dd.remove();continue;}
  const prose=['responsibilities','achievements','outcomes','challenges','problem','solution','architecture','description','activities','content','summary'].includes(field);
  const group=el('div','',prose?'pp-prose-field':'pp-fact');group.dataset.field=field;
  if(target||field==='technologies')group.classList.add('pp-related');
  group.append(dt,dd);list.append(group);
 }
 const range=dateRange(item);if(range){const time=el('span',range,'pp-timeline');metadata.append(time);}
 if(metadata.children.length)head.append(metadata);
 if(key==='skills'){
  const meta=el('div','','pp-skill-meta');
  if(item.proficiencyLevel)meta.append(el('span',enumLabels[item.proficiencyLevel]||item.proficiencyLevel));
  if(item.isCurrentlyUsing)meta.append(el('span','استفادهٔ فعلی','pp-current-badge'));
  head.append(meta);
 }
 if(!list.children.length)list.remove();
 const gallery=node.querySelector('.content-gallery');
 if(!gallery.children.length)gallery.remove();
 else{
  gallery.setAttribute('aria-label',`تصاویر ${item[titleFields[key]]}`);
  for(const [index,img] of [...gallery.children].entries()){
   const figure=el('figure'),a=el('a');a.href=img.src;a.target='_blank';a.rel='noopener noreferrer';a.setAttribute('aria-label',`بازکردن تصویر: ${img.alt}`);
   img.replaceWith(figure);a.append(img);figure.append(a);
   if(item.media[index]?.alt)figure.append(el('figcaption',item.media[index].alt));
   img.onerror=()=>{figure.remove();if(!gallery.children.length)gallery.remove();};
  }
  const layout=el('div','',`pp-story-layout${gallery.children.length>2?' pp-gallery-many':''}`);body.replaceWith(layout);layout.append(body,gallery);
 }
 if(key==='contacts'){const url=contactUrl(item);if(url)body.append(action('ارتباط','arrow',url));}
 if(key==='projects'&&(data.featured||[]).some(r=>r.section===key&&r.id===item.id)){
  node.classList.add('pp-selected');head.prepend(el('p','پروژهٔ منتخب','pp-eyebrow'));
 }
 return node;
}

export function renderPublicProfile(root,input,{preview=false}={}){
 renderSessions.get(root)?.abort();const session=new AbortController();renderSessions.set(root,session);
 const data=structuredClone(input);data.sections||={};data.links||=[];
 const sections=data.sections, totals=data.totals||Object.fromEntries(Object.entries(sections).map(([k,v])=>[k,v.length]));
 const name=data.displayName||data.name||'نام شما';
 const about=Object.hasOwn(sections,'about')?sections.about[0]?.description||'':data.professionalSummary||data.bio||'';
 const available=Object.hasOwn(sections,'availability')?sections.availability[0]?.status:data.availabilityStatus;
 const social=[];
 for(const [key,label] of [['githubUrl','GitHub'],['linkedinUrl','LinkedIn'],['portfolioUrl','وب‌سایت']])if(safeLink(data[key]))social.push({label,url:safeLink(data[key])});
 for(const c of sections.contacts||[]){const url=contactUrl(c);if(url)social.push({label:c.label||enumLabels[c.type],url});}
 const hasContact=social.length>0||data.links.length>0;
 const contentTarget='#section-contacts';
 const rootNode=el('div','','public-profile');rootNode.id='profile-home';
 const status=el('p','','pp-status');status.setAttribute('role','status');status.setAttribute('aria-live','polite');
 const skip=el('a','رفتن به محتوای پروفایل','pp-skip');skip.href='#profile-content';rootNode.append(skip);
 const nav=el('header','','pp-nav');
 const brand=el('a','هاتف','pp-brand');brand.href='/';brand.prepend(icon('brand'));nav.append(brand);
 const menu=el('nav','','pp-menu');menu.id='profile-navigation';menu.setAttribute('aria-label','بخش‌های پروفایل');
 const navOrder=[...new Set(['home','about',...(data.sectionOrder||[]),...Object.keys(labels),'contacts'])];
 for(const key of navOrder){
  if(key!=='home'&&!totals[key]&&!(key==='about'&&about)&&!(key==='contacts'&&hasContact))continue;
  const a=el('a',key==='home'?'معرفی':labels[key]||sectionLabels[key]);a.href=key==='home'?'#profile-home':'#section-'+key;menu.append(a);
 }
 const toggle=action('فهرست','menu');toggle.classList.add('pp-menu-toggle');toggle.setAttribute('aria-controls',menu.id);toggle.setAttribute('aria-expanded','false');toggle.onclick=()=>{const open=toggle.getAttribute('aria-expanded')!=='true';toggle.setAttribute('aria-expanded',String(open));menu.classList.toggle('is-open',open);};
 menu.onclick=()=>{menu.classList.remove('is-open');toggle.setAttribute('aria-expanded','false');};
 menu.addEventListener('keydown',event=>{if(event.key==='Escape'){menu.classList.remove('is-open');toggle.setAttribute('aria-expanded','false');toggle.focus();}});
 nav.append(menu,toggle);if(hasContact)nav.append(action('تماس با من','send',contentTarget,true));rootNode.append(nav);
 const main=el('main','','pp-main');main.id='profile-content';main.tabIndex=-1;
 const hero=el('section','','pp-hero');hero.setAttribute('aria-labelledby','preview-name');
 const cover=imageUrl(data.coverImageUrl);if(cover){hero.classList.add('has-cover');const bg=picture(cover,'','pp-hero-cover');bg.setAttribute('aria-hidden','true');hero.append(bg);}
 const avatar=picture(data.avatarUrl,`تصویر ${name}`,'pp-avatar');avatar.prepend(el('span',Array.from(name)[0],'pp-monogram'));const avatarImage=avatar.querySelector('img');if(avatarImage)avatarImage.id='preview-avatar';
 const intro=el('div','','pp-intro');const badge=el('span',enumLabels[available]||'','pp-availability');badge.id='preview-availability';badge.hidden=!available;badge.dataset.status=available||'';intro.append(badge);
 const h1=el('h1');h1.id='preview-name';h1.dir='auto';const parts=name.split(' ');h1.append(el('span',parts.shift()),document.createTextNode(parts.length?' '+parts.join(' '):''));intro.append(h1);
 if(data.englishName)intro.append(el('p',data.englishName,'pp-english'));
 if(data.title||data.tagline)intro.append(el('p',data.title||data.tagline,'pp-title'));
 if(data.tagline&&data.tagline!==data.title&&data.tagline!==about)intro.append(el('p',data.tagline,'pp-bio'));
 if(data.location){const loc=el('p',data.location,'pp-location');loc.prepend(icon('location'));intro.append(loc);}
 const actions=el('div','','pp-hero-actions');if(hasContact)actions.append(action('تماس با من','send',contentTarget,true));
 const resume=action('دریافت رزومه','download');resume.dataset.resume='';actions.append(resume);intro.append(actions);hero.append(avatar,intro);main.append(hero);
 const grid=el('div','','pp-sections');
 const aboutBox=el('section','','pp-section pp-about');aboutBox.id='section-about';aboutBox.hidden=!about&&!sections.about?.length;
 aboutBox.append(heading('about'));
 if(sections.about?.length){const first=card('about',sections.about[0],data);if(!sections.about[0].title||sections.about[0].title==='دربارهٔ من')first.querySelector('h3')?.remove();let bio=first.querySelector('.content-summary');if(!bio){bio=el('p',about);first.prepend(bio);}bio.id='preview-bio';aboutBox.append(first);}else{const bio=el('p',about);bio.id='preview-bio';aboutBox.append(bio);}
 const aboutId=el('div');aboutId.id='preview-about';aboutId.hidden=!about&&!sections.about?.length;aboutId.append(aboutBox);grid.append(aboutId);
 const order=[...new Set([...(data.sectionOrder||[]),...Object.keys(labels)])];
 const loaders=[];
 const refreshReferences=()=>{for(const a of rootNode.querySelectorAll('[data-reference-id]')){const item=sections[a.dataset.referenceSection]?.find(i=>i.id===a.dataset.referenceId);if(item)a.replaceChildren(isolated(item[titleFields[a.dataset.referenceSection]]));}};
 for(const key of order){if(key==='contacts'||!sections[key]?.length||key==='about'&&sections.about.length<=1)continue;
  const block=key==='about'?aboutBox:el('section','',`pp-section pp-section-${key}`);
  if(key!=='about'){block.id='section-'+key;block.append(heading(key));}
  const list=el('div','','pp-card-list'),groups=new Map();block.append(list);
  const append=item=>{
   if(key==='skills'){
    const category=item.category||'سایر مهارت‌ها';
    if(!groups.has(category)){const group=el('section','','pp-skill-group');group.append(el('h3',category));const cards=el('div','','pp-skill-list');group.append(cards);list.append(group);groups.set(category,cards);}
    const c=card(key,item,data);const h=c.querySelector('h3');const h4=el('h4');h4.append(...h.childNodes);h.replaceWith(h4);
    for(const fact of c.querySelectorAll('.pp-fact'))if(fact.querySelector('dt').textContent===fieldLabels.category)fact.remove();
    groups.get(category).append(c);
   }else list.append(card(key,item,data));
  };
  for(const item of sections[key].slice(key==='about'?1:0))append(item);
  let offset=sections[key].length,pending;
  const progress=el('p','','pp-load-status');progress.setAttribute('role','status');
  const retry=action('تلاش دوباره برای دریافت اطلاعات','arrow');retry.hidden=true;block.append(progress,retry);
  const load=()=>{
   if(pending)return pending;
   if(preview||offset>=totals[key])return Promise.resolve();
   pending=(async()=>{retry.hidden=true;progress.textContent='در حال دریافت ادامهٔ اطلاعات…';block.setAttribute('aria-busy','true');
    try{while(offset<totals[key]){
     const response=await fetch(`/api/profiles/${encodeURIComponent(data.id)}/content/${encodeURIComponent(key)}?offset=${offset}&limit=100`,{credentials:'omit',cache:'no-store',signal:session.signal});
     if(!response.ok)throw Error();const result=await response.json();const items=result.data.items;
     if(!Array.isArray(items)||!items.length)throw Error();
     totals[key]=result.data.total;sections[key].push(...items);for(const item of items)append(item);offset+=items.length;refreshReferences();
    }progress.textContent='';}
    catch(error){if(error.name!=='AbortError'){progress.textContent='دریافت بخشی از اطلاعات انجام نشد.';retry.hidden=false;}throw error;}
    finally{block.removeAttribute('aria-busy');pending=null;}
   })();return pending;
  };
  retry.onclick=()=>load().catch(()=>{});loaders.push(load);if(key!=='about')grid.append(block);
 }
 main.append(grid);
 if(hasContact){
  const contacts=el('section','','pp-section pp-contact-section');contacts.id='section-contacts';contacts.append(heading('contacts','راه‌های ارتباطی و پیوندها'));
  const list=el('div','','pp-contact-links');
  for(const item of sections.contacts||[])list.append(card('contacts',item,data));
  const known=new Set((sections.contacts||[]).map(contactUrl));
  for(const link of [...social,...data.links]){const url=safeLink(link.url)||(/^mailto:|^tel:/.test(link.url)?link.url:'');if(!url||known.has(url))continue;known.add(url);const wrap=el('div','','pp-contact-link');const a=action(link.title||link.label||url,'external',url);if(url.startsWith('http')){a.target='_blank';a.rel='noopener noreferrer';}wrap.append(a);if(link.description)wrap.append(el('p',link.description));list.append(wrap);}
  contacts.append(list);main.append(contacts);
 }
 if(hasContact){const cta=el('section','','pp-cta');cta.append(heading('availability',available==='AVAILABLE'?'آمادهٔ همکاری هستید؟':'بیایید در ارتباط باشیم'),el('p',available==='AVAILABLE'?'برای گفتگو دربارهٔ فرصت‌های همکاری با من در تماس باشید.':'راه‌های ارتباطی و پیوندهای من را ببینید.'),action('تماس با من','send',contentTarget,true));main.append(cta);}
 const footer=el('footer','','pp-footer');footer.append(el('strong','هاتف'));
 const share=action('اشتراک‌گذاری','external');share.onclick=async()=>{try{if(preview){status.textContent='پیش‌نمایش خصوصی قابل اشتراک‌گذاری نیست. ابتدا صفحه را منتشر کنید.';return;}if(navigator.share)await navigator.share({title:name,url:location.href});else await navigator.clipboard.writeText(location.href);status.textContent='آدرس صفحه آمادهٔ اشتراک‌گذاری است.';}catch(e){if(e.name!=='AbortError')status.textContent='آدرس صفحه را از نوار مرورگر کپی کنید.';}};footer.append(share,el('small','ساخته‌شده با هاتف'));main.append(footer,status);rootNode.append(main);
 resume.onclick=async()=>{resume.disabled=true;try{for(const load of loaders)await load();window.print();}catch{status.textContent='برای دریافت رزومهٔ کامل، بارگیری اطلاعات را دوباره امتحان کنید.';}finally{resume.disabled=false;}};
 root.replaceChildren(rootNode);
 const navLinks=[...menu.querySelectorAll('a')];
 const rail=menu.cloneNode(true);rail.id='profile-section-index';rail.className='pp-section-index';rail.setAttribute('aria-label','پیمایش همهٔ بخش‌ها');
 main.append(rail);
 const targets=navLinks.map(a=>({id:a.hash,node:rootNode.querySelector(a.hash)})).filter(t=>t.node&&!t.node.hidden).sort((a,b)=>a.node.compareDocumentPosition(b.node)&Node.DOCUMENT_POSITION_FOLLOWING?-1:1);
 let scheduled=false;
 const activateSection=()=>{
  scheduled=false;if(session.signal.aborted)return;
  let active=targets[0]?.id;for(const t of targets)if(t.node.getBoundingClientRect().top<=150)active=t.id;
  if(window.innerHeight+window.scrollY>=document.documentElement.scrollHeight-4)active=targets.at(-1)?.id;
  for(const a of [...navLinks,...rail.querySelectorAll('a')]){if(a.hash===active)a.setAttribute('aria-current','location');else a.removeAttribute('aria-current');}
 };
 window.addEventListener('scroll',()=>{if(!scheduled){scheduled=true;requestAnimationFrame(activateSection);}},{passive:true,signal:session.signal});
 window.addEventListener('resize',activateSection,{signal:session.signal});activateSection();
 document.fonts?.ready.then(()=>requestAnimationFrame(activateSection));
 // Sequential background loading keeps every public record available without clicks.
 (async()=>{for(const load of loaders){if(session.signal.aborted)break;try{await load();}catch{}}})();
 document.title=`${name} | پروفایل حرفه‌ای هاتف`;
 return rootNode;
}

const payload=document.getElementById('public-profile-data');
if(payload){try{const data=JSON.parse(payload.content.textContent);const host=document.createElement('div');renderPublicProfile(host,data);document.querySelector('.profile-page').replaceWith(host);document.body.classList.add('public-profile-page');document.querySelectorAll('link[href="/assets/css/profile-header.css"],link[href="/assets/css/profile-content.css"]').forEach(n=>n.remove());}catch(error){console.error('Profile presentation failed',error);}}
