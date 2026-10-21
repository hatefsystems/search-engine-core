// Dashboard presentation helpers. All persistence stays in the existing save queues.
export function dashboardIcon(name) {
    const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    svg.classList.add('dashboard-icon'); svg.setAttribute('aria-hidden', 'true');
    const use = document.createElementNS(svg.namespaceURI, 'use');
    use.setAttribute('href', `#d-${name}`); svg.append(use); return svg;
}

export function updateBasicCompletion(current) {
    const values = ['name', 'title', 'company', 'tagline', 'bio', 'location', 'availabilityStatus', 'avatarUrl', 'coverImageUrl'];
    const count = values.filter(key => String(current[key] || '').trim()).length;
    const percent = Math.round(count / values.length * 100);
    const fa = n => n.toLocaleString('fa-IR');
    const ring = document.getElementById('basic-completion-ring');
    ring.style.setProperty('--completion', `${percent}%`);
    ring.setAttribute('aria-valuenow', String(percent));
    ring.setAttribute('aria-valuetext', `${fa(count)} مورد از ${fa(values.length)} مورد تکمیل شده`);
    document.getElementById('basic-completion-percent').textContent = `${fa(percent)}٪`;
    document.getElementById('basic-completion-done').textContent = `${fa(count)} مورد از ${fa(values.length)} مورد تکمیل شده`;
    document.getElementById('basic-completion-remaining').textContent = `${fa(values.length - count)} مورد باقی‌مانده`;
    const bio = document.getElementById('profile-bio');
    const length = [...bio.value].length;
    // Structured about text supports a longer description than the legacy bio.
    const limit = Object.hasOwn(current.sections || {}, 'about') ? 5000 : 500;
    const counter = document.getElementById('bio-count');
    counter.textContent = `${fa(length)} / ${fa(limit)}`;
    counter.classList.toggle('over-limit', length > limit);
}

export function bindImageCrop({getSource, save, notice}) {
    const dialog = document.getElementById('image-crop-dialog');
    const canvas = document.getElementById('crop-canvas');
    const sliders = ['zoom', 'x', 'y'].map(key => document.getElementById(`crop-${key}`));
    const apply = document.getElementById('apply-crop');
    let source, kind, busy = false, request = 0;
    function draw() {
        if (!source) return;
        const [zoom, x, y] = sliders.map(input => Number(input.value));
        const scale = Math.max(canvas.width / source.naturalWidth, canvas.height / source.naturalHeight) * zoom;
        const width = canvas.width / scale, height = canvas.height / scale;
        const context = canvas.getContext('2d');
        context.clearRect(0, 0, canvas.width, canvas.height);
        context.drawImage(source, (source.naturalWidth - width) * x, (source.naturalHeight - height) * y, width, height, 0, 0, canvas.width, canvas.height);
    }
    for (const slider of sliders) slider.addEventListener('input', draw);
    for (const button of document.querySelectorAll('[data-crop]')) button.onclick = async () => {
        if (busy) return;
        const token = ++request;
        try {
            const selected = button.dataset.crop, url = getSource(selected);
            if (!url) return;
            const loaded = new Image(); loaded.crossOrigin = 'anonymous'; loaded.src = url;
            await loaded.decode(); if (token !== request || getSource(selected) !== url) return;
            kind = selected; source = loaded;
            canvas.width = kind === 'cover' ? 1600 : 640; canvas.height = kind === 'cover' ? 400 : 640;
            sliders[0].value = '1'; sliders[1].value = sliders[2].value = '0.5';
            draw(); dialog.showModal(); sliders[0].focus();
        } catch { notice('تصویر برای برش در دسترس نیست. آن را دوباره بارگذاری کنید.'); }
    };
    dialog.addEventListener('close', () => { source = null; ++request; });
    apply.onclick = async () => {
        if (!source || busy) return;
        busy = true; apply.disabled = true;
        try {
            const selected = kind, token = request;
            const blob = await new Promise(resolve => canvas.toBlob(resolve, 'image/webp', 0.92));
            if (token !== request || !dialog.open) return;
            if (!blob) throw new Error('ساخت تصویر انجام نشد.');
            dialog.close();
            await save(selected, new File([blob], `${selected}-crop.webp`, {type:blob.type}));
        } catch { notice('ذخیرهٔ برش انجام نشد. تصویر را دوباره بارگذاری کنید.'); }
        finally { busy = false; apply.disabled = false; }
    };
}
