#!/usr/bin/env python3
"""Verify public legal routes against the real C++ server, without credentials."""
import os
import urllib.error
import urllib.parse
import urllib.request

BASE = os.environ.get('BASE_URL', 'http://127.0.0.1:3000').rstrip('/')
PAGES = {'privacy': {'fa': '/حریم-خصوصی', 'en': '/privacy'},
         'terms': {'fa': '/شرایط-استفاده', 'en': '/terms'}}

def get(path):
    url = BASE + urllib.parse.quote(path, safe='/%?=&')
    request = urllib.request.Request(url, headers={'Accept-Language': 'en-US,fa;q=0.5'})
    return urllib.request.urlopen(request, timeout=20)

for document, languages in PAGES.items():
    for language, path in languages.items():
        with get(path) as response:
            body = response.read().decode('utf-8')
            assert response.status == 200, path
            assert response.headers['Content-Language'] == language, path
            assert 'text/html' in response.headers['Content-Type'], path
            assert response.headers['X-Content-Type-Options'] == 'nosniff'
            assert "frame-ancestors 'none'" in response.headers['Content-Security-Policy']
            assert not response.headers.get('Set-Cookie'), path
        assert f'<html lang="{language}" dir="{"rtl" if language == "fa" else "ltr"}">' in body
        assert f'rel="canonical" href="https://hatef.ir{path}"' in body
        assert 'href="/assets/legal.css?v=readability-1"' in body
        assert 'Google' in body
        if document == 'privacy':
            assert 'gmail.send' in body and 'contacts.readonly' in body
        # Query parameters and request language must never select a different document.
        with get(path + '?lang=' + ('fa' if language == 'en' else 'en')) as response:
            assert response.read().decode('utf-8') == body
        if language == 'fa':
            with get(urllib.parse.quote(path).lower()) as response:
                assert response.read().decode('utf-8') == body
        print(f'PASS public {document} {language}: status, language, headers, canonical, query independence')

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None

for name in PAGES:
    try:
        urllib.request.build_opener(NoRedirect).open(BASE + '/' + name + '.html', timeout=10)
        raise AssertionError('Expected permanent redirect')
    except urllib.error.HTTPError as error:
        assert error.code == 301
        assert error.headers['Location'] == '/' + name
    print('PASS legacy redirect:', name)

for path, mime in [('/assets/legal.css?v=readability-1', 'text/css'),
                   ('/assets/css/typography.css?v=readability-1', 'text/css'),
                   ('/assets/fonts/vazirmatn/fonts/webfonts/Vazirmatn-FD-Regular.woff2', 'font/woff2')]:
    with get(path) as response:
        assert response.status == 200 and response.read()
        assert mime in response.headers['Content-Type'], (path, response.headers)
    print('PASS asset:', path)
