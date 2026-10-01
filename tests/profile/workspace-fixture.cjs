// Local UI test fixture, not a replacement for the C++/MongoDB integration suite.
// Reads schema fields from the real C++ declarations so every existing field is exercised.
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const root = path.resolve(__dirname, '../..');
const header = fs.readFileSync(path.join(root,'include/search_engine/profile/ProfileContent.h'),'utf8');
const source = fs.readFileSync(path.join(root,'src/profile/ProfileContent.cpp'),'utf8');
const definitions = [...source.matchAll(/\{"(\w+)","([^"]+)","(\w+)",(\d+),(\w+)\{\}\}/g)];
const sections = definitions.map(([,key,label,titleField,limit,type])=>{
 const declaration=header.match(new RegExp(`struct ${type} \\{([\\s\\S]*?)\\};`))[1];
 const defaults={};
 for(const line of declaration.split(';').map(s=>s.trim()).filter(Boolean)){
  const match=line.match(/^(std::string|std::vector<[^;]+>|PartialDate|bool|double|int)\s+(\w+)(?:\s*=\s*(.+))?$/);
  if(!match) throw Error('Unsupported fixture field: '+line);
  const [,kind,name,value]=match;
  defaults[name]=kind==='PartialDate'?{calendar:'persian',year:0,month:0,day:0}:kind.startsWith('std::vector')?[]:kind==='bool'?value==='true':kind==='std::string'?value?JSON.parse(value):'':Number(value||0);
 }
 return {key,label,titleField,limit:Number(limit),defaults};
});
const enums=Object.fromEntries([...source.matchAll(/\{"(\w+)", \{((?:"[A-Z_]+",?)+)\}\}/g)].map(([,k,v])=>[k,JSON.parse('['+v+']')]));
const clone=structuredClone, profiles=new Map(), links=new Map(), media=new Map();
function seed(id){
 const state={id,version:1,isPublic:false,name:'هاتف رستمخانی',title:'مهندس پلتفرم و DevOps',company:'شرکت نمونه',bio:'عاشق ساخت زیرساخت‌های پایدار و مقیاس‌پذیرم. با تجربه در طراحی و راه‌اندازی پلتفرم‌های ابری، به تیم‌ها کمک می‌کنم سریع‌تر و مطمئن‌تر محصول بسازند.',location:'تهران، ایران',availabilityStatus:'AVAILABLE',avatarUrl:'',coverImageUrl:'',sections:{},privacy:{showEmail:true,showPhone:true,showLocation:true,showAvailability:true},contentLayout:{order:sections.map(s=>s.key),visibility:{},featured:[]},completion:{score:65,recommendations:['با افزودن پروژه‌ها و تجربه‌های کاری، پروفایل خود را کامل کنید.']}};
 for(const def of sections){
  if(['about','availability','contacts'].includes(def.key)) continue;
  state.sections[def.key]=[{...clone(def.defaults),id:`${def.key}-one`,visibility:'PUBLIC',evidence:[],[def.titleField]:({experiences:'مهندس ارشد پلتفرم',projects:'پلتفرم استقرار Kubernetes',skills:'Kubernetes',education:'دانشگاه نمونه',certifications:'AWS Solutions Architect',services:'مشاوره DevOps',languages:'فارسی',publications:'طراحی زیرساخت مقیاس‌پذیر',openSource:'hatef/terraform-modules',achievements:'راه‌اندازی پلتفرم ابری',recommendations:'نویسندهٔ نمونه'})[def.key]||def.label}];
 }
 Object.assign(state.sections.experiences[0],{organizationName:'شرکت نمونه',employmentType:'FULL_TIME',location:'تهران، ایران',startDate:{calendar:'persian',year:1401,month:1,day:0},isCurrent:true,responsibilities:['طراحی و توسعه زیرساخت ابری با Kubernetes','خودکارسازی استقرار سرویس‌ها با CI/CD'],achievements:['کاهش زمان استقرار سرویس‌ها'],skillIds:['skills-one']});
 state.sections.experiences.push({...clone(state.sections.experiences[0]),id:'experiences-two',roleTitle:'مهندس DevOps',organizationName:'تیم زیرساخت'});
 Object.assign(state.sections.skills[0],{category:'زیرساخت ابری',proficiencyLevel:'ADVANCED',yearsOfExperience:3});
 profiles.set(id,state);links.set(id,[]);return state;
}
function start(port=4173){
 const server=http.createServer(async(req,res)=>{
  try{
   const url=new URL(req.url,'http://localhost'),parts=url.pathname.split('/').filter(Boolean);
   const send=(data,status=200)=>{res.writeHead(status,{'Content-Type':'application/json; charset=utf-8','Cache-Control':'no-store'});res.end(JSON.stringify(data));};
   if(parts[0]==='api'){
    let raw='';for await(const data of req)raw+=data;const body=raw?JSON.parse(raw):{};
    const id=parts[2]||'fixture',state=profiles.get(id)||seed(id),method=req.method;
    if(parts[3]==='content-schema')return send({data:{sections,enums}});
    if(parts[3]==='session')return send({ok:true});
    if(parts[3]==='links'){
     const list=links.get(id),linkId=parts[4]||body.id,index=list.findIndex(l=>l.id===linkId);
     if(method==='GET')return send({data:list});
     if(method==='DELETE'){list.splice(index,1);return send({ok:true});}
     const entry={...(list[index]||{}),...body,version:(list[index]?.version||0)+1};if(index<0)list.push(entry);else list[index]=entry;return send({data:entry});
    }
    if(method==='GET')return send({data:state,canEdit:true});
    if(body.version!==state.version)return send({message:'نسخه تغییر کرده است.'},409);
    if(parts[3]==='layout'){const {version,...patch}=body;Object.assign(state.contentLayout,patch);}
    else if(parts[3]==='content'){
     const section=parts[4],itemId=parts[5],def=sections.find(s=>s.key===section),items=state.sections[section]||=[];
     if(itemId==='order')state.sections[section]=body.ids.map(id=>items.find(i=>i.id===id));
     else if(method==='DELETE')state.sections[section]=items.filter(i=>i.id!==itemId);
     else {const index=items.findIndex(i=>i.id===(itemId||body.item.id));if(index<0)items.push({...clone(def.defaults),...body.item});else Object.assign(items[index],body.item);}
    }else if(parts[3]==='avatar'||parts[3]==='cover'){
     const key=`${id}-${parts[3]}`;media.set(key,Buffer.from(body.image.split(',').pop(),'base64'));
     state[parts[3]==='avatar'?'avatarUrl':'coverImageUrl']='/fixture-media/'+key;
    }
    else {const {version,...patch}=body;Object.assign(state,patch);}
    state.version++;return send({data:state});
   }
   if(parts[0]==='fixture-media'&&media.has(parts[1])){res.writeHead(200,{'Content-Type':'image/png'});return res.end(media.get(parts[1]));}
   if(parts[0]==='assets'){
    const file=path.resolve(root,'public','.'+url.pathname);if(!file.startsWith(path.join(root,'public')+path.sep))return send({},403);
    const types={'.js':'text/javascript','.css':'text/css','.woff2':'font/woff2','.svg':'image/svg+xml','.html':'text/html; charset=utf-8'};res.writeHead(200,{'Content-Type':types[path.extname(file)]||'application/octet-stream'});return res.end(fs.readFileSync(file));
   }
   const id=url.searchParams.get('id')||'fixture';if(!profiles.has(id))seed(id);
   let html=fs.readFileSync(path.join(root,'templates/profile_editor.inja'),'utf8').replace(/{% if state != "new" %}hidden{% endif %}/g,'hidden').replace(/{{ state }}/g,'edit').replace(/{{ profileId }}/g,id).replace(/{{ (?:encodedSlug|slug) }}/g,'آزمایش');
   res.writeHead(200,{'Content-Type':'text/html; charset=utf-8','Cache-Control':'no-store'});res.end(html);
  }catch(e){res.writeHead(500);res.end(String(e));}
 });
 return server.listen(port,'0.0.0.0',()=>console.log('UI fixture: http://127.0.0.1:'+server.address().port));
}
if(require.main===module)start(Number(process.env.PORT||4173));
module.exports={start,sections,seedEmpty(id){const state=seed(id);state.sections={};return state;}};
