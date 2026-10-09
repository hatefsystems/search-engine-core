import {test} from 'node:test';
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const source = await readFile(new URL('../../public/assets/js/profile-autosave.js', import.meta.url),'utf8');
const {ProfileAutosave} = await import(`data:text/javascript;base64,${Buffer.from(source).toString('base64')}`);
class Clock {
    now=0; sequence=0; timers=new Map();
    setTimeout=(fn,ms)=>{const id=++this.sequence;this.timers.set(id,{fn,time:this.now+ms});return id;};
    clearTimeout=id=>this.timers.delete(id);
    async tick(ms){const end=this.now+ms; for(;;){const next=[...this.timers].sort((a,b)=>a[1].time-b[1].time)[0];if(!next||next[1].time>end)break;this.now=next[1].time;this.timers.delete(next[0]);next[1].fn();await new Promise(setImmediate);}this.now=end;}
}
function fixture(send, storage=new Map()) {
    const clock=new Clock(), states=[];
    const persistence={getItem:k=>storage.get(k),setItem:(k,v)=>storage.set(k,v),removeItem:k=>storage.delete(k)};
    const saver=new ProfileAutosave({id:'test',version:0,send,clock,storage:persistence,status:s=>states.push(s),saved:()=>{},conflict:()=>states.push('conflict')});
    return {saver,clock,storage,states};
}
test('one character saves after 600 ms; continuous input dispatches by two seconds',async()=>{
    const sent=[];const {saver,clock}=fixture(async patch=>{sent.push(patch);return {version:patch.version+1};});
    saver.change('name','ه');await clock.tick(599);assert.equal(sent.length,0);await clock.tick(1);assert.equal(sent[0].name,'ه');
    for(let i=0;i<5;i++){saver.change('bio',String(i));await clock.tick(400);}
    assert.equal(sent.length,2);assert.equal(sent[1].bio,'4');
});
test('slow acknowledgment preserves newer values and sends serially',async()=>{
    let resolve;const sent=[];const {saver,clock,storage}=fixture(patch=>{sent.push(patch);return new Promise(r=>resolve=r);});
    saver.change('name','ه');await clock.tick(600);saver.change('name','ها');saver.change('bio','');
    await clock.tick(600);assert.equal(sent.length,1);assert.ok(storage.size);
    resolve({version:1});await new Promise(setImmediate);assert.equal(sent.length,2);assert.equal(sent[1].name,'ها');assert.equal(sent[1].bio,'');assert.equal(sent[1].version,1);
    resolve({version:2});await new Promise(setImmediate);assert.deepEqual(saver.pending,{});assert.equal(storage.size,0);
});
test('offline changes survive reload including deletions and empty skills',async()=>{
    const a=fixture(async()=>{throw new TypeError('offline');});a.saver.change('bio','');a.saver.change('skillsWithLevel',[]);
    await a.clock.tick(600);assert.ok(a.states.includes('ذخیره نشد؛ تلاش مجدد…'));
    const b=fixture(async p=>({version:p.version+1}),a.storage);
    assert.deepEqual(b.saver.restore({bio:'old',skillsWithLevel:[{name:'C++'}]}),{bio:'',skillsWithLevel:[]});
    await b.clock.tick(600);assert.equal(b.storage.size,0);
});
test('conflict pauses and preserves local changes until explicit resolution',async()=>{
    let calls=0;const f=fixture(async patch=>{if(++calls===1)throw Object.assign(new Error(),{status:409});return {version:patch.version+1};});
    f.saver.change('name','ه');await f.clock.tick(600);assert.equal(f.saver.blocked,true);
    await f.clock.tick(10000);assert.equal(calls,1);assert.equal(f.saver.pending.name,'ه');
    await f.saver.resolve(7,true);assert.equal(f.saver.version,8);assert.equal(f.storage.size,0);
});
test('retry-after is honored even when publish asks to flush',async()=>{
    let calls=0;const f=fixture(async p=>{if(++calls===1)throw Object.assign(new Error(),{status:429,retryAfter:5});return {version:p.version+1};});
    f.saver.change('name','ه');await f.clock.tick(600);assert.equal(await f.saver.flush(),false);
    await f.clock.tick(4999);assert.equal(calls,1);await f.clock.tick(1);assert.equal(calls,2);
});
test('logout clears local draft and late responses cannot recreate it',async()=>{
    let resolve;const f=fixture(()=>new Promise(r=>resolve=r));f.saver.change('name','ه');await f.clock.tick(600);
    f.saver.stop();resolve({version:1});await new Promise(setImmediate);assert.equal(f.storage.size,0);assert.deepEqual(f.saver.pending,{});
});
test('restoring against changed server version blocks silent overwrite',()=>{
    const f=fixture(async()=>{});f.saver.change('name','ه');const g=fixture(async()=>{},f.storage);g.saver.version=2;
    assert.equal(g.saver.restore({name:'نام دیگر'}).name,'ه');assert.equal(g.saver.blocked,true);
});

test('item errors and drafts survive reload without automatically resending invalid values',async()=>{
    const a=fixture(async()=>{});a.saver.change('item',{name:''});
    a.saver.reject('item',{name:''},{message:'required',field:'name'});
    let calls=0;const b=fixture(async()=>{calls++;return {version:1};},a.storage);
    assert.deepEqual(b.saver.restore({}),{item:{name:''}});
    await b.clock.tick(1000);assert.equal(calls,0);assert.equal(b.saver.errors.item.field,'name');
    assert.equal(await b.saver.retryErrors(),true);assert.equal(calls,1);
});
