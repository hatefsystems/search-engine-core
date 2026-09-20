"""Canonical slug routing tests against explicit isolated test targets only."""
import datetime
import json
import os
import uuid
from urllib.error import HTTPError
from urllib.parse import quote
from urllib.request import Request, build_opener, HTTPRedirectHandler, ProxyHandler
from bson import ObjectId
from pymongo import MongoClient

class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs): return None

def main():
    base = os.environ['PROFILE_TEST_BASE_URL'].rstrip('/')
    collection = MongoClient(os.environ['PROFILE_TEST_MONGODB_URI'])[os.environ['PROFILE_TEST_DATABASE']].profiles
    opener = build_opener(ProxyHandler({}), NoRedirect())
    suffix = uuid.uuid4().hex[:10]
    slugs = ['هاتف.رستمخانی.'+suffix, 'دانشگاه.تهران.'+suffix, 'legacy-only-'+suffix]
    ids = [ObjectId() for _ in slugs]
    created = []
    def request(path, method='GET', data=None, accept='text/html'):
        headers = {'Accept':accept, 'Origin':base, 'Content-Type':'application/json'}
        try:
            response = opener.open(Request(base+path,method=method,headers=headers,data=json.dumps(data,ensure_ascii=False).encode() if data is not None else None),timeout=15)
        except HTTPError as e: response = e
        with response: return response.status,response.headers,response.read().decode()
    try:
        for i,slug in enumerate(slugs):
            collection.insert_one({'_id':ids[i],'slug':slug,'name':'نام آزمایشی','type':'BUSINESS' if i==1 else 'PERSON','isPublic':True,'createdAt':datetime.datetime.now(datetime.timezone.utc)})
        for slug in slugs[:2]:
            for prefix in ['/', '/profiles/']:
                assert request(prefix+quote(slug))[0]==200
                assert request(prefix+quote(slug).replace('.', '%2E'))[0]==200
                for separator in ['-', '_']:
                    legacy=slug.replace('.',separator)
                    for accept in ['text/html','application/json']:
                        status,headers,_=request(prefix+quote(legacy),accept=accept)
                        assert status==301,(legacy,status)
                        assert headers['Location']=='/'+quote(slug)
                        assert headers['Server']=='HatefEngine 1.0'
            status,headers,_=request('/profiles/'+quote(slug.replace('.', '_'))+'/edit')
            assert status==301 and headers['Location']=='/profiles/'+quote(slug)+'/edit'
        # When no dotted counterpart exists, an existing legacy profile remains readable.
        assert request('/'+slugs[2])[0]==200
        for invalid in ['.هاتف','هاتف.','هاتف..رستمخانی','هاتف.-رستمخانی','هاتف-.رستمخانی','هاتف_.رستمخانی','هاتف._رستمخانی','a..b','..','.','علی،رضا','علیَ','ه'*101]:
            for prefix in ['/', '/profiles/']:
                assert request(prefix+quote(invalid))[0] in [400,404],invalid
            assert request('/api/profiles','POST',{'slug':invalid,'name':'','isPublic':False})[0]==400,invalid
        for invalid in ['%2E','%2E%2E','a%2E%2Eb','a%252Eb','bad%2Fslug','%FF']:
            assert request('/'+invalid)[0] in [400,404],invalid
        status,_,_=request('/'+quote('ه'*100))
        assert status==404 # Valid, but absent; not rejected as 200 bytes.
        for legacy in ['new-name-'+suffix,'new_name_'+suffix]:
            canonical=legacy.replace('-', '.').replace('_','.')
            status,_,body=request('/api/profiles','POST',{'slug':legacy,'name':'','isPublic':False},'application/json')
            if status==201:
                result=json.loads(body);created.append(ObjectId(result['data']['id']));assert result['data']['slug']==canonical
            else: assert status==409 # Both forms claim the same canonical identifier.
        # Static assets bypass the profile grammar and keep the correct MIME type.
        for asset,mime in [('/assets/css/profile-header.css','text/css'),('/assets/js/profile-editor.js','application/javascript'),('/assets/fonts/vazirmatn/Vazirmatn-FD-font-face.css','text/css')]:
            status,headers,_=request(asset);assert status==200 and mime in headers['Content-Type'],asset
        assert request('/')[0]==200
        assert request('/api/profiles',accept='application/json')[0]==200
        assert request('/api')[0]==404
        print('Canonical slugs passed: dot rules, Unicode length, encoded URLs, person/business 301s, legacy fallback, creation normalization, static routes.')
    finally: collection.delete_many({'_id':{'$in':ids+created}})

if __name__=='__main__': main()
