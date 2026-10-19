import { el, safeLink } from "./profile-content-ui.js";
const form = document.getElementById("people-search"),
  status = document.getElementById("people-status"),
  results = document.getElementById("people-results");
let page = 1,
  request;
const params = new URLSearchParams(location.search);
for (const field of ["q", "skill", "location", "availability"])
  form.elements[field].value = params.get(field) || "";
async function search() {
  request?.abort();
  request = new AbortController();
  status.textContent = "در حال جست‌وجو…";
  results.replaceChildren();
  try {
    const params = new URLSearchParams(new FormData(form));
    params.set("page", page);
    history.replaceState(null, "", `/people?${params}`);
    const response = await fetch(`/api/people?${params}`, {
      signal: request.signal,
      cache: "no-store",
    });
    if (!response.ok) throw new Error();
    const { data } = await response.json();
    for (const person of data.items) {
      const article = el("article", "", "people-card");
      if (person.avatarUrl) {
        const img = el("img");
        const url =
          person.avatarUrl.startsWith("/") && !person.avatarUrl.startsWith("//")
            ? person.avatarUrl
            : safeLink(person.avatarUrl);
        if (url) {
          img.src = url;
          img.alt = `تصویر ${person.name}`;
          img.loading = "lazy";
          img.onerror = () => img.remove();
          article.append(img);
        }
      }
      const a = el("a", person.name);
      a.href = `/${encodeURIComponent(person.slug)}`;
      const h = el("h2");
      h.append(a);
      article.append(
        h,
        el("p", person.title),
        el("p", person.location || ""),
        el("p", person.skills.join(" · ")),
        el("p", `${Number(person.projectCount).toLocaleString("fa-IR")} پروژه`),
      );
      results.append(article);
    }
    status.textContent = data.total
      ? `${Number(data.total).toLocaleString("fa-IR")} پروفایل پیدا شد`
      : "پروفایلی با این مشخصات پیدا نشد. فیلترها را تغییر دهید.";
    document.getElementById("people-page").textContent =
      page.toLocaleString("fa-IR");
    document.getElementById("people-prev").disabled = page === 1;
    document.getElementById("people-next").disabled = page * 20 >= data.total;
    for (const [field, target] of [
      ["skills", "people-skills"],
      ["location", "people-cities"],
    ])
      document
        .getElementById(target)
        .replaceChildren(
          ...data.facets[field].map((v) => new Option(v.value, v.value)),
        );
  } catch (error) {
    if (error.name !== "AbortError")
      status.textContent = "جست‌وجو انجام نشد؛ دوباره تلاش کنید.";
  }
}
form.onsubmit = (e) => {
  e.preventDefault();
  page = 1;
  search();
};
document.getElementById("people-prev").onclick = () => {
  page--;
  search();
};
document.getElementById("people-next").onclick = () => {
  page++;
  search();
};
search();
