"""E5 local CPU inference and resumable memory-mapped vector shards."""
import fcntl
import functools
import hashlib
import json
import os
import sqlite3
import threading
from pathlib import Path
import numpy as np
from .text import CONFIG, VERSION, normalize
from .ingest import atomic_json, digest

class E5:
    def __init__(self,directory,threads=1):
        import onnxruntime as ort
        from tokenizers import Tokenizer
        root=Path(directory);self.spec=json.loads((root/'manifest.json').read_text());locked=json.loads((CONFIG/'sources.lock.json').read_text())['model']
        if self.spec['revision']!=locked['revision'] or self.spec['repository']!=locked['repository']: raise ValueError('model revision mismatch')
        if self.spec.get('parentChecksums',self.spec['checksums'])!=locked['checksums']: raise ValueError('unapproved model checksums')
        for name,sha in self.spec['checksums'].items():
            if Path(name).name!=name or digest(root/name)!=sha: raise ValueError('model integrity mismatch')
        self.tokenizer=Tokenizer.from_file(str(root/'tokenizer.json'));self.tokenizer.enable_truncation(max_length=512);self.tokenizer.enable_padding(pad_id=1,pad_token='<pad>')
        opts=ort.SessionOptions();opts.intra_op_num_threads=max(1,min(threads,4));opts.inter_op_num_threads=1;opts.execution_mode=ort.ExecutionMode.ORT_SEQUENTIAL;opts.enable_cpu_mem_arena=False
        self.session=ort.InferenceSession(str(root/'model.onnx'),opts,providers=['CPUExecutionProvider']);self.lock=threading.Lock()
        self.version=hashlib.sha256(json.dumps([self.spec,VERSION,1,(CONFIG/'synonyms.json').read_text()],sort_keys=True).encode()).hexdigest()
    def encode(self,texts,query=False):
        with self.lock:
            enc=self.tokenizer.encode_batch([('query: ' if query else 'passage: ')+normalize(t) for t in texts])
            ids=np.array([x.ids for x in enc],dtype=np.int64);mask=np.array([x.attention_mask for x in enc],dtype=np.int64)
            inputs={'input_ids':ids,'attention_mask':mask,'token_type_ids':np.zeros_like(ids)}
            output=self.session.run(None,{x.name:inputs[x.name] for x in self.session.get_inputs()})[0]
            if output.ndim!=3: raise ValueError('token embeddings required')
            pooled=(output*mask[:,:,None]).sum(1)/np.maximum(mask.sum(1,keepdims=True),1)
            pooled/=np.maximum(np.linalg.norm(pooled,axis=1,keepdims=True),1e-12)
            return pooled.astype(np.float32)
    @functools.lru_cache(maxsize=128)
    def query(self,text): return self.encode([text],True)[0]

class Vectors:
    def __init__(self,root,catalog,model):
        self.root=Path(root);spec=json.loads((self.root/'vectors.json').read_text())
        if spec['embeddingVersion']!=model.version or spec['catalogVersion']!=catalog.manifest['version']: raise ValueError('incompatible vectors')
        self.shards=spec['shards'];self.count=spec['count']
        for shard in self.shards:
            for key in ('vectors','ids'):
                if Path(shard[key]).name!=shard[key]: raise ValueError('unsafe vector path')
    def nearest(self,vector,limit=100):
        best=[]
        for shard in self.shards:
            matrix=np.load(self.root/shard['vectors'],mmap_mode='r',allow_pickle=False);ids=json.loads((self.root/shard['ids']).read_text());scores=matrix@vector
            if matrix.shape!=(len(ids),384): raise ValueError('invalid vector dimensions')
            top=np.argsort(-scores,kind='stable')[:limit];best.extend((ids[i],float(scores[i])) for i in top)
            best=sorted(best,key=lambda x:(-x[1],x[0]))[:limit]
        return best

def build(root,catalog,model,batch=8,priority=None,max_icons=None):
    root=Path(root);root.mkdir(parents=True,exist_ok=True)
    with (root/'.index.lock').open('w') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        return _build(root,catalog,model,batch,priority,max_icons)

def _build(root,catalog,model,batch,priority,max_icons):
    spec={'embeddingVersion':model.version,'catalogVersion':catalog.manifest['version'],'count':0,'shards':[]};manifest=root/'vectors.json'
    if manifest.exists():
        spec=json.loads(manifest.read_text())
        if spec['embeddingVersion']!=model.version or spec['catalogVersion']!=catalog.manifest['version']: raise ValueError('use a new vector directory')
    checkpoint=sqlite3.connect(root/'checkpoint.sqlite');checkpoint.execute('CREATE TABLE IF NOT EXISTS done(id TEXT PRIMARY KEY)');checkpoint.execute('DELETE FROM done')
    for shard in spec['shards']: checkpoint.executemany('INSERT OR IGNORE INTO done VALUES(?)',((i,) for i in json.loads((root/shard['ids']).read_text())))
    checkpoint.commit();rows=[];added=0
    def flush(rows):
        nonlocal added
        name=f"shard-{len(spec['shards']):06d}";texts=[r['search_text'] for r in rows]
        matrix=np.concatenate([model.encode(texts[i:i+batch]) for i in range(0,len(texts),batch)])
        np.save(root/(name+'.tmp.npy'),matrix);os.replace(root/(name+'.tmp.npy'),root/(name+'.npy'))
        atomic_json(root/(name+'.json'),[r['id'] for r in rows]);spec['shards'].append({'vectors':name+'.npy','ids':name+'.json'});spec['count']+=len(rows);atomic_json(manifest,spec)
        checkpoint.executemany('INSERT OR IGNORE INTO done VALUES(?)',((r['id'],) for r in rows));checkpoint.commit();added+=len(rows)
        print(json.dumps({'indexed':spec['count'],'catalog':catalog.count}),flush=True)
    with catalog.db() as db:
        for row in db.execute('SELECT * FROM icons ORDER BY (category=?) DESC,id',(priority or '',)):
            if checkpoint.execute('SELECT 1 FROM done WHERE id=?',(row['id'],)).fetchone(): continue
            rows.append(row)
            if len(rows)>=512 or (max_icons and added+len(rows)>=max_icons):
                flush(rows);rows=[]
                if max_icons and added>=max_icons: break
        if rows: flush(rows)
    checkpoint.close();return spec
