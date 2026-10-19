// One request at a time; dirty fields are acknowledged only if they haven't changed since dispatch.
export class ProfileAutosave {
    constructor({id, version, send, status, saved, conflict, storage = globalThis.localStorage, clock = globalThis, draftId = "", incremental = false, errorsChanged = () => {}}) {
        Object.assign(this, {id, version, send, status, saved, conflict, storage, clock});
        this.incremental = incremental; this.errorsChanged = errorsChanged; this.errors = {};
        this.pending = {}; this.busy = false; this.blocked = false; this.stopped = false;
        this.key = `hatef.profile.pending.${id}${draftId ? "." + draftId : ""}`;
        this.debounce = null; this.maximum = null; this.retry = null;
    }
    persist() {
        try {
            if (Object.keys(this.pending).length) this.storage.setItem(this.key, JSON.stringify({version:this.version, fields:this.pending, errors:this.errors, updatedAt:Date.now()}));
            else this.storage.removeItem(this.key);
        } catch { this.status('ذخیرهٔ محلی ممکن نیست؛ این صفحه را تا پایان ذخیره باز نگه دارید.'); }
    }
    restore(server) {
        try {
            const local = JSON.parse(this.storage.getItem(this.key) || 'null');
            if (!local) return {};
            this.pending = local.fields || {};
            this.errors = local.errors || {};
            for (const field of Object.keys(this.pending))
                if (JSON.stringify(this.pending[field]) === JSON.stringify(server[field] ?? '')) delete this.pending[field];
            for (const key of Object.keys(this.errors))
                if (JSON.stringify(this.errors[key].value) !== JSON.stringify(this.pending[key])) delete this.errors[key];
            if (Object.keys(this.pending).length && local.version !== this.version) {
                this.blocked = true; this.conflict();
            } else if (Object.keys(this.pending).length) this.schedule();
            this.persist(); return structuredClone(this.pending);
        } catch { return {}; }
    }
    change(field, value) {
        this.pending[field] = structuredClone(value);
        delete this.errors[field];
        for (const key of Object.keys(this.errors))
            if (this.errors[key].dependencies?.includes(field)) delete this.errors[key];
        this.errorsChanged();
        this.persist(); this.status('در حال ذخیره…'); this.schedule();
    }
    acknowledge(key, value, data) {
        if (this.stopped) return;
        this.version = data.version;
        if (JSON.stringify(this.pending[key]) === JSON.stringify(value)) {
            delete this.pending[key]; delete this.errors[key];
        }
        this.persist(); this.saved(data); this.errorsChanged();
    }
    reject(key, value, error) {
        if (this.stopped || JSON.stringify(this.pending[key]) !== JSON.stringify(value)) return;
        this.errors[key] = {value:structuredClone(value), message:error.message, field:error.field,
            dependencies:error.dependencies};
        this.persist(); this.errorsChanged();
    }
    retryErrors() {
        this.errors = {}; this.persist(); this.errorsChanged(); return this.flush();
    }
    ready() {
        return Object.fromEntries(Object.entries(this.pending).filter(([key]) => !this.errors[key]));
    }
    schedule() {
        if (this.stopped || this.blocked || this.retry) return;
        this.clock.clearTimeout(this.debounce);
        this.debounce = this.clock.setTimeout(() => this.flush(), 600);
        if (!this.maximum) this.maximum = this.clock.setTimeout(() => this.flush(), 2000);
    }
    async flush() {
        this.clock.clearTimeout(this.debounce); this.clock.clearTimeout(this.maximum); this.maximum = null;
        if (this.stopped || this.blocked || this.retry) return false;
        if (this.busy) { await this.flight; return this.flush(); }
        if (!Object.keys(this.pending).length) return true;
        const fields = structuredClone(this.ready());
        if (!Object.keys(fields).length) {
            this.status('تغییرات ذخیره‌نشده را بررسی کنید.'); return false;
        }
        this.busy = true; this.status('در حال ذخیره…');
        this.flight = (async () => {
            try {
                const data = await this.send({...fields, version:this.version}, {
                    acknowledge:(key, data) => this.acknowledge(key, fields[key], data),
                    reject:(key, error) => this.reject(key, fields[key], error),
                });
                if (this.stopped) return false;
                this.version = data.version;
                if (!this.incremental) for (const key of Object.keys(fields))
                    if (JSON.stringify(this.pending[key]) === JSON.stringify(fields[key])) delete this.pending[key];
                this.persist(); this.saved(data);
                this.status(Object.keys(this.errors).length ? 'تغییرات ذخیره‌نشده را بررسی کنید.' : Object.keys(this.pending).length ? 'در حال ذخیره…' : 'ذخیره شد');
                return true;
            } catch (error) {
                if (this.stopped) return false;
                if (error.status === 409) { this.blocked = true; this.conflict(); }
                else if (error.status === 400 || error.status === 401 || error.status === 403) {
                    this.status(error.message || 'ذخیره نشد؛ اطلاعات یا دسترسی را بررسی کنید.');
                } else {
                    this.status('ذخیره نشد؛ تلاش مجدد…');
                    this.clock.clearTimeout(this.retry);
                    this.retry = this.clock.setTimeout(() => { this.retry = null; this.flush(); }, Math.max(1000, (error.retryAfter || 3) * 1000));
                }
                return false;
            } finally { this.busy = false; }
        })();
        const ok = await this.flight;
        if (ok && Object.keys(this.ready()).length) return this.flush();
        return ok && !Object.keys(this.pending).length;
    }
    resolve(version, keep) {
        this.version = version; this.blocked = false;
        if (!keep) { this.pending = {}; this.errors = {}; this.errorsChanged(); }
        this.persist(); return keep ? this.flush() : Promise.resolve(true);
    }
    stop() {
        this.stopped = true;
        for (const timer of [this.debounce, this.maximum, this.retry]) this.clock.clearTimeout(timer);
        this.pending = {}; try { this.storage.removeItem(this.key); } catch {}
    }
}
