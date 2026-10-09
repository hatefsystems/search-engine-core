import importlib.util
import io
import json
from pathlib import Path
import tarfile
import pytest
from app import ingest as ingestion
from app.catalog import Catalog
from app.text import CONFIG
from app.ingest import ingest,atomic_json,simple_slug
spec=importlib.util.spec_from_file_location('manage',Path(__file__).resolve().parents[1]/'scripts/manage.py');manage=importlib.util.module_from_spec(spec);spec.loader.exec_module(manage)

@pytest.fixture
def sources(tmp_path,monkeypatch):
    upstream=tmp_path/'upstream';lock=json.loads((CONFIG/'sources.lock.json').read_text())
    for source in ('lucide','simple-icons','iconify'):(upstream/source).mkdir(parents=True)
    def git(args,**kw):return '' if 'status' in args else lock[Path(args[2]).name]['revision']+'\n'
    monkeypatch.setattr(ingestion.subprocess,'check_output',git)
    lucide=upstream/'lucide';(lucide/'icons').mkdir();(lucide/'LICENSE').write_text('ISC License\nCopyright fixture. Permission to use, copy, modify, and/or distribute this software is granted.')
    for name in ['briefcase','cloud']:(lucide/'icons'/f'{name}.svg').write_text('<svg viewBox="0 0 24 24"><path d="M0 0h1"/></svg>')
    simple=upstream/'simple-icons';(simple/'icons').mkdir();(simple/'_data').mkdir();(simple/'LICENSE.md').write_text('CC0 1.0 Universal fixture')
    atomic_json(simple/'_data/simple-icons.json',[{'title':'GitHub'}]);(simple/'icons/github.svg').write_text('<svg viewBox="0 0 24 24"><path d="M0 0h2"/></svg>')
    iconify=upstream/'iconify';(iconify/'json').mkdir();atomic_json(iconify/'json/lucide.json',{'prefix':'lucide','info':{'license':{'spdx':'ISC'}},'width':24,'height':24,'icons':{'cloud':{'body':'<path d="M0 0h3"/>'}},'aliases':{'cloud-alias':{'parent':'cloud'}}})
    policy=json.loads((CONFIG/'policy.json').read_text());policy_path=tmp_path/'policy.json';atomic_json(policy_path,policy)
    return upstream,policy_path

def test_resumable_atomic_ingestion_and_dedup(tmp_path,sources):
    upstream,policy=sources;root=tmp_path/'data'
    with pytest.raises(InterruptedError):ingest(root,upstream,policy,stop_after=1)
    assert not (root/'current.json').exists()
    report=ingest(root,upstream,policy);assert report['total']==4;cat=Catalog(root);assert cat.count==4
    assert cat.get('lucide:cloud')['hash']==cat.get('lucide:briefcase')['hash']
    assert cat.get('iconify:lucide:cloud-alias')['raw']['alias']['parent']=='cloud'
    assert ingest(root,upstream,policy)['version']==report['version']
    with cat.db() as db:assert db.execute('SELECT count(*) FROM search').fetchone()[0]==4

def test_license_and_brand_policy(tmp_path,sources):
    upstream,path=sources;p=json.loads(path.read_text());p['reviewedCollections']={};atomic_json(path,p)
    report=ingest(tmp_path/'first',upstream,path);assert report['total']==2
    assert report['sources']['iconify']['reasons']['original license notice needs review']==2
    p['publicBrands']=['simple-icons:github'];atomic_json(path,p);report=ingest(tmp_path/'second',upstream,path)
    assert report['total']==3;assert Catalog(tmp_path/'second').get('simple-icons:github')['brand']

def test_individual_brand_license_not_overridden(tmp_path,sources):
    upstream,path=sources;p=json.loads(path.read_text());p['publicBrands']=['simple-icons:github'];atomic_json(path,p)
    atomic_json(upstream/'simple-icons/_data/simple-icons.json',[{'title':'GitHub','license':{'type':'GPL-3.0'}}])
    report=ingest(tmp_path/'data',upstream,path);assert report['sources']['simple-icons']['reasons']['individual icon license disallowed']==1

def test_source_integrity_and_slug(tmp_path,sources,monkeypatch):
    upstream,policy=sources;monkeypatch.setattr(ingestion.subprocess,'check_output',lambda *a,**kw:'wrong-sha')
    with pytest.raises(ValueError,match='revision mismatch'):ingest(tmp_path/'data',upstream,policy)
    assert simple_slug({'title':'C++'})=='cplusplus';assert simple_slug({'title':'Name','slug':'hive_blockchain'})=='hive_blockchain'

def test_bundle_roundtrip_and_checksum(tmp_path):
    root=tmp_path/'root';root.mkdir();(root/'data').mkdir();(root/'data/a.txt').write_text('offline');archive=tmp_path/'bundle.tgz'
    manage.bundle_export(root,archive);manage.bundle_restore(archive,tmp_path/'restored',1024*1024);assert (tmp_path/'restored/data/a.txt').read_text()=='offline'
    with pytest.raises(ValueError):manage.bundle_restore(archive,tmp_path/'restored',1024*1024)
    with pytest.raises(ValueError):manage.bundle_restore(archive,tmp_path/'small',1)

@pytest.mark.parametrize('name',['../escape','/absolute','bundle/../../escape'])
def test_bundle_rejects_traversal(tmp_path,name):
    archive=tmp_path/'bad.tgz'
    with tarfile.open(archive,'w:gz') as tar:
        item=tarfile.TarInfo(name);item.size=1;tar.addfile(item,io.BytesIO(b'a'))
    with pytest.raises(ValueError):manage.bundle_restore(archive,tmp_path/'result',1024)
    assert not (tmp_path/'result').exists()

def test_bundle_rejects_symlink(tmp_path):
    archive=tmp_path/'bad.tgz'
    with tarfile.open(archive,'w:gz') as tar:
        item=tarfile.TarInfo('bundle/link');item.type=tarfile.SYMTYPE;item.linkname='/etc/passwd';tar.addfile(item)
    with pytest.raises(ValueError):manage.bundle_restore(archive,tmp_path/'result',1024)
