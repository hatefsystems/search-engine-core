"""Verify reindex only against explicitly isolated Docker/Mongo fixtures."""
import json
import os
import subprocess
import urllib.request
import uuid
from pymongo import MongoClient

base = os.environ['PROFILE_TEST_BASE_URL']
container = os.environ['PROFILE_TEST_CONTAINER']
profiles = MongoClient(os.environ['PROFILE_TEST_MONGODB_URI'])[os.environ['PROFILE_TEST_DATABASE']].profiles
opener = urllib.request.build_opener(urllib.request.ProxyHandler({}))
request = urllib.request.Request(base + '/api/profiles', data=json.dumps({
    'type': 'PERSON', 'slug': 'reindex.' + uuid.uuid4().hex,
    'name': 'آزمون بازسازی', 'isPublic': False,
}).encode(), headers={'Content-Type': 'application/json'}, method='POST')
with opener.open(request) as response:
    result = json.load(response)
profile_id = result['data']['id']
from bson import ObjectId
selector = {'_id': ObjectId(profile_id)}
try:
    request = urllib.request.Request(base + '/api/profiles/' + profile_id,
        data=json.dumps({'version': result['data']['version'], 'isPublic': True}).encode(),
        headers={'Content-Type': 'application/json', 'Authorization': 'Bearer ' + result['ownerToken']}, method='PUT')
    with opener.open(request) as response:
        assert response.status == 200
    profiles.update_one(selector, {'$unset': {'publicSearch': ''}})
    before = profiles.find_one(selector)
    def reindex(mode):
        subprocess.run(['docker', 'exec', container, '/app/server', '--profiles-reindex', mode],
                       check=True, timeout=60, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    reindex('--dry-run')
    assert profiles.find_one(selector) == before, 'dry-run modified the document'
    reindex('--apply')
    after = profiles.find_one(selector)
    assert after.get('publicSearch'), 'apply did not derive public search'
    assert {k: v for k, v in after.items() if k != 'publicSearch'} == before
    reindex('--apply')
    assert profiles.find_one(selector) == after, 'reindex was not idempotent'
    print('PASS isolated reindex: dry-run unchanged, derived field only, stable version/content/key/slug, repeat apply unchanged.')
finally:
    request = urllib.request.Request(base + '/api/profiles/' + profile_id,
        headers={'Authorization': 'Bearer ' + result['ownerToken']}, method='DELETE')
    with opener.open(request) as response:
        assert response.status == 204
