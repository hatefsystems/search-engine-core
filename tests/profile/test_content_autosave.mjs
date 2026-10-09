import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const dataURL=source=>`data:text/javascript;base64,${Buffer.from(source).toString('base64')}`;
const root=new URL('../../public/assets/js/',import.meta.url);
const autosave=dataURL(await readFile(new URL('profile-autosave.js',root),'utf8'));
const ui=dataURL(await readFile(new URL('profile-content-ui.js',root),'utf8'));
const source=(await readFile(new URL('profile-content-editor.js',root),'utf8')).replace(/(['"])\.\/profile-autosave\.js(?:\?[^\x22\x27]+)?\1/,JSON.stringify(autosave)).replace(/(['"])\.\/profile-content-ui\.js\1/,JSON.stringify(ui));
const {ProfileContentEditor}=await import(dataURL(source));
function fixture(server,api){
    const values=new Map();globalThis.localStorage={setItem:(k,v)=>values.set(k,v),getItem:k=>values.get(k),removeItem:k=>values.delete(k)};
    globalThis.document={getElementById:()=>({})};globalThis.window={addEventListener:()=>{}};
    let version=server.version,tail=Promise.resolve();const errors=[];
    const editor=new ProfileContentEditor({id:'test',draftId:'tab',version:()=>version,api,
        mutate:operation=>{const task=tail.then(async()=>{const result=await operation(version);version=result.data.version;return result;});tail=task.catch(()=>{});return task;},
        accept:data=>{version=data.version;},status:()=>{},conflict:()=>errors.push('conflict'),notice:()=>{}});
    editor.server=structuredClone(server);editor.renderPreview=()=>{};editor.completion=()=>{};editor.rebuild();
    return {editor,values,errors};
}
test('an existing item sends only edited fields, preserving concurrent unrelated values after reconciliation',async()=>{
    const sent=[];const initial={version:1,sections:{projects:[{id:'project-one',title:'old',role:'role',architecture:''}]}};
    const {editor}=fixture(initial,async(path,method,body)=>{sent.push(body);return {data:{version:3,sections:{projects:[{...initial.sections.projects[0],role:'other tab',...body.item}]}}};});
    editor.change('projects',{...initial.sections.projects[0],architecture:'local'});
    assert.deepEqual(editor.queue.pending['projects/project-one'].item,{id:'project-one',architecture:'local'});
    editor.server={version:2,sections:{projects:[{...initial.sections.projects[0],role:'other tab'}]}};
    await editor.flush();assert.equal(sent[0].item.role,undefined);assert.equal(editor.sections.projects[0].role,'other tab');editor.queue.stop();
});
test('new item creation has a stable ID and each operation uses the last acknowledged version',async()=>{
    const calls=[];let state={version:7,sections:{}};
    const {editor}=fixture(state,async(path,method,body)=>{calls.push({path,method,body});const pieces=path.split('/');const section=method==='POST'?pieces.at(-1):pieces.at(-2);state={version:body.version+1,sections:{...state.sections,[section]:[{...(state.sections[section]?.[0]||{}),...body.item}]}};return {data:state};});
    editor.change('projects',{id:'stable-project',title:'ه',visibility:'HIDDEN'});
    editor.change('skills',{id:'stable-skill',name:'C++',visibility:'HIDDEN'});
    await editor.flush();assert.deepEqual(calls.map(c=>c.body.version),[7,8,9,10]);assert.equal(calls[0].body.item.id,'stable-project');assert.equal(calls[0].method,'POST');assert.deepEqual(editor.queue.pending,{});editor.queue.stop();
});
test('an unsuccessful response keeps the unacknowledged item in local storage',async()=>{
    const {editor,values}=fixture({version:1,sections:{}},async()=>{throw Object.assign(new Error('offline'),{status:503});});
    editor.change('projects',{id:'stable-project',title:'ه',visibility:'HIDDEN'});
    assert.equal(await editor.flush(),false);const saved=JSON.parse(values.get(editor.queue.key));assert.equal(saved.fields['projects/stable-project'].item.title,'ه');editor.queue.stop();
});
test('a late acknowledgment never clears a newer edit',async()=>{
    let release;const calls=[];let state={version:1,sections:{projects:[{id:'project-one',title:'old'}]}};
    const {editor}=fixture(state,(path,method,body)=>{calls.push(body);return new Promise(resolve=>{release=()=>{state={version:body.version+1,sections:{projects:[{...state.sections.projects[0],...body.item}]}};resolve({data:state});};});});
    editor.change('projects',{id:'project-one',title:'first'});const flight=editor.flush();await new Promise(setImmediate);
    editor.change('projects',{id:'project-one',title:'latest'});release();await new Promise(setImmediate);assert.equal(calls.length,2);assert.equal(calls[1].item.title,'latest');release();await flight;assert.equal(editor.sections.projects[0].title,'latest');editor.queue.stop();
});
test('a stale version requests reconciliation without deleting local edits',async()=>{
    const {editor,errors,values}=fixture({version:2,sections:{}},async()=>{throw Object.assign(new Error('stale'),{status:409});});
    editor.change('projects',{id:'stable-project',title:'local'});assert.equal(await editor.flush(),false);assert.deepEqual(errors,['conflict']);assert.ok(values.get(editor.queue.key));assert.equal(editor.queue.blocked,true);editor.queue.stop();
});
test('reverting a field while an earlier value is in flight persists the reversion',async()=>{
    let release;const calls=[];let state={version:1,sections:{projects:[{id:'project-one',title:'original'}]}};
    const {editor}=fixture(state,(path,method,body)=>{calls.push(body);return new Promise(resolve=>{release=()=>{state={version:body.version+1,sections:{projects:[{...state.sections.projects[0],...body.item}]}};resolve({data:state});};});});
    editor.change('projects',{id:'project-one',title:'temporary'});const flight=editor.flush();await new Promise(setImmediate);
    editor.change('projects',{id:'project-one',title:'original'});release();await new Promise(setImmediate);assert.equal(calls[1].item.title,'original');release();await flight;assert.equal(editor.sections.projects[0].title,'original');editor.queue.stop();
});
test('new mutually linked items are reserved before references or featured layout are written',async()=>{
    const calls=[];let state={version:0,sections:{}};
    const {editor}=fixture(state,async(path,method,body)=>{
        const parts=path.split('/');calls.push({path,method,body});
        if(parts.at(-1)==='layout') {assert.ok(state.sections.projects.some(i=>i.id===body.featured[0].id));return {data:{...state,version:++state.version}};}
        const section=method==='POST'?parts.at(-1):parts.at(-2);
        for(const [key,target] of Object.entries({projectIds:'projects',skillIds:'skills'}))for(const id of body.item[key]||[])assert.ok(state.sections[target]?.some(i=>i.id===id));
        state.sections[section]=[{...(state.sections[section]?.[0]||{}),...body.item}];state.version++;return {data:structuredClone(state)};
    });
    editor.setLayout({featured:[{section:'projects',id:'new-project'}]});
    editor.change('projects',{id:'new-project',title:'Project',skillIds:['new-skill'],visibility:'PUBLIC'});
    editor.change('skills',{id:'new-skill',name:'C++',projectIds:['new-project'],visibility:'PUBLIC'});
    assert.equal(await editor.flush(),true);assert.deepEqual(calls.slice(0,2).map(c=>c.method),['POST','POST']);assert.ok(calls.at(-1).path.endsWith('/layout'));editor.queue.stop();
});

function memoryAPI(initial) {
    let state=structuredClone(initial); const calls=[];
    return {calls, get state(){return structuredClone(state);}, async api(path,method,body) {
        calls.push({path,method,body:structuredClone(body)});
        assert.equal(body.version,state.version);
        const parts=path.split('/'), section=method==='POST'?parts.at(-1):parts.at(-2);
        const entries=state.sections[section]||=[], index=entries.findIndex(i=>i.id===body.item.id);
        const item={...(entries[index]||{}),...body.item};
        if(item.visibility==='PUBLIC' && !(item.name||item.title)?.trim())
            throw Object.assign(new Error('نام گواهی‌نامه را برای نمایش عمومی وارد کنید.'),{status:400,field:'name'});
        if(index<0)entries.push(item);else entries[index]=item;
        state.sections[section]=entries;state.version++;
        return {data:structuredClone(state)};
    }};
}
test('validation failure isolates an item, acknowledges successes and retries only after correction',async()=>{
    const initial={version:1,sections:{certifications:[{id:'bad',name:'valid',visibility:'PUBLIC'}],projects:[{id:'good',title:'old'}]}};
    const backend=memoryAPI(initial), {editor,values}=fixture(initial,backend.api);
    editor.change('certifications',{id:'bad',name:'',visibility:'PUBLIC'});
    editor.change('projects',{id:'good',title:'saved'});
    assert.equal(await editor.flush(),false);
    assert.equal(backend.state.sections.projects[0].title,'saved');
    assert.equal(backend.state.sections.certifications[0].name,'valid');
    assert.deepEqual(Object.keys(editor.queue.pending),['certifications/bad']);
    assert.equal(editor.queue.errors['certifications/bad'].field,'name');
    const persisted=JSON.parse(values.get(editor.queue.key));assert.equal(persisted.version,2);
    assert.equal(persisted.errors['certifications/bad'].field,'name');
    assert.equal(await editor.flush(),false);assert.equal(backend.calls.length,2);
    editor.change('certifications',{id:'bad',name:'corrected',visibility:'PUBLIC'});
    assert.equal(await editor.flush(),true);assert.equal(backend.calls.length,3);
    assert.deepEqual(editor.queue.errors,{});assert.equal(values.size,0);editor.queue.stop();
});
test('acknowledged operation is not replayed after a later network failure',async()=>{
    const initial={version:1,sections:{projects:[{id:'first',title:'old'},{id:'second',title:'old'}]}};
    const backend=memoryAPI(initial);let fail=true;
    const {editor}=fixture(initial,async(...args)=>{
        if(args[2].item.id==='second' && fail) throw Object.assign(new Error('offline'),{status:503});
        return backend.api(...args);
    });
    editor.change('projects',{id:'first',title:'saved'});editor.change('projects',{id:'second',title:'later'});
    assert.equal(await editor.flush(),false);assert.equal(editor.queue.pending['projects/first'],undefined);
    fail=false;clearTimeout(editor.queue.retry);editor.queue.retry=null;
    assert.equal(await editor.flush(),true);assert.equal(backend.calls.filter(c=>c.body.item.id==='first').length,1);editor.queue.stop();
});
test('a late validation error cannot block a newer corrected value',async()=>{
    const initial={version:1,sections:{certifications:[{id:'bad',name:'valid',visibility:'PUBLIC'}]}};
    const backend=memoryAPI(initial);let reject;
    const {editor}=fixture(initial,(...args)=>args[2].item.name===''?new Promise((resolve,r)=>{reject=r;}):backend.api(...args));
    editor.change('certifications',{id:'bad',name:'',visibility:'PUBLIC'});
    const flight=editor.flush();await new Promise(setImmediate);
    editor.change('certifications',{id:'bad',name:'fixed',visibility:'PUBLIC'});
    reject(Object.assign(new Error('invalid'),{status:400}));
    assert.equal(await flight,true);assert.deepEqual(editor.queue.errors,{});assert.equal(backend.state.sections.certifications[0].name,'fixed');editor.queue.stop();
});
test('a failed reservation holds references while independent items save; correction releases dependencies',async()=>{
    const backend=memoryAPI({version:1,sections:{}});let rejectReservation=true;
    const {editor}=fixture(backend.state,(...args)=>{
        if(args[1]==='POST'&&args[2].item.id==='skill'&&rejectReservation)throw Object.assign(new Error('limit'),{status:400});
        return backend.api(...args);
    });
    editor.change('skills',{id:'skill',name:'C++',visibility:'PUBLIC'});
    editor.change('projects',{id:'project',title:'linked',skillIds:['skill'],visibility:'PUBLIC'});
    editor.change('certifications',{id:'certificate',name:'independent',visibility:'PUBLIC'});
    assert.equal(await editor.flush(),false);
    assert.equal(backend.state.sections.certifications[0].name,'independent');
    assert.deepEqual(editor.queue.errors['projects/project'].dependencies,['skills/skill']);
    assert.equal(backend.calls.some(c=>c.body.item.skillIds?.length),false);
    rejectReservation=false;editor.change('skills',{id:'skill',name:'C++ corrected',visibility:'PUBLIC'});
    assert.equal(await editor.flush(),true);assert.deepEqual(backend.state.sections.projects[0].skillIds,['skill']);editor.queue.stop();
});
