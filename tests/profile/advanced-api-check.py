#!/usr/bin/env python3
"""Run only against an explicitly configured isolated server. Uses temporary profiles."""
import os, json, time, uuid, urllib.request, urllib.error, urllib.parse, statistics, base64, struct, zlib
BASE=os.environ['PROFILE_TEST_BASE_URL'].rstrip('/')
http=urllib.request.build_opener(urllib.request.ProxyHandler({}))
created=[];checks=0

def call(path,method='GET',body=None,key=None,status=200,headers=None):
    global checks
    h={'Content-Type':'application/json',**(headers or {})}
    if key:h['Authorization']='Bearer '+key
    request=urllib.request.Request(BASE+path,data=None if body is None else json.dumps(body,ensure_ascii=False).encode(),method=method,headers=h)
    # This suite exceeds the production limit of 120 owner mutations per minute.
    # Honor its backoff without changing the server limit or hiding other failures.
    for attempt in range(3):
        try:response=http.open(request,timeout=30)
        except urllib.error.HTTPError as e:response=e
        raw=response.read();response.close()
        if response.status!=429 or status==429 or not key or method=='GET' or attempt==2:break
        retry_after=response.headers.get('Retry-After','')
        assert retry_after.isdigit() and 0<=int(retry_after)<=60,('invalid Retry-After',retry_after)
        delay=max(1,int(retry_after))
        print('Owner mutation rate limit; retrying after',delay,'seconds',flush=True)
        time.sleep(delay)
    assert response.status==status,(method,path,response.status,raw[:600])
    assert response.headers.get('Server')=='HatefEngine 1.0',(path,response.headers)
    checks+=1
    try:return json.loads(raw)
    except (ValueError,UnicodeDecodeError):return raw

def create(role='متخصص'):
    slug='آزمون.پیشرفته.'+uuid.uuid4().hex[:12]
    data=call('/api/profiles','POST',{'type':'PERSON','slug':slug,'name':'ه','isPublic':False},status=201)
    p=data['data'];key=data['ownerToken'];created.append((p['id'],key));return p,key

def png(animated=False):
    def chunk(t,d):return struct.pack('!I',len(d))+t+d+struct.pack('!I',zlib.crc32(t+d)&0xffffffff)
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!2I5B',2,2,8,2,0,0,0))+chunk(b'tEXt',b'Comment\0PRIVATE-METADATA-DO-NOT-PUBLISH')+(chunk(b'acTL',struct.pack('!2I',2,0)) if animated else b'')+chunk(b'IDAT',zlib.compress(b'\0\xff\0\0\0\xff\0'*2))+chunk(b'IEND',b'')

