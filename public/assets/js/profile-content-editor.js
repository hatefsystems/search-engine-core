import { ProfileAutosave } from "./profile-autosave.js";
import {
  labels,
  enumLabels,
  sectionLabels,
  titleFields,
  el,
  dateText,
} from "./profile-content-ui.js";
const clone = (value) => structuredClone(value);
const clean = (item) =>
  Object.fromEntries(
    Object.entries(item).filter(
      ([key]) => !["createdAt", "updatedAt", "displayOrder"].includes(key),
    ),
  );
// Keep the visual form order independent of JSON object key ordering from the API.
const fieldOrder = {
  experiences: ["roleTitle","organizationName","employmentType","locationType","location","startDate","endDate","isCurrent","summary","responsibilities","achievements","technologies","skillIds","projectIds","organizationProfileId"],
  projects: ["title","projectType","subtitle","description","role","organization","startDate","endDate","isOngoing","problem","solution","architecture","challenges","outcomes","responsibilities","technologies","skillIds","experienceIds","collaborators","links"],
  skills: ["name","category","proficiencyLevel","yearsOfExperience","firstUsedYear","lastUsedYear","isCurrentlyUsing","description","projectIds","experienceIds","certificationIds"],
  education: ["kind","institutionName","degree","fieldOfStudy","startDate","endDate","isCurrent","description","grade","activities","achievements","institutionProfileId"],
  certifications: ["name","issuingOrganization","issueDate","expirationDate","credentialId","credentialUrl","skillIds"],
  publications: ["type","title","publisher","authors","description","publicationDate","url","topics","skillIds","doi","isbn"],
  openSource: ["repositoryName","contributionType","role","platform","repositoryUrl","description","technologies","skillIds"],
  services: ["title","description","deliveryMode","availability","pricingMode","price","currency","contactMethod","technologies","skillIds"],
  achievements: ["title","description","issuer","date","url"],
  recommendations: ["authorName","authorTitle","relationship","content","sourceUrl"],
  contacts: ["type","label","value"], availability: ["type","status","description"], about: ["title","description"]
};
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
    this.selected = "basic";
    this.selectedItems = {};
    this.filters = {};
    try { this.selected = sessionStorage.getItem(`hatef.editor.section.${id}`) || "basic"; } catch {}
    this.schemas = {};
    this.root = document.getElementById("advanced-editor");

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
  select(section, focus = true) {
    this.selected = section;
    try { sessionStorage.setItem(`hatef.editor.section.${this.id}`, section); } catch {}
    this.render();
    this.renderPreview();
    if (focus) document.querySelector(this.selected === "basic" ? "#basic-editor h2" : "#advanced-editor h2")?.focus({preventScroll:true});
  }
  renderNavigation() {
    const nav = document.getElementById("editor-section-nav");
    nav.replaceChildren();
    const sections = { basic: "اطلاعات اصلی و معرفی", experiences: sectionLabels.experiences,
      projects: sectionLabels.projects, skills: sectionLabels.skills, education: sectionLabels.education,
      certifications: sectionLabels.certifications, publications: sectionLabels.publications,
      openSource: sectionLabels.openSource, services: sectionLabels.services,
      achievements: sectionLabels.achievements, languages: sectionLabels.languages,
      recommendations: sectionLabels.recommendations, contacts: sectionLabels.contacts,
      links: "لینک‌های صفحه", availability: sectionLabels.availability, about: sectionLabels.about };
    const symbols = {basic:"◉",experiences:"▣",projects:"▱",skills:"◇",education:"▰",certifications:"▧",
      publications:"▤",openSource:"‹/›",services:"▢",achievements:"☆",languages:"◎",recommendations:"❞",contacts:"✉",links:"↗",availability:"♧",about:"☰"};
    for (const [key, label] of Object.entries(sections)) {
      const button = this.button(label, () => this.select(key));
      button.dataset.section = key;
      button.setAttribute("aria-label", label);
      button.setAttribute("aria-pressed", String(this.selected === key));
      const icon = el("span", symbols[key], "section-symbol"); icon.setAttribute("aria-hidden", "true");
      button.prepend(icon);
      nav.append(button);
    }
    const picker = document.getElementById("section-picker-options");
    picker.replaceChildren();
    for (const [key, label] of Object.entries(sections)) {
      if (key === "basic") continue;
      picker.append(this.button(label, () => {
        document.getElementById("section-picker").close(); this.select(key);
      }));
    }
  }
  renderSettings() {
    const settings = document.getElementById("profile-settings");
    settings.replaceChildren();
    const goalLabel = el("label", "هدف صفحه (اختیاری)");
    const goal = el("select");
    for (const [v, t] of Object.entries(goals)) goal.add(new Option(t, v));
    goal.value = this.layout.goal || "PERSONAL_IDENTITY";
    goal.onchange = () => this.setLayout({ goal: goal.value });
    goalLabel.append(goal); settings.append(goalLabel);
    const privacy = el("fieldset"); privacy.append(el("legend", "نمایش اطلاعات اصلی"));
    for (const [field, title] of Object.entries({ showEmail: "ایمیل", showPhone: "تلفن", showLocation: "شهر", showAvailability: "وضعیت همکاری" })) {
      const label = el("label", title); const input = el("input"); input.type = "checkbox";
      input.checked = (this.layout.privacy || this.server.privacy || {})[field] !== false;
      input.onchange = () => this.setLayout({ privacy: { ...(this.layout.privacy || this.server.privacy || {}), [field]: input.checked } });
      label.prepend(input); privacy.append(label);
    }
    settings.append(privacy);
  }
  render() {
    if (!["basic", "links", ...Object.keys(sectionLabels)].includes(this.selected)) this.selected = "basic";
    this.renderNavigation(); this.renderSettings();
    document.getElementById("basic-editor").hidden = this.selected !== "basic";
    document.getElementById("links-editor").hidden = !["contacts", "links"].includes(this.selected);
    this.root.hidden = ["basic", "links"].includes(this.selected);
    this.root.replaceChildren();
    if (this.root.hidden) return;
    const section = this.selected, definition = this.schemas[section];
    const heading = el("div", "", "section-heading");
    const title = el("h2", sectionLabels[section]); title.tabIndex = -1;
    heading.append(title, el("p", {
      experiences:"سوابق شغلی و پروژه‌های کاری خود را اضافه کنید.", projects:"پروژه‌ها و مطالعه‌های موردی را همراه با جزئیات فنی معرفی کنید.",
      skills:"مهارت‌های خود را اضافه کنید و به تجربه‌ها و پروژه‌های مرتبط پیوند دهید.", education:"تحصیلات دانشگاهی، دوره‌ها و تجربه‌های یادگیری شما.",
      contacts:"راه‌های ارتباطی و لینک‌های حرفه‌ای خود را مدیریت کنید.", services:"خدماتی که به دیگران ارائه می‌دهید را معرفی کنید."
    }[section] || "اطلاعات این بخش را تکمیل کنید و نحوهٔ نمایش آن را انتخاب کنید.", "hint"));
    this.root.append(heading);
    if (!definition) { this.root.append(el("p", "دریافت فرم انجام نشد؛ صفحه را دوباره بارگیری کنید.")); return; }
    const panel = el("section", "", "content-editor-section");
    const settings = el("details", "", "section-settings"); settings.append(el("summary", "تنظیمات نمایش و ترتیب بخش"));
    const visibility = el("label", "این بخش در صفحهٔ عمومی دیده شود");
    const checkbox = el("input"); checkbox.type = "checkbox";
    checkbox.checked = this.layout.visibility?.[section] !== "HIDDEN";
    checkbox.onchange = () => this.setLayout({visibility:{...this.layout.visibility,[section]:checkbox.checked?"PUBLIC":"HIDDEN"}});
    visibility.prepend(checkbox); settings.append(visibility);
    const order = [...new Set([...(this.layout.order || []), ...Object.keys(sectionLabels)])];
    for (const [delta, label] of [[-1,"بخش بالاتر"],[1,"بخش پایین‌تر"]]) {
      const button = this.button(label, () => {
        const i = order.indexOf(section), j = i + delta;
        if (j < 0 || j >= order.length) return;
        [order[i],order[j]]=[order[j],order[i]]; this.setLayout({order}); this.render();
      });
      button.disabled = order.indexOf(section)+delta<0 || order.indexOf(section)+delta>=order.length;
      settings.append(button);
    }
    const items = this.sections[section] || [];
    const add = this.button(`+ افزودن ${sectionLabels[section]}`, () => {
      const item = {...clone(definition.defaults),id:crypto.randomUUID(),visibility:"HIDDEN",evidence:[]};
      this.selectedItems[section] = item.id; this.filters[section] = "";
      this.change(section,item); this.render();
      this.root.querySelector(".item-fields input, .item-fields select, .item-fields textarea")?.focus();
    });
    add.className = "button primary add-content-item"; add.disabled = items.length >= definition.limit;
    const split = el("div", "", "item-workspace");
    const rail = el("aside", "", "item-rail"); rail.setAttribute("aria-label", `فهرست ${sectionLabels[section]}`);
    rail.append(el("h3", `فهرست ${sectionLabels[section]}`), add);
    const search = el("input"); search.type = "search"; search.placeholder = "جستجو در این بخش…";
    search.setAttribute("aria-label", "جستجو در موارد"); search.value = this.filters[section] || "";
    const list = el("div", "", "item-list");
    const filter = () => {
      const query = search.value.trim().toLocaleLowerCase(); this.filters[section] = search.value;
      for (const button of list.children) button.hidden = !button.textContent.toLocaleLowerCase().includes(query);
    };
    search.oninput = filter; if (items.length) rail.append(search);
    if (!items.some(i => i.id === this.selectedItems[section])) this.selectedItems[section] = items[0]?.id;
    for (const item of items) {
      const button = this.button("", () => { this.selectedItems[section] = item.id; this.render(); });
      button.className = "item-choice"; button.dataset.itemChoice = item.id;
      button.setAttribute("aria-pressed", String(item.id === this.selectedItems[section]));
      const name = el("strong", item[titleFields[section]] || "مورد تازه", "item-choice-title"); name.dir="auto";
      const subtitle = item.organizationName || item.institutionName || item.issuingOrganization || item.category || item.description || "";
      button.append(name, el("span", subtitle, "item-choice-subtitle"), el("small", item.visibility === "PUBLIC" ? "عمومی" : "پیش‌نویس خصوصی", "item-privacy"));
      if (item.startDate?.year) button.append(el("span", `${dateText(item.startDate)} · ${item.isCurrent || item.isOngoing ? "اکنون" : dateText(item.endDate)}`, "item-choice-subtitle"));
      if (this.layout.featured?.some(ref => ref.section === section && ref.id === item.id)) button.prepend(el("span", "★ مورد برجسته", "featured-label"));
      list.append(button);
    }
    filter(); rail.append(list); split.append(rail);
    const selected = items.find(i=>i.id === this.selectedItems[section]);
    if (selected) split.append(this.itemForm(section, selected, items.indexOf(selected)));
    else {
      const empty = el("div", "", "editor-empty");
      empty.append(el("span", "＋", "empty-symbol"),el("h3", "داستان حرفه‌ای شما از اینجا شروع می‌شود"),el("p", "اولین مورد را اضافه کنید. تا زمان انتخاب نمایش عمومی، خصوصی می‌ماند.","hint")); split.append(empty);
    }
    panel.append(split, settings); this.root.append(panel);
  }
  itemForm(section, item, index) {
    const box = el("section", "", "content-item-form");
    box.dataset.itemId = item.id;
    const summary = el("h3", item[titleFields[section]] || "مورد تازه");
    box.append(summary);
    const actions = el("div", "", "actions");
    const visibility = el("label", "نمایش عمومی");
    const visible = el("input");
    visible.type = "checkbox";
    visible.checked = item.visibility === "PUBLIC";
    visible.onchange = () => {
      item.visibility = visible.checked ? "PUBLIC" : "HIDDEN";
      const badge = this.root.querySelector(`[data-item-choice="${item.id}"] .item-privacy`);
      if (badge) badge.textContent = visible.checked ? "عمومی" : "پیش‌نویس خصوصی";
      this.change(section, item);
    };
    visibility.prepend(visible);
    visibility.classList.add("visibility-choice");
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
    featured.classList.add("featured-choice");
    actions.append(featured);
    const duplicate = this.button("کپی", () => {
      const copy = {...clean(clone(item)), id:crypto.randomUUID(), visibility:"HIDDEN", evidence:clone(item.evidence || []).map(e=>({...e,id:crypto.randomUUID()}))};
      // Uploaded files belong to the original item; do not copy media ownership.
      if (copy.media) copy.media = [];
      this.selectedItems[section] = copy.id; this.change(section,copy); this.render();
    });
    duplicate.disabled = this.sections[section].length >= this.schemas[section].limit;
    actions.append(duplicate);
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
        if (!window.confirm("این مورد از پروفایل حذف شود؟")) return;
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
      summary.textContent = item[titleFields[section]] || "مورد تازه";
      const choice = this.root.querySelector(`[data-item-choice="${item.id}"]`);
      if (choice) choice.querySelector(".item-choice-title").textContent = summary.textContent;
      this.change(section, item);
    };
    const fields = el("div", "", "item-fields");
    const defaults = this.schemas[section].defaults;
    const keys = [...new Set([...(fieldOrder[section] || []), ...Object.keys(defaults)])].filter(key => Object.hasOwn(defaults,key));
    for (const field of keys) {
      const value = defaults[field];
      if (["media", "visibility", "id", "displayOrder", "createdAt", "updatedAt", "evidence"].includes(field)) continue;
      const control = this.field(field, item, update, value);
      control.dataset.field = field;
      if (control.querySelector("textarea") || Array.isArray(value) || ["description","summary","content","value","location","isCurrent","isOngoing"].includes(field)) control.classList.add("wide-field");
      fields.append(control);
    }
    const syncEndDate = () => {
      const ongoing = fields.querySelector('[data-field="isCurrent"] input, [data-field="isOngoing"] input');
      const endDate = fields.querySelector('[data-field="endDate"]');
      if (endDate && ongoing) endDate.disabled = ongoing.checked;
    };
    fields.addEventListener("input", syncEndDate); syncEndDate();
    box.append(fields);
    const evidence = el("details", "", "evidence-details"); evidence.append(el("summary", "شواهد و منابع (اختیاری)"),this.field("evidence", item, update, [])); box.append(evidence);
    if (section === "recommendations")
      box.append(
        el(
          "p",
          "برای نمایش عمومی، لینک منبع لازم است. این توصیه‌نامه خوداظهاری است و تأیید نویسنده محسوب نمی‌شود.",
          "hint",
        ),
      );
    if (["projects", "experiences"].includes(section)) box.append(this.mediaPanel(section, item, update));
    return box;
  }
  mediaPanel(section, item, update) {
    const title = section === "projects" ? "پروژه" : "سابقه";
    const panel = el("fieldset", "", "item-media-panel");
    panel.append(el("legend", `تصاویر ${title}`), el("p", "تصاویر همین مورد را اضافه کنید. اولین تصویر در ابتدای گالری نمایش داده می‌شود؛ وضعیت نمایش تصاویر از همین آیتم پیروی می‌کند.", "hint"));
    const list = el("div", "", "item-media-list");
    const render = () => {
      list.replaceChildren();
      for (const [index, media] of (item.media || []).entries()) {
        const row = el("div", "", "item-media-row"); row.dataset.mediaId = media.id;
        const image = el("img"); image.src = `/api/profiles/${encodeURIComponent(this.id)}/media/${encodeURIComponent(media.id)}`;
        image.alt = media.alt || `تصویر ${index + 1} ${title}`;
        const label = el("label", `توضیح تصویر ${index + 1}`), caption = el("input");
        caption.value = media.alt || ""; caption.maxLength = 300; caption.dir = "auto";
        caption.oninput = () => {media.alt = caption.value; image.alt = media.alt; update();}; label.append(caption);
        const controls = el("div", "", "item-media-actions");
        for (const [delta, text] of [[-1,"تصویر قبلی"],[1,"تصویر بعدی"]]) {
          const button = this.button(text, () => {
            [item.media[index], item.media[index + delta]] = [item.media[index + delta], item.media[index]];
            update(); render(); list.children[index + delta].querySelector("button").focus();
          });
          button.disabled = index + delta < 0 || index + delta >= item.media.length; controls.append(button);
        }
        controls.append(this.button("حذف تصویر", () => {item.media = item.media.filter(m => m.id !== media.id); update(); render();}));
        row.append(image,label,controls); list.append(row);
      }
    };
    render();
    const altLabel = el("label", "توضیح تصویر جدید"), alt = el("input"); alt.maxLength = 300; alt.dir = "auto"; altLabel.append(alt);
    const uploadLabel = el("label", `افزودن تصویر ${title}`), input = el("input");
    input.type = "file"; input.accept = "image/jpeg,image/png,image/webp"; uploadLabel.append(input);
    const progress = el("p", "", "hint"); progress.setAttribute("role", "status");
    input.onchange = async () => {
      const file = input.files[0]; if (!file || this.uploading) return;
      if (file.size > 5 * 1024 * 1024) {this.notice("حداکثر اندازهٔ تصویر ۵ مگابایت است."); input.value = ""; return;}
      if ((item.media || []).length >= 10) {this.notice("حداکثر ده تصویر برای هر آیتم مجاز است."); input.value = ""; return;}
      const form = document.getElementById("profile-form"), publish = document.getElementById("publish");
      this.uploading = true; form.inert = true; publish.disabled = true; panel.setAttribute("aria-busy", "true");
      progress.textContent = "در حال بارگذاری تصویر…"; this.notice("");
      try {
        if (!(await this.flush())) throw new Error("ابتدا ذخیرهٔ تغییرات را کامل کنید.");
        const image = await new Promise((resolve, reject) => {
          const reader = new FileReader(); reader.onload = () => resolve(reader.result); reader.onerror = reject; reader.readAsDataURL(file);
        });
        const result = await this.mutate(version => this.api(`/api/profiles/${this.id}/${section}/${item.id}/media`, "POST", {image, alt:alt.value, version}));
        this.accept(result.data); this.receive(result.data); this.render(); this.notice("تصویر ذخیره شد.");
      } catch (error) {
        if (error.status === 409) {this.queue.blocked = true; this.conflict();}
        this.notice(error.message || "بارگذاری انجام نشد؛ دوباره تلاش کنید.");
      } finally {
        this.uploading = false; form.inert = false; publish.disabled = false; panel.removeAttribute("aria-busy"); progress.textContent = ""; input.value = "";
      }
    };
    panel.append(list,altLabel,uploadLabel,progress,el("p", "JPEG، PNG یا WebP ثابت؛ حداکثر ۵ مگابایت و ده تصویر. ویدئو و مدارک را با لینک در شواهد اضافه کنید.", "hint"));
    return panel;
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
      const group = el("fieldset", "", "partial-date");
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
        label.classList.add("reference-chip");
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
    if (typeof value === "string" && !choices) {
      const long = ["description","summary","problem","solution","architecture","content"].includes(field);
      input.maxLength = long ? 5000 : field.endsWith("Url") || ["url","value"].includes(field) ? 2048 : 200;
      if (long) {
        const counter = el("small", "", "field-counter");
        const count = () => { counter.textContent = `${[...input.value].length.toLocaleString("fa-IR")} / ${input.maxLength.toLocaleString("fa-IR")}`; };
        input.addEventListener("input", count); count(); wrapper.append(counter);
      }
    }
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
    this.headerPreview?.();
  }
}
