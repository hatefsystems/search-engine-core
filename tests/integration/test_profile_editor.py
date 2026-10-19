"""Creation/editing integration against explicit, isolated test targets only.
Run with PROFILE_TEST_BASE_URL, PROFILE_TEST_MONGODB_URI, PROFILE_TEST_DATABASE.
Requires pymongo. Only uniquely named test documents are removed.
"""
import base64
import concurrent.futures
import datetime
import hashlib
import json
import os
import uuid
from http.cookiejar import CookieJar
from urllib.error import HTTPError
from urllib.parse import quote
from urllib.request import Request, build_opener, HTTPCookieProcessor, ProxyHandler
from bson import ObjectId
from pymongo import MongoClient


def main():
    base = os.environ['PROFILE_TEST_BASE_URL'].rstrip('/')
    db = MongoClient(os.environ['PROFILE_TEST_MONGODB_URI'])[os.environ['PROFILE_TEST_DATABASE']]
    prefix = 'آزمون.ویرایش.' + uuid.uuid4().hex[:10]
    created_ids = []
    cookiejar = CookieJar()
    owner = build_opener(ProxyHandler({}), HTTPCookieProcessor(cookiejar))
    guest = build_opener(ProxyHandler({}))

    def request(path, method='GET', data=None, opener=guest, headers=None):
        h = {'Accept':'application/json', **(headers or {})}
        if data is not None: h['Content-Type'] = 'application/json'
        try:
            response = opener.open(Request(base+path, method=method, headers=h,
                data=json.dumps(data,ensure_ascii=False).encode() if data is not None else None),timeout=15)
        except HTTPError as error: response = error
        with response:
            text = response.read().decode()
            assert response.headers.get('Server') == 'HatefEngine 1.0', (path,response.status)
            return response.status, response.headers, json.loads(text) if 'application/json' in response.headers.get('Content-Type','') else text

    def create(slug, opener=owner):
        status,headers,result = request('/api/profiles','POST',{'slug':slug,'type':'PERSON','name':'','isPublic':False},opener,{'Origin':base})
        if status==201: created_ids.append(ObjectId(result['data']['id']))
        return status,headers,result

    try:
        for path in ['/'+quote(prefix), '/profiles/'+quote(prefix)]:
            status,headers,html=request(path,headers={'Accept':'text/html'})
            assert status==404 and 'ساخت این صفحه' in html and 'noindex' in html
            assert 'no-store' in headers['Cache-Control']
        assert db.profiles.count_documents({'slug':prefix})==0
        assert request('/profiles/new?slug='+quote(prefix),headers={'Accept':'text/html'})[0]==200
        assert db.profiles.count_documents({'slug':prefix})==0
        assert request('/profiles/new?slug=%25D9%2587')[0]==400
        assert request('/api/profiles','POST',{'slug':prefix,'name':'نام','isPublic':True},headers={'Origin':base})[0]==400
        status,headers,result=create(prefix)
        assert status==201,(status,result)
        p=result['data']; key=result['ownerToken']; pid=p['id']; endpoint='/api/profiles/'+pid
        assert len(key)==64 and all(c in '0123456789abcdef' for c in key)
        assert not p['isPublic'] and p['version']==0 and p['name']==''
        cookie=headers['Set-Cookie']; assert 'HttpOnly' in cookie and 'SameSite=Strict' in cookie and 'Max-Age=2592000' in cookie
        stored=db.profiles.find_one({'_id':ObjectId(pid)})
        assert stored['ownerTokenHash']==hashlib.sha256(key.encode()).hexdigest() and 'ownerToken' not in stored
        assert key not in json.dumps(p)
        assert request(endpoint)[0]==403
        assert all(item['id']!=pid for item in request('/api/profiles')[2]['data'])
        status,_,html=request('/'+quote(prefix),headers={'Accept':'text/html'})
        assert status==403 and 'ورود برای ویرایش' in html and 'ساخت این صفحه' not in html
        assert create(prefix)[0]==409
        assert request(endpoint,opener=owner)[2]['canEdit']
        assert request(endpoint,'PUT',{'name':'ه','version':0})[0]==403
        assert request(endpoint,'PUT',{'name':'ه','version':0},owner,{'Origin':'https://other.example'})[0]==403
        assert request(endpoint,'PUT',{'name':'ه','version':0},owner)[0]==403
        status,_,response=request(endpoint,'PUT',{'name':'ه','version':0},owner,{'Origin':base})
        assert status==200,(status,response)
        assert request(endpoint,opener=owner)[2]['data']['name']=='ه'
        version=response['data']['version']; assert version==1
        # Existing fields outside the form must survive updates.
        db.profiles.update_one({'_id':ObjectId(pid)},{'$set':{'englishName':'Legacy English','privacy':{'showEmail':False,'showPhone':False,'showLocation':False,'showAvailability':False}}})
        for patch in [{'bio':'معرفی','company':'شرکت','skillsWithLevel':[{'name':'C++','level':'EXPERT'}]}, {'bio':'','company':'','skillsWithLevel':[]}, {'bio':''}]:
            status,_,response=request(endpoint,'PUT',{**patch,'version':version},owner,{'Origin':base})
            assert status==200,(status,response);version=response['data']['version']
        stored=db.profiles.find_one({'_id':ObjectId(pid)})
        assert stored['bio']=='' and stored['company']=='' and stored['skillsWithLevel']==[] and stored['skills']==[]
        assert stored['englishName']=='Legacy English' and not stored['privacy']['showLocation']
        assert request(endpoint,'PUT',{'name':'دیگر','version':0},owner,{'Origin':base})[0]==409
        for forbidden in ['ownerToken','ownerTokenHash','ownerId','englishName','privacy','slug']:
            assert request(endpoint,'PUT',{forbidden:'bad','version':version},owner,{'Origin':base})[0]==400
        assert request(endpoint,'PUT',{'name':'English','version':version},owner,{'Origin':base})[0]==400
        assert request(endpoint,'PUT',{'name':'','isPublic':True,'version':version},owner,{'Origin':base})[0]==400
        status,_,response=request(endpoint,'PUT',{'isPublic':True,'version':version},owner,{'Origin':base})
        assert status==200;version=response['data']['version']
        assert request('/'+quote(prefix))[2]['data']['name']=='ه'
        status,_,response=request(endpoint,'PUT',{'name':'نام تازه','version':version},owner,{'Origin':base})
        assert status==200;version=response['data']['version']
        assert request('/'+quote(prefix))[2]['data']['name']=='نام تازه'
        # Key login in a different browser; missing/wrong/other-profile credentials never authorize.
        other = build_opener(ProxyHandler({}),HTTPCookieProcessor(CookieJar()))
        assert request(endpoint+'/session','POST',{'key':'wrong'},other,{'Origin':base})[0]==403
        assert request(endpoint+'/session','POST',{'key':pid},other,{'Origin':base})[0]==403
        _,_,another=create(prefix+'.دیگر'); other_key=another['ownerToken']
        assert request(endpoint+'/session','POST',{'key':other_key},other,{'Origin':base})[0]==403
        assert request(endpoint+'/session','POST',{'key':key},other,{'Origin':'https://other.example'})[0]==403
        assert request(endpoint+'/session','POST',{'key':key},other,{'Origin':base})[0]==200
        assert request(endpoint,opener=other)[2]['canEdit']
        # Bearer still works; uploads require ownership and version.
        bearer={'Authorization':'Bearer '+key}
        assert request(endpoint,headers=bearer)[2]['canEdit']
        png=base64.b64encode(bytes.fromhex('89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c4890000000b49444154789c636000020000050001a5f645400000000049454e44ae426082')).decode()
        for kind in ['avatar','cover']:
            assert request(endpoint+'/'+kind,'POST',{'image':png,'version':version},headers={'Authorization':'Bearer '+other_key})[0]==403
            assert request(endpoint+'/'+kind,'POST',{'image':png,'version':0},headers=bearer)[0]==409
            status,_,response=request(endpoint+'/'+kind,'POST',{'image':png,'version':version},headers=bearer)
            assert status==200,(kind,status,response);version=response['data']['version']
        assert request(endpoint+'/skills','POST',{'skills':[{'name':'C++'}],'version':version})[0]==403
        assert request(endpoint+'/session','DELETE',opener=other,headers={'Origin':base})[0]==200
        assert not request(endpoint,opener=other)[2]['canEdit']
        # Concurrent claims rely on the database unique index.
        with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
            results=list(pool.map(lambda _:create(prefix+'.رقابت',guest),range(2)))
        assert sorted(item[0] for item in results)==[201,409]
        assert db.profiles.count_documents({'slug':prefix+'.رقابت'})==1
        # Legacy token and version-less documents remain editable.
        legacy_id=ObjectId();created_ids.append(legacy_id)
        db.profiles.insert_one({'_id':legacy_id,'slug':prefix+'.قدیمی','type':'PERSON','name':'قدیمی','isPublic':False,'ownerToken':'legacy-test-token','createdAt':datetime.datetime.now(datetime.timezone.utc)})
        assert request('/api/profiles/'+str(legacy_id),'PUT',{'bio':'جدید','version':0},headers={'Authorization':'Bearer legacy-test-token'})[0]==200
        # Per-owner limit applies after authentication, independently of the global API limit.
        rate_id=another['data']['id']; rate_endpoint='/api/profiles/'+rate_id
        for i in range(120):
            assert request(rate_endpoint,'PUT',{'version':i,'bio':''},headers={'Authorization':'Bearer '+other_key})[0]==200
        status,headers,_=request(rate_endpoint,'PUT',{'version':120,'bio':''},headers={'Authorization':'Bearer '+other_key})
        assert status==429 and int(headers['Retry-After'])>0
        print('Profile editor integration passed: draft, cookies, ownership, clearing, publish, CAS, uploads, race, legacy key, rate limit.')
    finally:
        db.profiles.delete_many({'_id':{'$in':created_ids}})

if __name__=='__main__': main()
