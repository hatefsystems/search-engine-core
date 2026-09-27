import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const dataURL=source=>`data:text/javascript;base64,${Buffer.from(source).toString('base64')}`;
const root=new URL('../../public/assets/js/',import.meta.url);
const autosave=dataURL(await readFile(new URL('profile-autosave.js',root),'utf8'));
const ui=dataURL(await readFile(new URL('profile-content-ui.js',root),'utf8'));
const source=(await readFile(new URL('profile-content-editor.js',root),'utf8')).replace(/(['"])\.\/profile-autosave\.js\1/,JSON.stringify(autosave)).replace(/(['"])\.\/profile-content-ui\.js\1/,JSON.stringify(ui));
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
