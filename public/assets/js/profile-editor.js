import {projectPublicProfile} from './profile-public.js';
import {ProfileLinksEditor} from './profile-links-editor.js';
import {ProfileContentEditor} from './profile-content-editor.js';
import {ProfileAutosave} from './profile-autosave.js';

const $ = id => document.getElementById(id);
const fields = ['name', 'title', 'company', 'bio', 'location', 'availabilityStatus'];
const availability = {AVAILABLE:'آماده همکاری', BUSY:'مشغول به کار', NOT_AVAILABLE:'فعلاً در دسترس نیست'};
const levels = {BEGINNER:'مبتدی', INTERMEDIATE:'متوسط', ADVANCED:'پیشرفته', EXPERT:'حرفه‌ای'};
let id = document.body.dataset.profileId, slug = document.body.dataset.slug;
let current = {}, autosave, contentEditor, linksEditor, fullData, latest, key = '', uploading = false;
let mutationTail = Promise.resolve();
function mutate(operation) {
    const task = mutationTail.then(async () => {
        if (autosave?.stopped || autosave?.blocked || contentEditor?.queue.blocked) { const error = new Error('ابتدا تعارض اطلاعات را بررسی کنید.'); error.status = 409; throw error; }
        const result = await operation(autosave?.version ?? current.version);
        if (autosave) autosave.version = result.data.version;
        if (contentEditor) contentEditor.queue.version = result.data.version;
        return result;
    });
    mutationTail = task.catch(() => {}); return task;
}
function accept(data) {
    fullData = data; current = {...editorData(data), ...autosave.pending, isPublic:!!data.isPublic};
    autosave.version = data.version; autosave.persist(); preview();
}
// Each tab keeps its own unacknowledged draft, so another tab cannot erase it.
// Duplicated tabs copy sessionStorage, so allocate a fresh key for each page instance.
const draftId = crypto.randomUUID();
let previousDraftId = '';
try { previousDraftId = sessionStorage.getItem('hatef.profile.tab') || ''; sessionStorage.setItem('hatef.profile.tab', draftId); }
catch {}
const status = text => { if (text === 'ذخیره شد' && (Object.keys(autosave?.pending || {}).length || Object.keys(contentEditor?.queue.pending || {}).length || Object.keys(linksEditor?.queue.pending || {}).length)) text = 'در حال ذخیره…'; $('save-status').textContent = text; $('save-status').dataset.saved = String(text === 'ذخیره شد'); };
const notice = text => { $('notice').textContent = text; $('notice').hidden = !text; };
async function api(path, method = 'GET', body) {
    const response = await fetch(path, {method, credentials:'same-origin', cache:'no-store',
        headers:body ? {'Content-Type':'application/json'} : {}, body:body ? JSON.stringify(body) : undefined});
    let result;
    try { result = await response.json(); } catch { result = {}; }
    if (!response.ok) {
        const error = new Error(result.message || result.error?.message || 'درخواست انجام نشد؛ دوباره تلاش کنید.');
        error.status = response.status;
        const retry = response.headers.get('Retry-After');
        error.retryAfter = /^\d+$/.test(retry || '') ? Number(retry) : Math.max(1, (Date.parse(retry) - Date.now()) / 1000) || 3;
        throw error;
    }
    return result;
}
const endpoint = () => `/api/profiles/${id}`;
function editorData(data) {
    const result = {version:data.version || 0, isPublic:!!data.isPublic, avatarUrl:data.avatarUrl || '', coverImageUrl:data.coverImageUrl || ''};
    for (const field of fields) result[field] = data[field] || '';
    if (Object.hasOwn(data.sections || {}, 'about')) result.bio = data.sections.about[0]?.description || '';
    if (Object.hasOwn(data.sections || {}, 'availability')) result.availabilityStatus = data.sections.availability[0]?.status || '';
    result.skillsWithLevel = data.skillsWithLevel?.length ? data.skillsWithLevel : (data.skills || []).map(name => ({name, level:'BEGINNER'}));
    return result;
}
function image(id, url) {
    const element = $(id);
    let safe = '';
    try { const parsed = new URL(url, location.origin); if (url && ['http:', 'https:'].includes(parsed.protocol)) safe = parsed.href; } catch {}
    element.hidden = !safe;
    if (safe && element.src !== safe) element.src = safe;
    if (!safe) element.removeAttribute('src');
    element.onerror = () => { element.hidden = true; };
}
function preview() {
    $('account-name').textContent = current.name || 'پروفایل من';
    $('account-title').textContent = current.title || '';
    image('editor-avatar', current.avatarUrl); image('editor-cover', current.coverImageUrl);
    const content = contentEditor?.sections || fullData?.sections || {}, layout = contentEditor?.layout || fullData?.contentLayout || {};
    for (const [section, field, property] of [['about','bio','description'],['availability','availabilityStatus','status']]) {
        const input = $('profile-form').elements.namedItem(field);
        if (Object.hasOwn(content, section) && document.activeElement !== input) input.value = content[section][0]?.[property] || '';
    }
    const profile = projectPublicProfile({...fullData, ...current, id, sections:content, contentLayout:layout, sectionOrder:layout.order, featured:layout.featured, privacy:layout.privacy || fullData?.privacy || {}}, linksEditor?.links || []);
    $('public-preview').contentWindow?.postMessage({type:'hatef-profile-preview', profile}, location.origin);
    $('publication-label').textContent = current.isPublic ? 'صفحهٔ منتشرشده' : 'پیش‌نویس خصوصی';
    $('publish').hidden = current.isPublic;
}
window.addEventListener('message', event => {
    if (event.origin === location.origin && event.source === $('public-preview').contentWindow && event.data?.type === 'hatef-profile-preview-ready' && fullData) preview();
});

