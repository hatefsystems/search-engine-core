import { ProfileAutosave } from "./profile-autosave.js";
import {
  labels,
  enumLabels,
  sectionLabels,
  titleFields,
  el,
  renderItem,
} from "./profile-content-ui.js";
const clone = (value) => structuredClone(value);
const clean = (item) =>
  Object.fromEntries(
    Object.entries(item).filter(
      ([key]) => !["createdAt", "updatedAt", "displayOrder"].includes(key),
    ),
  );
const goals = {
  FIND_JOB: "یافتن شغل",
  FIND_CLIENTS: "یافتن مشتری",
  PORTFOLIO: "نمونه‌کار",
  RESEARCH_VISIBILITY: "دیده‌شدن پژوهشی",
  PERSONAL_IDENTITY: "معرفی عمومی",
};
export class ProfileContentEditor {
  constructor({
    id,
    draftId,
    previousDraftId,
    api,
    mutate,
    version,
    accept,
    status,
    conflict,
    notice,
    headerPreview,
  }) {
    Object.assign(this, {
      id,
      api,
      mutate,
      version,
      accept,
      status,
      conflict,
      notice,
      headerPreview,
    });
    this.selected = "projects";
    this.schemas = {};
    this.root = document.getElementById("advanced-editor");
    this.preview = document.getElementById("advanced-preview");
    this.queue = new ProfileAutosave({
      id: `content.${id}`,
      draftId,
      version: version(),
      status,
      conflict,
      send: async (batch) => {
        let data;
        const operations = Object.entries(batch).filter(
          ([key]) => key !== "version",
        );
        // Reserve all new item IDs first. Links between two new items (including
        // cycles) can then be written without depending on the order of typing.
        for (const [, op] of operations) {
          if (
            op.kind !== "item" ||
            (this.server.sections?.[op.section] || []).some(
              (item) => item.id === op.id,
            )
          )
            continue;
          const initial = {
            id: op.id,
            visibility: "HIDDEN",
            [titleFields[op.section]]: "",
          };
          const result = await this.mutate((version) =>
            this.api(`/api/profiles/${id}/content/${op.section}`, "POST", {
              version,
              item: initial,
            }),
          );
          this.server = clone(result.data);
          this.accept(result.data);
        }
        const priority = { item: 0, delete: 1, order: 2, layout: 3 };
        operations.sort((a, b) => priority[a[1].kind] - priority[b[1].kind]);
        for (const [key, op] of operations) {
          const result = await this.mutate(async (currentVersion) => {
            const body = { version: currentVersion };
            let path = `/api/profiles/${id}`;
            let method = "PUT";
            if (op.kind === "layout") {
              path += "/layout";
              Object.assign(body, op.value);
            } else if (op.kind === "order") {
              path += `/content/${op.section}/order`;
              body.ids = op.ids;
            } else {
              path += `/content/${op.section}`;
              const exists = (this.server.sections?.[op.section] || []).some(
                (item) => item.id === op.id,
              );
              if (op.kind === "delete") {
                if (!exists) return { data: this.server };
                path += `/${op.id}`;
                method = "DELETE";
              } else {
                method = exists ? "PUT" : "POST";
                if (exists) path += `/${op.id}`;
                body.item = op.item;
              }
            }
            return this.api(path, method, body);
          });
          data = result.data;
          this.server = clone(data);
          this.accept(data);
        }
        return data || this.server;
      },
      saved: (data) => {
        this.server = clone(data);
        this.rebuild();
        this.renderPreview();
        this.completion(data.completion);
        if (this.recovered && !Object.keys(this.queue.pending).length) {
          try {
            if (
              localStorage.getItem(this.recovered.key) === this.recovered.value
            )
              localStorage.removeItem(this.recovered.key);
          } catch {}
          this.recovered = null;
        }
      },
    });
    this.previousDraftId = previousDraftId;
    window.addEventListener("online", () => this.queue.flush());
    window.addEventListener("beforeunload", (event) => {
      if (Object.keys(this.queue.pending).length) {
        this.queue.persist();
        event.preventDefault();
        event.returnValue = "";
      }
    });
  }
  async start(data) {
    this.server = clone(data);
    try {
      const result = await this.api(`/api/profiles/${this.id}/content-schema`);
      for (const def of result.data.sections) this.schemas[def.key] = def;
      this.enums = result.data.enums || {};
      const prefix = `hatef.profile.pending.content.${this.id}.`;
      const candidates = Object.keys(localStorage)
        .filter((k) => k.startsWith(prefix) && k !== this.queue.key)
        .map((key) => ({ key, value: localStorage.getItem(key) }))
        .sort(
          (a, b) =>
            (JSON.parse(b.value)?.updatedAt || 0) -
            (JSON.parse(a.value)?.updatedAt || 0),
        );
      this.recovered =
        candidates.find((c) => c.key === prefix + this.previousDraftId) ||
        candidates[0];
      if (this.recovered)
        localStorage.setItem(this.queue.key, this.recovered.value);
    } catch (error) {
      this.notice(error.message || "بازیابی بخش‌ها انجام نشد.");
    }
    this.queue.restore({});
    this.rebuild();
    this.render();
    this.renderPreview();
    this.completion(data.completion);
  }
  receive(data) {
    this.server = clone(data);
    this.queue.version = data.version;
    this.queue.persist();
    this.rebuild();
    this.renderPreview();
    this.completion(data.completion);
  }
  rebuild() {
    this.sections = clone(this.server.sections || {});
    this.layout = clone(this.server.contentLayout || {});
    for (const op of Object.values(this.queue.pending)) {
      if (op.kind === "layout") {
        Object.assign(this.layout, clone(op.value));
        continue;
      }
      const items = (this.sections[op.section] ??= []);
      const index = items.findIndex((i) => i.id === op.id);
      if (op.kind === "delete") {
        if (index >= 0) items.splice(index, 1);
      } else if (op.kind === "order") {
        items.sort((a, b) => op.ids.indexOf(a.id) - op.ids.indexOf(b.id));
      } else if (index >= 0)
        items[index] = { ...items[index], ...clone(op.item) };
      else items.push(clone(op.item));
    }
  }
  change(section, item) {
    const base = (this.server.sections?.[section] || []).find(
      (v) => v.id === item.id,
    );
    const value = clean(item);
    const dirty = this.queue.pending[`${section}/${item.id}`]?.item || {};
    const patch = base
      ? Object.fromEntries(
          Object.entries(value).filter(
            ([key, v]) =>
              key === "id" ||
              Object.hasOwn(dirty, key) ||
              JSON.stringify(v) !== JSON.stringify(base[key]),
          ),
        )
      : value;
    this.queue.version = this.version();
    this.queue.change(`${section}/${item.id}`, {
      kind: "item",
      section,
      id: item.id,
      item: patch,
    });
    this.rebuild();
    this.renderPreview();
  }
  setLayout(patch) {
    Object.assign(this.layout, patch);
    const existing = this.queue.pending.layout?.value || {};
    this.queue.version = this.version();
    this.queue.change("layout", {
      kind: "layout",
      value: { ...existing, ...clone(patch) },
    });
    this.renderPreview();
  }
  async flush() {
    return this.queue.flush();
  }
  stop() {
    this.queue.stop();
    try {
      for (const key of Object.keys(localStorage))
        if (key.startsWith(`hatef.profile.pending.content.${this.id}.`))
          localStorage.removeItem(key);
    } catch {}
    this.root.replaceChildren();
    this.preview.replaceChildren();
  }
  resolve(data, keep) {
    this.server = clone(data);
    this.queue.version = data.version;
    this.queue.blocked = false;
    if (!keep) this.queue.pending = {};
    this.queue.persist();
    this.rebuild();
    this.render();
    this.renderPreview();
    return keep ? this.flush() : Promise.resolve(true);
  }
  button(text, action) {
    const b = el("button", text, "button secondary");
    b.type = "button";
    b.onclick = action;
    return b;
  }
  completion(value) {
    const box = document.getElementById("completion-guide");
    box.replaceChildren();
    if (!value) return;
    box.append(
      el("h2", "قدم بعدی برای صفحهٔ شما"),
      el(
        "p",
        `${Number(value.score || 0).toLocaleString("fa-IR")} از ۱۰۰ · فقط برای خودتان`,
      ),
    );
    box.append(
      el("p", "این راهنما معیار اعتبار یا رتبهٔ جست‌وجو نیست.", "hint"),
    );
    for (const suggestion of value.recommendations || [])
      box.append(
        el(
          "p",
          typeof suggestion === "string" ? suggestion : suggestion.message,
        ),
      );
  }
  render() {
    this.root.replaceChildren();
    const goalLabel = el("label", "هدف صفحه (اختیاری)");
    const goal = el("select");
    for (const [v, t] of Object.entries(goals)) goal.add(new Option(t, v));
    goal.value = this.layout.goal || "PERSONAL_IDENTITY";
    goal.onchange = () => this.setLayout({ goal: goal.value });
    goalLabel.append(goal);
    this.root.append(goalLabel);
    const privacy = el("fieldset");
    privacy.append(el("legend", "نمایش اطلاعات اصلی"));
    for (const [field, title] of Object.entries({
      showEmail: "ایمیل قبلی",
      showPhone: "تلفن قبلی",
      showLocation: "شهر",
      showAvailability: "وضعیت همکاری",
    })) {
      const label = el("label", title);
      const input = el("input");
      input.type = "checkbox";
      input.checked =
        (this.layout.privacy || this.server.privacy || {})[field] !== false;
      input.onchange = () =>
        this.setLayout({
          privacy: {
            ...(this.layout.privacy || this.server.privacy || {}),
            [field]: input.checked,
          },
        });
      label.prepend(input);
      privacy.append(label);
    }
    this.root.append(privacy);
    const nav = el("nav", "", "section-nav");
    nav.setAttribute("aria-label", "بخش‌های پروفایل");
    for (const [key, label] of Object.entries(sectionLabels)) {
      const b = this.button(label, () => {
        this.selected = key;
        this.render();
      });
      b.setAttribute("aria-pressed", String(this.selected === key));
      nav.append(b);
    }
    this.root.append(nav);
    const section = this.selected;
    const definition = this.schemas[section];
    if (!definition) {
      this.root.append(
        el("p", "دریافت فرم انجام نشد؛ صفحه را دوباره بارگیری کنید."),
      );
      return;
    }
    const panel = el("section", "", "content-editor-section");
    panel.append(el("h2", sectionLabels[section]));
    const visibility = el("label", "این بخش در صفحهٔ عمومی دیده شود");
    const checkbox = el("input");
    checkbox.type = "checkbox";
    checkbox.checked = this.layout.visibility?.[section] !== "HIDDEN";
    checkbox.onchange = () =>
      this.setLayout({
        visibility: {
          ...this.layout.visibility,
          [section]: checkbox.checked ? "PUBLIC" : "HIDDEN",
        },
      });
    visibility.prepend(checkbox);
    panel.append(visibility);
    const order = [
      ...new Set([...(this.layout.order || []), ...Object.keys(sectionLabels)]),
    ];
    const moveSection = (delta) => {
      const i = order.indexOf(section),
        j = i + delta;
      if (j < 0 || j >= order.length) return;
      [order[i], order[j]] = [order[j], order[i]];
      this.setLayout({ order });
      this.render();
    };
    panel.append(
      this.button("بخش بالاتر", () => moveSection(-1)),
      this.button("بخش پایین‌تر", () => moveSection(1)),
    );
    const items = this.sections[section] || [];
    for (const [index, item] of items.entries())
      panel.append(this.itemForm(section, item, index));
    const add = this.button(`+ افزودن ${sectionLabels[section]}`, () => {
      const item = {
        ...clone(definition.defaults),
        id: crypto.randomUUID(),
        visibility: "HIDDEN",
        evidence: [],
      };
      (this.sections[section] ??= []).push(item);
      this.change(section, item);
      this.render();
      this.root.querySelector(`[data-item-id="${item.id}"] input`)?.focus();
    });
    add.disabled = items.length >= definition.limit;
    panel.append(
      add,
      el(
        "p",
        "آیتم تازه به‌صورت پنهان ذخیره می‌شود. برای نمایش عمومی، اطلاعات ضروری آن را کامل کنید.",
        "hint",
      ),
    );
    this.root.append(panel);
  }
  itemForm(section, item, index) {
    const box = el("details", "", "content-item-form");
    box.dataset.itemId = item.id;
    box.open = true;
    const summary = el("summary", item[titleFields[section]] || "آیتم تازه");
    box.append(summary);
    const actions = el("div", "", "actions");
    const visibility = el("label", "نمایش عمومی");
    const visible = el("input");
    visible.type = "checkbox";
    visible.checked = item.visibility === "PUBLIC";
    visible.onchange = () => {
      item.visibility = visible.checked ? "PUBLIC" : "HIDDEN";
      this.change(section, item);
    };
    visibility.prepend(visible);
    actions.append(visibility);
    const featured = el("label", "برجسته");
    const star = el("input");
    star.type = "checkbox";
    star.checked = (this.layout.featured || []).some(
      (r) => r.section === section && r.id === item.id,
    );
    star.onchange = () => {
      const refs = (this.layout.featured || []).filter(
        (r) => r.section !== section || r.id !== item.id,
      );
      if (star.checked) refs.push({ section, id: item.id });
      if (refs.length > 6) {
        star.checked = false;
        this.notice("حداکثر شش مورد برجسته انتخاب کنید.");
        return;
      }
      this.setLayout({ featured: refs });
    };
    featured.prepend(star);
    actions.append(featured);
    for (const [delta, text] of [
      [-1, "بالاتر"],
      [1, "پایین‌تر"],
    ]) {
      const b = this.button(text, () => {
        const items = this.sections[section];
        [items[index], items[index + delta]] = [
          items[index + delta],
          items[index],
        ];
        this.queue.change(`order/${section}`, {
          kind: "order",
          section,
          ids: items.map((i) => i.id),
        });
        this.render();
        this.renderPreview();
      });
      b.disabled =
        index + delta < 0 || index + delta >= this.sections[section].length;
      actions.append(b);
    }
    actions.append(
      this.button("حذف آیتم", () => {
        this.sections[section] = this.sections[section].filter(
          (i) => i.id !== item.id,
        );
        if (this.queue.pending[`order/${section}`])
          this.queue.pending[`order/${section}`].ids = this.queue.pending[
            `order/${section}`
          ].ids.filter((id) => id !== item.id);
        this.layout.featured = (this.layout.featured || []).filter(
          (ref) => ref.id !== item.id,
        );
        if (this.queue.pending.layout?.value.featured)
          this.queue.pending.layout.value.featured = this.layout.featured;
        this.queue.change(`${section}/${item.id}`, {
          kind: "delete",
          section,
          id: item.id,
        });
        this.render();
        this.renderPreview();
      }),
    );
    box.append(actions);
    const update = () => {
      summary.textContent = item[titleFields[section]] || "آیتم تازه";
      this.change(section, item);
    };
    for (const [field, value] of Object.entries(
      this.schemas[section].defaults,
    )) {
      if (field === "media") continue;
      box.append(this.field(field, item, update, value));
    }
    box.append(this.field("evidence", item, update, []));
    if (section === "recommendations")
      box.append(
        el(
          "p",
          "برای نمایش عمومی، لینک منبع لازم است. این توصیه‌نامه خوداظهاری است و تأیید نویسنده محسوب نمی‌شود.",
          "hint",
        ),
      );
    if (section === "projects") {
      for (const media of item.media || [])
        box.append(
          this.button(`حذف تصویر: ${media.alt || "بدون توضیح"}`, () => {
            item.media = item.media.filter((m) => m.id !== media.id);
            update();
            this.render();
          }),
        );
      const alt = el("input");
      alt.placeholder = "توضیح تصویر پروژه";
      alt.setAttribute("aria-label", "توضیح تصویر پروژه");
      const input = el("input");
      input.type = "file";
      input.accept = "image/jpeg,image/png,image/webp";
      input.setAttribute("aria-label", "افزودن تصویر پروژه");
      input.onchange = async () => {
        const file = input.files[0];
        if (!file) return;
        if (file.size > 5 * 1024 * 1024) {
          this.notice("حداکثر اندازهٔ تصویر ۵ مگابایت است.");
          return;
        }
        input.disabled = true;
        try {
          if (!(await this.flush()))
            throw new Error("ابتدا ذخیرهٔ تغییرات را کامل کنید.");
          const image = await new Promise((resolve, reject) => {
            const reader = new FileReader();
            reader.onload = () => resolve(reader.result);
            reader.onerror = reject;
            reader.readAsDataURL(file);
          });
          const result = await this.mutate((version) =>
            this.api(
              `/api/profiles/${this.id}/projects/${item.id}/media`,
              "POST",
              { image, alt: alt.value, version },
            ),
          );
          this.accept(result.data);
          this.receive(result.data);
          this.render();
        } catch (error) {
          if (error.status === 409) {
            this.queue.blocked = true;
            this.conflict();
          }
          this.notice(error.message || "بارگذاری انجام نشد.");
        } finally {
          input.disabled = false;
          input.value = "";
        }
      };
      box.append(
        alt,
        input,
        el(
          "p",
          "JPEG، PNG یا WebP ثابت؛ حداکثر ۵ مگابایت و ده تصویر. ویدئو و مدارک را با لینک در شواهد اضافه کنید.",
          "hint",
        ),
      );
    }
    return box;
  }
  field(field, item, update, defaultValue) {
    const wrapper = el("label", labels[field] || field);
    const value = item[field] ?? clone(defaultValue);
    item[field] = value;
    if (["evidence", "links"].includes(field)) {
      const group = el("fieldset");
      group.append(el("legend", labels[field]));
      const render = () => {
        group.querySelectorAll(".reference-form").forEach((n) => n.remove());
        for (const reference of item[field]) {
          const row = el("div", "", "reference-form");
          for (const key of field === "evidence"
            ? ["type", "title", "url", "description"]
            : ["title", "url"])
            row.append(this.field(key, reference, update, ""));
          row.append(
            this.button("حذف منبع", () => {
              item[field] = item[field].filter((r) => r !== reference);
              update();
              render();
            }),
          );
          group.append(row);
        }
      };
      group.append(
        this.button("+ افزودن منبع", () => {
          if (item[field].length >= 10) return;
          item[field].push(
            field === "evidence"
              ? {
                  id: crypto.randomUUID(),
                  type: "LINK",
                  title: "",
                  url: "",
                  description: "",
                  verificationStatus: "SELF_REPORTED",
                }
              : { title: "", url: "" },
          );
          update();
          render();
        }),
      );
      render();
      return group;
    }
    if (value && typeof value === "object" && !Array.isArray(value)) {
      const group = el("fieldset");
      group.append(el("legend", labels[field]));
      const calendar = el("select");
      calendar.add(new Option("شمسی", "persian"));
      calendar.add(new Option("میلادی", "gregory"));
      calendar.value = value.calendar || "persian";
      calendar.setAttribute("aria-label", `تقویم ${labels[field]}`);
      calendar.onchange = () => {
        value.calendar = calendar.value;
        update();
      };
      group.append(calendar);
      for (const [key, label] of [
        ["year", "سال"],
        ["month", "ماه (اختیاری)"],
        ["day", "روز (اختیاری)"],
      ]) {
        const input = el("input");
        input.type = "number";
        input.min = "0";
        input.placeholder = label;
        input.setAttribute("aria-label", `${label} ${labels[field]}`);
        input.value = value[key] || "";
        input.oninput = () => {
          value[key] = Number(input.value) || 0;
          update();
        };
        group.append(input);
      }
      return group;
    }
    const target = {
      skillIds: "skills",
      projectIds: "projects",
      experienceIds: "experiences",
      certificationIds: "certifications",
    }[field];
    if (target) {
      const group = el("fieldset");
      group.append(el("legend", labels[field]));
      for (const other of this.sections[target] || []) {
        const label = el(
          "label",
          other[titleFields[target]] || "آیتم در حال تکمیل",
        );
        const input = el("input");
        input.type = "checkbox";
        input.checked = value.includes(other.id);
        input.onchange = () => {
          item[field] = input.checked
            ? [...item[field], other.id]
            : item[field].filter((id) => id !== other.id);
          update();
        };
        label.prepend(input);
        group.append(label);
      }
      if (!group.querySelector("input"))
        group.append(
          el(
            "p",
            `ابتدا یک مورد در ${sectionLabels[target]} اضافه کنید.`,
            "hint",
          ),
        );
      return group;
    }
    let input;
    const choices =
      field === "type" && Object.hasOwn(item, "value")
        ? ["EMAIL", "PHONE", "LINK"]
        : field === "type" && Object.hasOwn(item, "status")
          ? [
              "COLLABORATION",
              "FREELANCE",
              "CONSULTING",
              "TEACHING",
              "MENTORSHIP",
              "SPEAKING",
            ]
          : this.enums[field];
    if (choices) {
      input = el("select");
      input.add(new Option("انتخاب نکرده‌ام", ""));
      for (const option of choices)
        input.add(new Option(enumLabels[option] || option, option));
      input.value = value;
    } else if (typeof value === "boolean") {
      input = el("input");
      input.type = "checkbox";
      input.checked = value;
    } else if (typeof value === "number") {
      input = el("input");
      input.type = "number";
      input.min = "0";
      input.step = field === "yearsOfExperience" ? "0.5" : "1";
      input.value = value || "";
    } else if (
      Array.isArray(value) ||
      [
        "description",
        "summary",
        "problem",
        "solution",
        "architecture",
        "content",
      ].includes(field)
    ) {
      input = el("textarea");
      input.rows = 3;
      input.value = Array.isArray(value) ? value.join("\n") : value;
      if (Array.isArray(value))
        wrapper.append(el("small", "هر مورد در یک خط", "hint"));
    } else {
      input = el("input");
      input.value = value;
    }
    input.setAttribute("aria-label", labels[field] || field);
    input.dir = "auto";
    input.addEventListener("input", () => {
      item[field] =
        typeof value === "boolean"
          ? input.checked
          : typeof value === "number"
            ? Number(input.value) || 0
            : Array.isArray(value)
              ? input.value.split("\n").filter((s) => s.trim())
              : input.value;
      update();
    });
    wrapper.append(input);
    return wrapper;
  }
  renderPreview() {
    this.preview.replaceChildren();
    const publicSections = {};
    for (const [section, items] of Object.entries(this.sections || {}))
      if (this.layout.visibility?.[section] !== "HIDDEN")
        publicSections[section] = items.filter(
          (i) => i.visibility === "PUBLIC",
        );
    const privacy = this.layout.privacy || this.server.privacy || {};
    if (privacy.showAvailability === false) delete publicSections.availability;
    if (publicSections.contacts)
      publicSections.contacts = publicSections.contacts.filter(
        (item) =>
          !(item.type === "EMAIL" && !privacy.showEmail) &&
          !(item.type === "PHONE" && !privacy.showPhone),
      );
    this.headerPreview?.();
    // References are resolved only against visible items, including in the local preview.
    const order = [
      ...new Set([
        ...(this.layout.order || []),
        ...Object.keys(publicSections),
      ]),
    ];
    for (const section of order) {
      const items = publicSections[section];
      if (!items?.length) continue;
      const block = el("section", "", "profile-content-section");
      block.append(el("h3", sectionLabels[section]));
      for (const original of items) {
        const item = clone(original);
        for (const [key, target] of Object.entries({
          skillIds: "skills",
          projectIds: "projects",
          experienceIds: "experiences",
          certificationIds: "certifications",
        }))
          if (item[key])
            item[key] = item[key].filter((id) =>
              publicSections[target]?.some((i) => i.id === id),
            );
        block.append(renderItem(section, item, this.id, publicSections));
      }
      this.preview.append(block);
    }
  }
}
