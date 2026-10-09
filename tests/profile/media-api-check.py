#!/usr/bin/env python3
"""Explicit isolated backend only; temporary profiles and optional Compose recreation."""
import base64
import json
import os
from pathlib import Path
import struct
import subprocess
import time
import urllib.error
import urllib.request
import uuid
import zlib

base = os.environ['PROFILE_TEST_BASE_URL'].rstrip('/')
http = urllib.request.build_opener(urllib.request.ProxyHandler({}))
profile_id = key = None


def call(path, method='GET', body=None, expected=200, owner=True):
    headers = {'Content-Type': 'application/json'}
    if owner and key:
        headers['Authorization'] = 'Bearer ' + key
    request = urllib.request.Request(base + path, method=method, headers=headers,
        data=None if body is None else json.dumps(body, ensure_ascii=False).encode())
    try:
        response = http.open(request, timeout=20)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        raw = response.read()
        assert response.status == expected, (path, response.status, raw[:400])
        if 'application/json' in response.headers.get('Content-Type', ''):
            return json.loads(raw)
        assert response.headers.get('Content-Type') == 'image/png'
        return raw


def png():
    def chunk(kind, data):
        return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!2I5B', 2, 2, 8, 2, 0, 0, 0))
        + chunk(b'IDAT', zlib.compress(b'\0\xff\0\0\0\xff\0' * 2)) + chunk(b'IEND', b''))


try:
    created = call('/api/profiles', 'POST', {
        'type': 'PERSON', 'slug': 'آزمون.تصویر.' + uuid.uuid4().hex[:12], 'name': 'ه', 'isPublic': False
    }, expected=201)
    profile_id, key = created['data']['id'], created['ownerToken']
    path = '/api/profiles/' + profile_id
    state = created['data']
    item = {'id': 'certificate-regression', 'name': 'گواهی معتبر', 'visibility': 'PUBLIC'}
    state = call(path + '/content/certifications', 'POST', {'version': state['version'], 'item': item})['data']
    item_path = path + '/content/certifications/' + item['id']
    error = call(item_path, 'PUT', {'version': state['version'], 'item': {'name': ''}}, expected=400)['error']
    assert error['code'] == 'BAD_REQUEST' and error['section'] == 'certifications'
    assert error['itemId'] == item['id'] and error['field'] == 'name'
    assert error['message'] == 'نام گواهی‌نامه را برای نمایش عمومی وارد کنید.'
    unchanged = call(path)['data']
    assert unchanged['version'] == state['version'] and unchanged['sections']['certifications'][0]['name'] == item['name']
    state = call(path + '/content/projects', 'POST', {'version': state['version'], 'item': {
        'id': 'valid-project', 'title': 'پروژه سالم', 'visibility': 'PUBLIC'
    }})['data']
    image = png()
    encoded = 'data:image/png;base64,' + base64.b64encode(image).decode()
    urls = {}
    for kind, field in [('avatar', 'avatarUrl'), ('cover', 'coverImageUrl')]:
        call(path + '/' + kind, 'POST', {'version': state['version'], 'image': encoded}, expected=403, owner=False)
        call(path + '/' + kind, 'POST', {'version': state['version'] - 1, 'image': encoded}, expected=409)
        call(path + '/' + kind, 'POST', {'version': state['version'], 'image': base64.b64encode(b'invalid').decode()}, expected=400)
        media = call(path + '/' + kind, 'POST', {'version': state['version'], 'image': encoded})['data']
        assert media['version'] == state['version'] + 1
        urls[field] = media[field]
        assert media[field].startswith('/uploads/') and call(media[field], owner=False) == image
        state = call(path)['data']
        assert state['sections']['certifications'][0]['name'] == item['name']
    # Force a database rejection for only this temporary profile, proving the
    # newly written file is removed when its URL cannot be committed.
    if os.environ.get('PROFILE_TEST_RECREATE_APP') == '1':
        assert base == 'http://127.0.0.1:3019'
        def compose(*args):
            return subprocess.check_output(['docker', 'compose', '-f', 'docker-compose.profile-test.yml', *args],
                cwd=Path(__file__).resolve().parents[2], text=True).strip()
        def mongo(script):
            return json.loads(compose('exec', '-T', 'mongo', 'mongosh', '--quiet', 'search-engine', '--eval', script))
        options = mongo('JSON.stringify(db.getCollectionInfos({name:"profiles"})[0].options)')
        previous_files = compose('exec', '-T', 'app', 'find', '/app/uploads/avatars', '-name', profile_id + '_*')
        rule = '{"$or":[{"_id":{"$ne":ObjectId(' + json.dumps(profile_id) + ')}},{"avatarUrl":' + json.dumps(urls['avatarUrl']) + '}]}'
        try:
            result = mongo('JSON.stringify(db.runCommand({collMod:"profiles",validationLevel:"strict",validationAction:"error",validator:' + rule + '}))')
            assert result['ok'] == 1
            call(path + '/avatar', 'POST', {'version': state['version'], 'image': encoded}, expected=500)
            assert compose('exec', '-T', 'app', 'find', '/app/uploads/avatars', '-name', profile_id + '_*') == previous_files
            assert call(path)['data']['avatarUrl'] == urls['avatarUrl']
        finally:
            restore = {'collMod': 'profiles', 'validator': options.get('validator', {}),
                'validationLevel': options.get('validationLevel', 'strict'), 'validationAction': options.get('validationAction', 'error')}
            assert mongo('JSON.stringify(db.runCommand(' + json.dumps(restore) + '))')['ok'] == 1
    # Replace a picture, then recreate only the isolated application container.
    previous = urls['avatarUrl']
    media = call(path + '/avatar', 'POST', {'version': state['version'], 'image': encoded})['data']
    urls['avatarUrl'] = media['avatarUrl']
    assert urls['avatarUrl'] != previous
    state = call(path)['data']
    if os.environ.get('PROFILE_TEST_RECREATE_APP') == '1':
        assert base == 'http://127.0.0.1:3019', 'Recreation requires the isolated profile-test Compose target.'
        subprocess.run(['docker', 'compose', '-f', 'docker-compose.profile-test.yml', 'up', '-d',
            '--no-deps', '--no-build', '--force-recreate', 'app'], cwd=Path(__file__).resolve().parents[2], check=True)
        for attempt in range(60):
            try:
                state = call(path)['data']
                break
            except (OSError, AssertionError):
                if attempt == 59:
                    raise
                time.sleep(1)
        for field, url in urls.items():
            assert state[field] == url and call(url, owner=False) == image
    for field in urls:
        state = call(path, 'PUT', {'version': state['version'], field: ''})['data']
        assert not state.get(field)
    call('/uploads/covers/missing-regression.png', expected=404, owner=False)
    print('Passed: structured validation, unchanged public item, independent media/content, upload/replace/delete, invalid image/auth/version, and volume persistence' if os.environ.get('PROFILE_TEST_RECREATE_APP') == '1' else 'Passed: media API and structured validation regressions')
finally:
    if profile_id and key:
        call('/api/profiles/' + profile_id, 'DELETE')