function change(field, value) {
    current[field] = value;
    const section = {bio:'about', availabilityStatus:'availability'}[field];
    if (section && Object.hasOwn(contentEditor?.sections || {}, section)) {
        const property = field === 'bio' ? 'description' : 'status';
        const existing = contentEditor.sections[section][0];
        const item = existing || {...structuredClone(contentEditor.schemas[section].defaults), id:crypto.randomUUID(), visibility:'HIDDEN', evidence:[], ...(section === 'about' ? {title:'دربارهٔ من'} : {type:'COLLABORATION'})};
        contentEditor.change(section, {...item, [property]:value});
        if (!existing) notice('معرفی یا وضعیت جدید به‌صورت پیش‌نویس ذخیره شد. نمایش عمومی را از بخش مربوط انتخاب کنید.');
    } else autosave.change(field, value);
    preview();
}
function skillRow(skill = {name:'', level:'BEGINNER'}) {
    const row = document.createElement('div'); row.className = 'skill-row';
    const name = document.createElement('input'); name.value = skill.name; name.dir = 'auto'; name.placeholder = 'نام مهارت'; name.setAttribute('aria-label', 'نام مهارت');
    const level = document.createElement('select'); level.setAttribute('aria-label', 'سطح مهارت');
    for (const [value, label] of Object.entries(levels)) level.add(new Option(label, value));
    level.value = skill.level;
    const remove = document.createElement('button'); remove.type = 'button'; remove.textContent = '×'; remove.setAttribute('aria-label', 'حذف مهارت');
    const save = () => change('skillsWithLevel', [...$('skills-editor').children].map(item => ({name:item.querySelector('input').value, level:item.querySelector('select').value})).filter(item => item.name));
    name.addEventListener('input', save); level.addEventListener('change', save);
    remove.onclick = () => { row.remove(); save(); };
    row.append(name, level, remove); $('skills-editor').append(row); return name;
}
function fill() {
    for (const field of fields) $('profile-form').elements.namedItem(field).value = current[field] || '';
    $('skills-editor').replaceChildren();
    for (const skill of current.skillsWithLevel || []) skillRow(skill);
    preview();
}
function conflict() { if (autosave) autosave.blocked = true; if (contentEditor) contentEditor.queue.blocked = true; $('conflict').hidden = false; status('ذخیره نشد؛ نسخهٔ جدید را بررسی کنید.'); }
function activate(data) {
    fullData = data; current = editorData(data);
    let recoveredDraft;
    autosave = new ProfileAutosave({id, draftId, version:current.version, status, conflict,
        send: async patch => (await mutate(version => api(endpoint(), 'PUT', {...patch, version}))).data,
        saved: data => {
            accept(data); contentEditor?.receive(data);
            if (recoveredDraft && !Object.keys(autosave.pending).length) {
                try { if (localStorage.getItem(recoveredDraft.key) === recoveredDraft.value) localStorage.removeItem(recoveredDraft.key); } catch {}
                recoveredDraft = null;
            }
        }});
    // Recover a closed tab's draft if this tab has none. Keep the original copy until logout.
    try {
        if (!localStorage.getItem(autosave.key)) {
            const prefix = `hatef.profile.pending.${id}`;
            const candidates = Object.keys(localStorage).filter(k => k === prefix || k.startsWith(prefix + '.'))
                .map(k => ({key:k, value:localStorage.getItem(k), data:JSON.parse(localStorage.getItem(k))})).filter(item => item.data)
                .sort((a,b) => (b.data.updatedAt || 0) - (a.data.updatedAt || 0));
            recoveredDraft = candidates.find(item => item.key === `${prefix}.${previousDraftId}`) || candidates[0];
            if (recoveredDraft) localStorage.setItem(autosave.key, recoveredDraft.value);
        }
    } catch {}
    current = {...current, ...autosave.restore(current), isPublic:!!data.isPublic};
    $('start-panel').hidden = true; $('login-panel').hidden = true; $('workspace').hidden = false;
    status(Object.keys(autosave.pending).length ? 'در حال ذخیره…' : 'ذخیره شد');
    if (autosave.blocked) conflict();
    fill();
    contentEditor = new ProfileContentEditor({id, draftId, previousDraftId, api, mutate, version:()=>autosave.version, accept, status, conflict, notice, headerPreview:preview});
    contentEditor.start(data).catch(error => notice(error.message));
    linksEditor = new ProfileLinksEditor({id, draftId, api, notice, status, preview});
    linksEditor.start().catch(error => notice(error.message));
}
async function load() {
    try {
        const result = await api(endpoint());
        if (!result.canEdit) { $('login-panel').hidden = false; status('ورود با کلید'); return; }
        activate(result.data);
    } catch (error) {
        if ([401,403].includes(error.status)) { $('login-panel').hidden = false; status('ورود با کلید'); }
        else notice(error.message);
    }
}
$('start').onclick = async () => {
    $('start').disabled = true; notice('');
    try {
        const result = await api('/api/profiles', 'POST', {type:'PERSON', slug, isPublic:false, name:''});
        id = result.data.id; document.body.dataset.profileId = id; watchSession();
        history.replaceState(null, '', `/profiles/${encodeURIComponent(slug)}/edit`);
        key = result.ownerToken; $('new-key').textContent = key; $('key-panel').hidden = false;
        activate(result.data);
    } catch (error) { notice(error.message); }
    finally { $('start').disabled = false; }
};
$('login-form').onsubmit = async event => {
    event.preventDefault(); const button = event.submitter; button.disabled = true; notice('');
    try { await api(`${endpoint()}/session`, 'POST', {key:$('owner-key').value.trim()}); $('owner-key').value = ''; await load(); }
    catch (error) { notice(error.message); }
    finally { button.disabled = false; }
};
$('copy-key').onclick = async () => { try { await navigator.clipboard.writeText(key); $('copy-key').textContent = 'کپی شد'; } catch { notice('کپی خودکار ممکن نیست؛ کلید را انتخاب و کپی کنید.'); } };
$('download-key').onclick = () => {
    const url = URL.createObjectURL(new Blob([`صفحه: /${slug}\nکلید دسترسی ویرایش:\n${key}\nاین کلید را در اختیار دیگران نگذارید.\n`], {type:'text/plain;charset=utf-8'}));
    const link = document.createElement('a'); link.href = url; link.download = 'hatef-profile-key.txt'; link.click(); setTimeout(() => URL.revokeObjectURL(url), 1000);
};
$('dismiss-key').onclick = () => { key = ''; $('new-key').textContent = ''; $('key-panel').hidden = true; };
$('profile-form').onsubmit = event => event.preventDefault();
for (const field of fields) $('profile-form').elements.namedItem(field).addEventListener('input', event => change(field, event.target.value));
$('add-skill').onclick = () => { if ($('skills-editor').children.length < 50) skillRow().focus(); };
for (const button of document.querySelectorAll('[data-clear]')) button.onclick = () => change(button.dataset.clear, '');
$('publish').onclick = async () => {
    if (!current.name.trim()) { contentEditor.select('basic'); notice('برای انتشار، نام فارسی را وارد کنید.'); $('profile-name').focus(); return; }
    $('publish').disabled = true; notice('');
    try {
        if (!await autosave.flush() || !await contentEditor.flush() || !await linksEditor.flush()) { notice('ابتدا ذخیرهٔ تغییرات را کامل کنید.'); return; }
        autosave.change('isPublic', true);
        if (!await autosave.flush()) notice('انتشار انجام نشد؛ وضعیت ذخیره را بررسی کنید.');
    } finally { $('publish').disabled = false; }
};
$('reload-version').onclick = async () => {
    try {
        latest = (await api(endpoint())).data;
        const labels = {name:'نام', title:'عنوان شغلی', company:'شرکت', bio:'معرفی', location:'شهر', availabilityStatus:'وضعیت همکاری'};
        $('server-version').textContent = Object.entries(labels).map(([field,label]) => `${label}: ${latest[field]}`).join('\n') + '\nمهارت‌ها: ' + (latest.skillsWithLevel || []).map(s => s.name).join('، ') + '\n\nبخش‌های ذخیره‌شده:\n' + JSON.stringify(latest.sections || {}, null, 2);
        $('conflict-review').hidden = false;
    } catch (error) { notice(error.message); }
};
async function resolveConflict(keep) {
    if (!latest) return;
    current = {...editorData(latest), ...(keep ? autosave.pending : {})};
    fullData = latest; autosave.version = latest.version; autosave.blocked = false; contentEditor.queue.blocked = false;
    $('conflict').hidden = true; $('conflict-review').hidden = true; fill();
    await contentEditor.resolve(latest, keep);
    await autosave.resolve(autosave.version, keep); latest = null;
    if (!keep) status('ذخیره شد');
}
$('keep-local').onclick = () => resolveConflict(true);
$('use-server').onclick = () => resolveConflict(false);
$('logout').onclick = async () => {
    if (uploading) return;
    $('logout').disabled = true;
    try {
        // Prevent new requests before clearing the cookie; await the request already in flight.
        autosave.blocked = true; contentEditor.queue.blocked = true; linksEditor.queue.blocked = true;
        if (linksEditor.queue.busy) await linksEditor.queue.flight;
        await mutationTail;
        if (contentEditor.queue.busy) await contentEditor.queue.flight;
        if (autosave.busy) await autosave.flight;
        await api(`${endpoint()}/session`, 'DELETE'); autosave.stop(); contentEditor.stop(); linksEditor.stop();
        try { const prefix = `hatef.profile.pending.${id}`; for (const k of Object.keys(localStorage)) if (k === prefix || k.startsWith(prefix + '.')) localStorage.removeItem(k); } catch {}
        sessionChannel?.postMessage('logout');
        fullData = null; contentEditor = null; linksEditor = null; current = {}; fill(); $('public-preview-dialog').close(); key = ''; $('new-key').textContent = ''; $('key-panel').hidden = true;
        $('workspace').hidden = true; $('login-panel').hidden = false; $('conflict').hidden = true; notice(''); status('از دسترسی ویرایش خارج شدید');
    } catch (error) { autosave.blocked = false; contentEditor.queue.blocked = false; linksEditor.queue.blocked = false; notice(error.message); }
    finally { $('logout').disabled = false; }
};
for (const kind of ['avatar', 'cover']) $(`${kind}-file`).onchange = async event => {
    const file = event.target.files[0]; if (!file || uploading) return;
    if (file.size > (kind === 'avatar' ? 5 : 10) * 1024 * 1024) { notice('اندازهٔ تصویر بیش از حد مجاز است.'); return; }
    uploading = true; $('profile-form').inert = true; $('publish').disabled = true; notice('');
    try {
        if (!await autosave.flush() || !await contentEditor.flush()) throw new Error('ابتدا ذخیرهٔ تغییرات را کامل کنید.');
        status('در حال ذخیرهٔ تصویر…');
        const image = await new Promise((resolve, reject) => { const reader = new FileReader(); reader.onload = () => resolve(reader.result); reader.onerror = reject; reader.readAsDataURL(file); });
        const result = await mutate(version => api(`${endpoint()}/${kind}`, 'POST', {image, version}));
        autosave.version = result.data.version;
        Object.assign(current, result.data); preview(); status('ذخیره شد');
    } catch (error) { if (error.status === 409) { autosave.blocked = true; conflict(); } else notice(error.message || 'بارگذاری تصویر انجام نشد.'); }
    finally { uploading = false; $('profile-form').inert = false; $('publish').disabled = false; event.target.value = ''; }
};
for (const button of document.querySelectorAll('[data-section-shortcut]')) button.onclick = () => contentEditor?.select(button.dataset.sectionShortcut);
$('add-section').onclick = () => $('section-picker').showModal();
$('close-section-picker').onclick = () => $('section-picker').close();
const previewFrame = $('public-preview'), previewSurface = previewFrame.parentElement;
$('expand-preview').onclick = () => { $('expanded-preview-host').append(previewFrame); $('public-preview-dialog').showModal(); preview(); };
$('close-public-preview').onclick = () => $('public-preview-dialog').close();
$('public-preview-dialog').addEventListener('close', () => { previewSurface.append(previewFrame); preview(); $('expand-preview').focus(); });
for (const button of document.querySelectorAll('[data-preview-size]')) button.onclick = () => {
    $('preview').dataset.device = button.dataset.previewSize;
    for (const other of document.querySelectorAll('[data-preview-size]')) other.setAttribute('aria-pressed',String(other === button));
};
for (const tab of ['form', 'preview']) $(`${tab}-tab`).onclick = () => {
    $('workspace').classList.toggle('show-preview', tab === 'preview');
    $('form-tab').setAttribute('aria-selected', String(tab === 'form')); $('preview-tab').setAttribute('aria-selected', String(tab === 'preview'));
};
window.addEventListener('online', () => { if (autosave && !autosave.retry) autosave.flush(); });
window.addEventListener('beforeunload', event => { if (autosave && Object.keys(autosave.pending).length) { autosave.persist(); event.preventDefault(); event.returnValue = ''; } });
let sessionChannel;
function watchSession() {
    sessionChannel?.close();
    if (!id || !globalThis.BroadcastChannel) return;
    sessionChannel = new BroadcastChannel(`hatef.profile.session.${id}`);
    sessionChannel.onmessage = event => {
        if (event.data === 'logout') {
            autosave?.stop(); contentEditor?.stop(); linksEditor?.stop(); fullData = null; contentEditor = null; linksEditor = null; current = {}; fill(); $('public-preview-dialog').close(); key = ''; $('new-key').textContent = '';
            $('key-panel').hidden = true; $('workspace').hidden = true; $('login-panel').hidden = false;
            status('از دسترسی ویرایش خارج شدید');
        }
    };
}
watchSession();
$('start').disabled = false;
if (id) load();
