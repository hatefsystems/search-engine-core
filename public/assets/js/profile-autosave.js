// One request at a time; dirty fields are acknowledged only if they haven't changed since dispatch.
export class ProfileAutosave {
    constructor({id, version, send, status, saved, conflict, storage = globalThis.localStorage, clock = globalThis, draftId = ""}) {
        Object.assign(this, {id, version, send, status, saved, conflict, storage, clock});
        this.pending = {}; this.busy = false; this.blocked = false; this.stopped = false;
        this.key = `hatef.profile.pending.${id}${draftId ? "." + draftId : ""}`;
        this.debounce = null; this.maximum = null; this.retry = null;
    }
    persist() {
        try {
            if (Object.keys(this.pending).length) this.storage.setItem(this.key, JSON.stringify({version:this.version, fields:this.pending, updatedAt:Date.now()}));
            else this.storage.removeItem(this.key);
        } catch { this.status('ذخیرهٔ محلی ممکن نیست؛ این صفحه را تا پایان ذخیره باز نگه دارید.'); }
    }
    restore(server) {
        try {
            const local = JSON.parse(this.storage.getItem(this.key) || 'null');
            if (!local) return {};
            this.pending = local.fields || {};
            for (const field of Object.keys(this.pending))
                if (JSON.stringify(this.pending[field]) === JSON.stringify(server[field] ?? '')) delete this.pending[field];
            if (Object.keys(this.pending).length && local.version !== this.version) {
                this.blocked = true; this.conflict();
            } else if (Object.keys(this.pending).length) this.schedule();
            this.persist(); return structuredClone(this.pending);
        } catch { return {}; }
    }
    change(field, value) {
        this.pending[field] = structuredClone(value);
        this.persist(); this.status('در حال ذخیره…'); this.schedule();
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
        const fields = structuredClone(this.pending);
        this.busy = true; this.status('در حال ذخیره…');
        this.flight = (async () => {
            try {
                const data = await this.send({...fields, version:this.version});
                if (this.stopped) return false;
                this.version = data.version;
                for (const key of Object.keys(fields))
                    if (JSON.stringify(this.pending[key]) === JSON.stringify(fields[key])) delete this.pending[key];
                this.persist(); this.saved(data);
                this.status(Object.keys(this.pending).length ? 'در حال ذخیره…' : 'ذخیره شد');
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
        if (ok && Object.keys(this.pending).length) return this.flush();
        return ok;
    }
    resolve(version, keep) {
        this.version = version; this.blocked = false;
        if (!keep) this.pending = {};
        this.persist(); return keep ? this.flush() : Promise.resolve(true);
    }
    stop() {
        this.stopped = true;
        for (const timer of [this.debounce, this.maximum, this.retry]) this.clock.clearTimeout(timer);
        this.pending = {}; try { this.storage.removeItem(this.key); } catch {}
    }
}
