import { ProfileAutosave } from "./profile-autosave.js";
import { el, safeLink } from "./profile-content-ui.js";
// Link documents have their own versions and never advance the profile version.
export class ProfileLinksEditor {
  constructor({ id, draftId, api, notice, status }) {
    Object.assign(this, { id, api, notice, status });
    this.root = document.getElementById("links-editor");
    this.server = [];
    this.versions = {};
    this.state = el("p", "", "hint");
    this.state.setAttribute("role", "status");
    this.queue = new ProfileAutosave({
      id: `links.${id}`,
      draftId,
      version: 0,
      status: (text) => {
        this.state.textContent = text;
        this.status?.(text);
      },
      conflict: () => this.conflict(),
      send: async (batch) => {
        for (const [key, operation] of Object.entries(batch)) {
          if (key === "version") continue;
          const exists = this.server.some((l) => l.id === key);
          if (operation.remove && !exists) continue;
          const body = operation.remove
            ? { version: this.versions[key] }
            : { ...operation.link, version: this.versions[key] || 0 };
          const response = await this.api(
            `/api/profiles/${id}/links${exists ? "/" + key : ""}`,
            operation.remove ? "DELETE" : exists ? "PUT" : "POST",
            body,
          );
          if (operation.remove) {
            this.server = this.server.filter((l) => l.id !== key);
            delete this.versions[key];
          } else {
            this.server = this.server.filter((l) => l.id !== key);
            this.server.push(response.data);
            this.versions[key] = response.data.version;
            if (
              this.queue.pending[key] &&
              JSON.stringify(this.queue.pending[key]) !==
                JSON.stringify(operation)
            )
              this.queue.pending[key].baseVersion = response.data.version;
          }
        }
        return { version: 0 };
      },
      saved: () => {
        this.rebuild();
        if (this.recovered) {
          try {
            if (
              !Object.keys(this.queue.pending).length &&
              localStorage.getItem(this.recovered.key) === this.recovered.value
            )
              localStorage.removeItem(this.recovered.key);
          } catch {}
        }
      },
    });
    window.addEventListener("online", () => this.queue.flush());
    window.addEventListener("beforeunload", (event) => {
      if (Object.keys(this.queue.pending).length) {
        this.queue.persist();
        event.preventDefault();
        event.returnValue = "";
      }
    });
  }
  async start() {
    this.server = (await this.api(`/api/profiles/${this.id}/links`)).data;
    this.versions = Object.fromEntries(
      this.server.map((l) => [l.id, l.version]),
    );
    try {
      const prefix = `hatef.profile.pending.links.${this.id}.`;
      const candidates = Object.keys(localStorage)
        .filter((k) => k.startsWith(prefix) && k !== this.queue.key)
        .map((key) => ({ key, value: localStorage.getItem(key) }))
        .sort(
          (a, b) =>
            JSON.parse(b.value).updatedAt - JSON.parse(a.value).updatedAt,
        );
      this.recovered = candidates[0];
      if (this.recovered)
        localStorage.setItem(this.queue.key, this.recovered.value);
    } catch {}
    this.queue.restore({});
    // Each queued operation carries its originally observed link version across reloads.
    for (const [id, op] of Object.entries(this.queue.pending))
      if (
        this.server.some((l) => l.id === id) &&
        op.baseVersion !== this.versions[id]
      ) {
        this.queue.blocked = true;
      }
    this.rebuild();
    this.render();
    if (this.queue.blocked) this.conflict();
  }
  rebuild() {
    this.links = structuredClone(this.server);
    for (const [id, op] of Object.entries(this.queue.pending)) {
      const base = this.links.find((l) => l.id === id);
      this.links = this.links.filter((l) => l.id !== id);
      if (!op.remove) this.links.push({ ...base, ...op.link });
    }
    this.links.sort((a, b) => a.sortOrder - b.sortOrder);
    this.renderPreview();
  }
  renderPreview() {
    const box = document.getElementById("links-preview");
    if (!box) return;
    box.replaceChildren();
    const visible = this.links.filter(
      (l) =>
        l.isActive &&
        l.visibility === "PUBLIC" &&
        l.privacy === "PUBLIC" &&
        safeLink(l.url),
    );
    if (!visible.length) return;
    box.append(el("h3", "لینک‌ها"));
    for (const link of visible) {
      const a = el("a", link.title, "content-reference");
      a.href = safeLink(link.url);
      a.target = "_blank";
      a.rel = "noopener noreferrer";
      box.append(a);
    }
  }
  button(text, fn) {
    const b = el("button", text, "button secondary");
    b.type = "button";
    b.onclick = fn;
    return b;
  }
  change(link) {
    const keys = [
      "id",
      "url",
      "title",
      "description",
      "visibility",
      "privacy",
      "sortOrder",
      "isActive",
    ];
    const base = this.server.find((l) => l.id === link.id);
    const dirty = this.queue.pending[link.id]?.link || {};
    const value = Object.fromEntries(
      keys
        .filter(
          (k) =>
            link[k] !== undefined &&
            (!base ||
              k === "id" ||
              Object.hasOwn(dirty, k) ||
              JSON.stringify(link[k]) !== JSON.stringify(base[k])),
        )
        .map((k) => [k, link[k]]),
    );
    this.queue.change(link.id, {
      link: value,
      baseVersion: this.versions[link.id] || 0,
    });
    this.rebuild();
  }
  conflict() {
    this.queue.blocked = true;
    this.state.textContent =
      "یکی از لینک‌ها در جای دیگری تغییر کرده؛ نوشته‌های شما حفظ شده‌اند.";
    const box = el("div", "", "notice");
    box.append(
      this.button("بارگیری لینک‌های جدید و مقایسه", async () => {
        try {
          const data = (await this.api(`/api/profiles/${this.id}/links`)).data;
          const pre = el("pre", JSON.stringify(data, null, 2));
          box.replaceChildren(pre);
          for (const [keep, title] of [
            [true, "اعمال تغییرات من روی نسخهٔ جدید"],
            [false, "استفاده از نسخهٔ جدید"],
          ])
            box.append(
              this.button(title, async () => {
                this.server = data;
                this.versions = Object.fromEntries(
                  data.map((l) => [l.id, l.version]),
                );
                await this.queue.resolve(0, keep);
                this.rebuild();
                this.render();
              }),
            );
        } catch (e) {
          this.notice(e.message);
        }
      }),
    );
    this.root.append(box);
  }
  render() {
    this.root.replaceChildren(el("h2", "لینک‌های صفحه"), this.state);
    this.root.append(
      el(
        "p",
        "«پنهان» دسترسی عمومی را می‌بندد. لینک‌های قدیمی با حالت «دسترسی با لینک» همچنان با آدرس مستقیم باز می‌شوند.",
        "hint",
      ),
    );
    const workspace = el("div", "", "item-workspace");
    const rail = el("aside", "", "item-rail"); rail.setAttribute("aria-label", "فهرست لینک‌ها");
    const list = el("div", "", "item-list"); rail.append(list); workspace.append(rail);
    this.root.append(workspace);
    if (!this.links.some(link => link.id === this.selectedId)) this.selectedId = this.links[0]?.id;
    for (const [index, link] of this.links.entries()) {
      const choice = this.button("", () => { this.selectedId = link.id; this.render(); });
      choice.className = "item-choice";
      choice.setAttribute("aria-pressed", String(this.selectedId === link.id));
      choice.append(el("strong", link.title || "لینک تازه", "item-choice-title"),el("span",link.url || "", "item-choice-subtitle"));
      list.append(choice);
      if (link.id !== this.selectedId) continue;
      const box = el("fieldset");
      box.append(el("legend", link.title || "لینک تازه"));
      for (const [key, title] of [
        ["title", "عنوان لینک"],
        ["url", "آدرس کامل"],
        ["description", "توضیح"],
      ]) {
        const label = el("label", title),
          input = el("input");
        input.value = link[key] || "";
        input.dir = "auto";
        input.oninput = () => {
          link[key] = input.value;
          if (key === "title") choice.querySelector("strong").textContent = input.value || "لینک تازه";
          if (key === "url") choice.querySelector("span").textContent = input.value;
          this.change(link);
        };
        label.append(input);
        box.append(label);
      }
      const label = el("label", "نمایش لینک"),
        select = el("select");
      for (const [v, t] of [
        ["PUBLIC", "عمومی"],
        ["HIDDEN", "پنهان"],
        ["UNLISTED", "دسترسی با لینک (قدیمی)"],
      ])
        select.add(new Option(t, v));
      select.setAttribute("aria-label", "نمایش لینک");
      select.value =
        link.visibility === "HIDDEN"
          ? "HIDDEN"
          : link.privacy === "HIDDEN"
            ? "UNLISTED"
            : "PUBLIC";
      select.onchange = () => {
        link.visibility = select.value === "HIDDEN" ? "HIDDEN" : "PUBLIC";
        link.privacy = select.value === "UNLISTED" ? "HIDDEN" : "PUBLIC";
        this.change(link);
      };
      label.append(select);
      box.append(label);
      for (const [delta, title] of [
        [-1, "بالاتر"],
        [1, "پایین‌تر"],
      ]) {
        const b = this.button(title, () => {
          const other = this.links[index + delta];
          [link.sortOrder, other.sortOrder] = [other.sortOrder, link.sortOrder];
          if (link.sortOrder === other.sortOrder) {
            link.sortOrder = index + delta;
            other.sortOrder = index;
          }
          this.change(link);
          this.change(other);
          this.rebuild();
          this.render();
        });
        b.disabled = index + delta < 0 || index + delta >= this.links.length;
        box.append(b);
      }
      box.append(
        this.button("حذف لینک", () => {
          if (!window.confirm("این لینک حذف شود؟")) return;
          this.queue.change(link.id, {
            remove: true,
            baseVersion: this.versions[link.id] || 0,
          });
          this.rebuild();
          this.render();
        }),
      );
      box.classList.add("link-item-form");
      workspace.append(box);
    }
    rail.prepend(
      this.button("+ افزودن لینک", () => {
        if (this.links.length >= 50) return;
        const id = crypto.randomUUID().replaceAll("-", "").slice(0, 24);
        const link = {
          id,
          title: "",
          url: "",
          description: "",
          visibility: "HIDDEN",
          privacy: "PUBLIC",
          isActive: true,
          sortOrder: this.links.length,
        };
        this.selectedId = id;
        this.links.push(link);
        this.change(link);
        this.render();
      }),
    );
  }
  flush() {
    return this.queue.flush();
  }
  stop() {
    this.queue.stop();
    try {
      for (const k of Object.keys(localStorage))
        if (k.startsWith(`hatef.profile.pending.links.${this.id}.`))
          localStorage.removeItem(k);
    } catch {}
    this.root.replaceChildren();
    document.getElementById("links-preview")?.replaceChildren();
  }
}