try:
    p,key=create();id=p['id'];path='/api/profiles/'+id;version=p['version']
    call('/'+urllib.parse.quote(p['slug']),status=403)
    call(path,key='wrong',status=403)
    call(path+'/content/projects','POST',{'version':version,'item':{'id':'test-project','title':'ه'}},key='wrong',status=403)
    call(path+'/content/projects','POST',{'version':version,'item':{'id':'test-project','title':'ه'}},key=id,status=403)
    call(path+'/content/projects','POST',{'version':version,'item':{'id':'test-project','ownerToken':'bad'}},key=key,status=400)
    p=call(path+'/content/projects','POST',{'version':version,'item':{'id':'test-project','title':'ه'}},key=key)['data'];version=p['version']
    assert p['sections']['projects'][0]['visibility']=='HIDDEN'
    call(path+'/content/projects',status=404)
    replay=call(path+'/content/projects','POST',{'version':0,'item':{'id':'test-project','title':'ه'}},key=key)['data'];assert replay['version']==version
    call(path+'/content/projects/test-project','PUT',{'version':0,'item':{'title':'stale'}},key=key,status=409)
    item={'id':'skill-cpp','name':'C++','visibility':'PUBLIC','proficiencyLevel':'EXPERT'}
    p=call(path+'/content/skills','POST',{'version':version,'item':item},key=key)['data'];version=p['version']
    p=call(path+'/content/projects/test-project','PUT',{'version':version,'item':{'title':'نمونه‌کار دانشجویی','description':'<script>window.injection=true</script>','skillIds':['skill-cpp'],'problem':'مسئله','solution':'راه‌حل','outcomes':['نتیجه'],'visibility':'PUBLIC'}},key=key)['data'];version=p['version']
    fixtures={
        'about':{'title':'دربارهٔ من','description':'معرفی حرفه‌ای'},
        'experiences':{'roleTitle':'طراح','organizationName':'گروه داوطلب','employmentType':'VOLUNTEER','startDate':{'year':1402,'calendar':'persian'},'projectIds':['test-project']},
        'education':{'institutionName':'یادگیری مستقل','kind':'SELF_LEARNING'},
        'certifications':{'name':'مدرک نمونه','issuingOrganization':'مرجع','credentialUrl':'https://example.com/credential'},
        'publications':{'title':'مقالهٔ پژوهشی','type':'ARTICLE','url':'https://example.com/paper'},
        'openSource':{'repositoryName':'کتابخانهٔ متن‌باز','repositoryUrl':'https://example.com/repo'},
        'services':{'title':'مشاورهٔ فنی','pricingMode':'CONTACT','projectIds':['test-project']},
        'achievements':{'title':'دستاورد نمونه'},
        'languages':{'name':'فارسی','proficiency':'NATIVE'},
        'recommendations':{'authorName':'نویسندهٔ نمونه','content':'متن خوداظهاری','sourceUrl':'https://example.com/recommendation'},
        'contacts':{'label':'وب‌سایت','type':'LINK','value':'https://example.com/contact'},
        'availability':{'type':'CONSULTING','status':'AVAILABLE'},
    }
    for section,fields in fixtures.items():
        p=call(path+'/content/'+section,'POST',{'version':version,'item':{'id':'item-'+section.lower(),'visibility':'PUBLIC',**fields}},key=key)['data'];version=p['version']
    p=call(path+'/layout','PUT',{'version':version,'featured':[{'section':'projects','id':'test-project'}],'goal':'FIND_CLIENTS'},key=key)['data'];version=p['version']
    assert p['completion']['goal']=='FIND_CLIENTS'
    p=call(path,'PUT',{'version':version,'name':'هاتف آزمایشی','title':'طراح و پژوهشگر','location':'تهران','isPublic':True},key=key)['data'];version=p['version']
    public=call(path);assert len(public['data']['sections'])==14;assert 'completion' not in public['data'];assert key not in json.dumps(public)
    for alias in ['/'+urllib.parse.quote(p['slug']),'/profiles/'+urllib.parse.quote(p['slug'])]:
        raw=call(alias);assert b'<script>window.injection=true</script>' not in raw;assert 'نمونه‌کار دانشجویی'.encode() in raw
    # Live people search sees public sections only, and preserves punctuation in skills.
    for query in ['?skill=C%2B%2B','?q=C%2B%2B','?location='+urllib.parse.quote('تهران'),' ?availability=CONSULTING'.strip()]:
        results=call('/api/people'+query)['data'];assert any(i['id']==id for i in results['items']),(query,results)
    for query in ['?skill=C%23','?q=C%23','?q=C']:
        assert all(i['id']!=id for i in call('/api/people'+query)['data']['items']),query
    # Correct owner cookies require a same-origin mutation; bearer remains supported.
    req=urllib.request.Request(BASE+path+'/session',data=json.dumps({'key':key}).encode(),headers={'Content-Type':'application/json','Origin':BASE},method='POST')
    with http.open(req) as r:cookie=r.headers['Set-Cookie'].split(';')[0];assert 'HttpOnly' in r.headers['Set-Cookie'];assert 'SameSite=Strict' in r.headers['Set-Cookie']
    call(path+'/layout','PUT',{'version':version,'goal':'PORTFOLIO'},headers={'Cookie':cookie,'Origin':'https://attacker.invalid'},status=403)
    # Project images are decoded and rewritten, with visibility checked on every fetch.
    image='data:image/png;base64,'+base64.b64encode(png()).decode()
    p=call(path+'/projects/test-project/media','POST',{'version':version,'image':image,'alt':'تصویر نمونه'},key=key)['data'];version=p['version'];media=p['sections']['projects'][0]['media'][0]['id']
    imagepath=path+'/media/'+media;rewritten=call(imagepath);assert rewritten[8:12]==b'WEBP';assert b'PRIVATE-METADATA' not in rewritten
    call(path+'/projects/test-project/media','POST',{'version':version,'image':base64.b64encode(png(animated=True)).decode()},key=key,status=400)
    call(path+'/projects/test-project/media','POST',{'version':version-1,'image':image},key=key,status=409)
    call(path+'/projects/test-project/media','POST',{'version':version,'image':base64.b64encode(b'not an image').decode()},key=key,status=400)
    p=call(path+'/content/projects/test-project','PUT',{'version':version,'item':{'visibility':'HIDDEN'}},key=key)['data'];version=p['version']
    call(imagepath,status=404);assert call(imagepath,key=key)[8:12]==b'WEBP'
    public=call(path)['data'];assert 'نمونه‌کار دانشجویی' not in json.dumps(public,ensure_ascii=False);assert public['featured']==[]
    assert public['sections']['experiences'][0]['projectIds']==[]
    # Media belongs to exactly one item in each icon-capable section.
    for section,titlefield in [('experiences','roleTitle'),('projects','title'),('services','title'),('skills','name'),('achievements','title')]:
        itemid='media-'+section;itempath=path+'/content/'+section+'/'+itemid;upload=path+'/'+section+'/'+itemid+'/media'
        p=call(path+'/content/'+section,'POST',{'version':version,'item':{'id':itemid,titlefield:'آزمون تصاویر','visibility':'PUBLIC'}},key=key)['data'];version=p['version']
        call(upload,'POST',{'version':version,'image':image},status=403)
        call(upload,'POST',{'version':version,'image':base64.b64encode(b'not an image').decode()},key=key,status=400)
        for alt in ['نمودار اول','نمودار دوم']:
            p=call(upload,'POST',{'version':version,'image':image,'alt':alt},key=key)['data'];version=p['version']
        images=next(i for i in p['sections'][section] if i['id']==itemid)['media'];assert len(images)==2 and images[0]['id']!=images[1]['id']
        for m in images:assert call(path+'/media/'+m['id'])[8:12]==b'WEBP'
        p=call(itempath,'PUT',{'version':version,'item':{'iconMode':'manual','iconId':'lucide:scale'}},key=key)['data'];version=p['version']
        p=call(itempath,'PUT',{'version':version,'item':{titlefield:'عنوان تغییر یافته'}},key=key)['data'];version=p['version']
        saved=next(i for i in p['sections'][section] if i['id']==itemid);assert saved['iconMode']=='manual' and saved['iconId']=='lucide:scale'
        call(itempath,'PUT',{'version':version,'item':{'iconId':'../unsafe.svg'}},key=key,status=400)
        call(itempath,'PUT',{'version':version,'item':{'iconMode':'custom','iconMediaId':'not-owned'}},key=key,status=400)
        p=call(itempath,'PUT',{'version':version,'item':{'iconMode':'custom','iconMediaId':images[1]['id']}},key=key)['data'];version=p['version']
        persisted=call(path,key=key)['data'];assert next(i for i in persisted['sections'][section] if i['id']==itemid)['iconMediaId']==images[1]['id']

        ordered=[{**images[1],'alt':'توضیح ویرایش‌شده'},images[0]]
        p=call(itempath,'PUT',{'version':version,'item':{'media':ordered}},key=key)['data'];version=p['version']
        persisted=call(path,key=key)['data'];assert next(i for i in persisted['sections'][section] if i['id']==itemid)['media']==ordered
        call(itempath,'PUT',{'version':version,'item':{'media':[images[0],images[0]]}},key=key,status=400)
        call(itempath,'PUT',{'version':version,'item':{'media':[{**images[0],'alt':'a'*301}]}},key=key,status=400)
        call(path+'/content/'+section,'POST',{'version':version,'item':{'id':'forged-copy',titlefield:'کپی','media':images}},key=key,status=400)
        other='projects/test-project' if section=='experiences' else 'experiences/item-experiences'
        call(path+'/content/'+other,'PUT',{'version':version,'item':{'media':images}},key=key,status=400)
        p=call(itempath,'PUT',{'version':version,'item':{'visibility':'HIDDEN'}},key=key)['data'];version=p['version']
        call(path+'/media/'+images[0]['id'],status=404);call(path+'/media/'+images[0]['id'],key=key)
        p=call(itempath,'PUT',{'version':version,'item':{'visibility':'PUBLIC'}},key=key)['data'];version=p['version']
        p=call(path+'/layout','PUT',{'version':version,'visibility':{section:'HIDDEN'}},key=key)['data'];version=p['version']
        call(path+'/media/'+images[0]['id'],status=404);call(path+'/media/'+images[0]['id'],key=key)
        p=call(path+'/layout','PUT',{'version':version,'visibility':{section:'PUBLIC'}},key=key)['data'];version=p['version']
        p=call(path,'PUT',{'version':version,'isPublic':False},key=key)['data'];version=p['version']
        call(path+'/media/'+images[0]['id'],status=404);call(path+'/media/'+images[0]['id'],key=key)
        p=call(path,'PUT',{'version':version,'isPublic':True},key=key)['data'];version=p['version']
        p=call(itempath,'PUT',{'version':version,'item':{'media':[ordered[0]]}},key=key)['data'];version=p['version']
        call(path+'/media/'+images[0]['id'],key=key,status=404);call(path+'/media/'+images[1]['id'])
        p=call(itempath,'DELETE',{'version':version},key=key)['data'];version=p['version']
        call(path+'/media/'+images[1]['id'],key=key,status=404)
    p=call(path,'PUT',{'version':version,'tagline':'طراحی زیرساخت قابل اتکا'},key=key)['data'];version=p['version']
    assert call(path)['data']['tagline']=='طراحی زیرساخت قابل اتکا'
    call(path,'PUT',{'version':version,'tagline':'a'*121},key=key,status=400)
    # Clearing structured sections never resurrects legacy content.
    p=call(path+'/content/skills/skill-cpp','DELETE',{'version':version},key=key)['data'];version=p['version'];assert p['sections']['skills']==[]
    assert all(i['id']!=id for i in call('/api/people?skill=C%2B%2B')['data']['items'])
    call(path+'/content/projects/test-project','PUT',{'version':version,'item':{'skillIds':['another-profile-skill']}},key=key,status=400)
    call(path+'/content/projects/order','PUT',{'version':version,'ids':['nonexistent']},key=key,status=400)
    call(path+'/content/unknown',key=key,status=400)
    call(path+'/content/projects/test-project','PUT',{'version':version,'item':{'description':'x'*70000}},key=key,status=413)
    # Separate link versions, including the difference between unlisted and truly hidden.
    lid=uuid.uuid4().hex[:24]
    link=call(path+'/links','POST',{'id':lid,'title':'لینک','url':'https://example.com','privacy':'HIDDEN'},key=key)['data']
    assert all(l['id']!=lid for l in call(path+'/links')['data'])
    # Do not follow external redirects: assert from a raw connection.
    from http.client import HTTPConnection
    target=urllib.parse.urlsplit(BASE);conn=HTTPConnection(target.hostname,target.port);conn.request('GET','/l/'+lid);resp=conn.getresponse();assert resp.status==302;resp.read();conn.close()
    updated=call(path+'/links/'+lid,'PUT',{'version':link['version'],'visibility':'HIDDEN'},key=key)['data']
    call('/l/'+lid,status=404);call(path+'/links/'+lid,status=404)
    call(path+'/links/'+lid,'PUT',{'version':link['version'],'title':'stale'},key=key,status=409)
    call(path+'/links/'+lid,'DELETE',{'version':updated['version']},key=key)
    # Large public profile: first three per section, with explicit pagination.
    for n in range(49):
        p=call(path+'/content/projects','POST',{'version':version,'item':{'id':'large-project-'+str(n),'title':'پروژهٔ '+str(n),'visibility':'PUBLIC','description':'توضیحات '*250}},key=key)['data'];version=p['version']
    listing=call(path+'/content/projects?offset=3&limit=3')['data'];assert len(listing['items'])==3
    times=[]
    for _ in range(8):
        start=time.perf_counter();raw=call('/'+urllib.parse.quote(p['slug']));times.append((time.perf_counter()-start)*1000)
    print('PUBLIC_LATENCY_MS',json.dumps({'median':round(statistics.median(times),2),'max':round(max(times),2),'samples':len(times),'html_bytes':len(raw)}))
    # Six roles, with substantial independent/student work allowed.
    for role in ['دانشجو','طراح','پژوهشگر','مدرس','مشاور','متخصص فنی']:
        person,k=create(role);root='/api/profiles/'+person['id'];person=call(root,'PUT',{'version':person['version'],'name':'شخص آزمایشی','title':role,'isPublic':True},key=k)['data'];assert call(root)['data']['title']==role
    print('PASS advanced API assertions:',checks)
finally:
    for id,key in created:
        try:call('/api/profiles/'+id,'DELETE',key=key,status=204)
        except Exception as e:print('Fixture cleanup failed:',id,str(e))
