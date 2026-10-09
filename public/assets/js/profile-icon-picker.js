// Same-origin catalog images only; uploaded images retain item media ownership.
const canonical = /^(?:lucide:[a-z0-9][a-z0-9-]*|simple-icons:[a-z0-9][a-z0-9_-]*|iconify:[a-z0-9][a-z0-9-]*:[a-z0-9][a-z0-9-]*)$/;
export function iconUrl(item, profileId) {
  if (item.iconMode === "custom" && item.iconMediaId && item.media?.some(m => m.id === item.iconMediaId))
    return `/api/profiles/${encodeURIComponent(profileId)}/media/${encodeURIComponent(item.iconMediaId)}`;
  if (["auto", "manual"].includes(item.iconMode) && item.iconId?.length <= 200 && canonical.test(item.iconId))
    return `/assets/icons/${item.iconId.replaceAll(":", "/")}.svg`;
  return "";
}
const node = (tag, text = "") => { const n = document.createElement(tag); n.textContent = text; return n; };
const button = (label, action) => { const n = node("button", label); n.type = "button"; n.onclick = action; return n; };
const select = (label, values) => {
  const wrap = node("label", label), input = node("select"); input.setAttribute("aria-label", label);
  for (const [value, text] of values) { const option = node("option", text); option.value = value; input.append(option); }
  wrap.append(input); return [wrap, input];
};
async function request(url, init, signal) {
  const response = await fetch(url, {credentials:"same-origin", ...init, signal});
  if (!response.ok) throw new Error("icon service unavailable");
  return response.json();
}
export function mountIconPicker({item, profileId, titleField = "title", onChange}) {
  const panel = node("fieldset"); panel.className = "icon-picker"; panel.append(node("legend", "آیکن"));
  const [modeLabel, mode] = select("روش انتخاب آیکن", [["auto","پیشنهاد خودکار"],["manual","انتخاب دستی"],["custom","تصویر دلخواه"],["none","بدون آیکن"]]);
  mode.value = item.iconMode || "none";
  const preview = node("img"); preview.width = 32; preview.height = 32; preview.alt = "آیکن انتخاب‌شده";
  const status = node("p"); status.setAttribute("role","status"); status.setAttribute("aria-live","polite");
  const suggestions = node("div"); suggestions.className = "icon-results";
  const custom = node("div");
  const details = node("details"); details.append(node("summary","جستجو در آیکن‌ها"));
  const filters = node("div"); filters.className = "icon-filters";
  const search = node("input"); search.type = "search"; search.placeholder = "نام یا کاربرد آیکن"; search.maxLength = 500; search.setAttribute("aria-label","جستجوی آیکن");
  const [sourceLabel, source] = select("منبع", [["","همه"],["lucide","Lucide"],["iconify","Iconify"],["simple-icons","Simple Icons"]]);
  const [styleLabel, style] = select("سبک", [["","همه"],["outline","خطی"],["filled","توپر"],["mixed","ترکیبی"],["brand","نشان تجاری"]]);
  const [categoryLabel, category] = select("دسته", [["","همه"]]);
  filters.append(search, sourceLabel, styleLabel, categoryLabel);
  const results = node("div"); results.className = "icon-results";
  const pages = node("div"); pages.className = "icon-pages";
  let offset = 0, nextOffset = null, lastText = null, autoTimer, searchTimer, autoGeneration = 0, searchGeneration = 0, autoController, searchController;
  const previous = button("قبلی",()=>{offset=Math.max(0,offset-24);browse();});
  const next = button("بعدی",()=>{if(nextOffset!==null){offset=nextOffset;browse();}});
  pages.append(previous,next); details.append(filters,results,pages); panel.append(modeLabel,preview,status,suggestions,custom,details);
  function refresh() {
    mode.value = item.iconMode || "none"; const url = iconUrl(item,profileId); preview.hidden = !url;
    if (url) preview.src = url; else preview.removeAttribute("src");
    custom.replaceChildren(); custom.hidden = item.iconMode !== "custom";
    if (!custom.hidden) {
      const [label, input] = select("تصویر همین آیتم", [["","انتخاب تصویر"], ...(item.media || []).map((m,i)=>[m.id,m.alt || `تصویر ${i+1}`])]);
      input.value = item.iconMediaId || ""; input.onchange = ()=>{item.iconMediaId=input.value; if(!input.value)item.iconMode="none";refresh();onChange();};
      custom.append(label); if (!item.media?.length) custom.append(node("p","ابتدا تصویر را از بخش تصاویر همین آیتم بارگذاری کنید."));
    }
  }
  function choose(icon) {
    if (!canonical.test(icon.id) || icon.id.length > 200) return;
    ++autoGeneration; autoController?.abort(); item.iconMode="manual";item.iconId=icon.id;item.iconMediaId="";refresh();onChange();
  }
  function fill(container, icons) {
    container.replaceChildren();
    for (const icon of icons) {
      if (!canonical.test(icon.id)) continue;
      const b = button(icon.labelFa || icon.labelEn || icon.name || icon.id,()=>choose(icon)); b.title=icon.id;b.dataset.iconId=icon.id;
      const image=node("img");image.src=iconUrl({iconMode:"manual",iconId:icon.id},profileId);image.alt="";image.width=24;image.height=24;image.loading="lazy";b.prepend(image);container.append(b);
    }
  }
  for (const grid of [results,suggestions]) grid.addEventListener("keydown",event=>{
    if(!["ArrowLeft","ArrowRight","ArrowUp","ArrowDown"].includes(event.key))return;
    const choices=[...grid.querySelectorAll("button")],i=choices.indexOf(document.activeElement);if(i<0)return;
    event.preventDefault();choices[(i+(event.key==="ArrowLeft"||event.key==="ArrowUp"?-1:1)+choices.length)%choices.length]?.focus();
  });
  async function suggest() {
    if(item.iconMode!=="auto" || !panel.isConnected)return;
    const text = [item[titleField],item.description || item.summary].filter(Boolean).join(" ").slice(0,2000); if(!text)return;
    const generation=++autoGeneration;autoController?.abort();const controller=autoController=new AbortController();const timeout=setTimeout(()=>controller.abort(),3000);
    try {
      const data=await request("/api/icons/suggest",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({text,limit:3})},controller.signal);
      if(generation!==autoGeneration || item.iconMode!=="auto" || !panel.isConnected)return;
      fill(suggestions,data.suggestions || []);status.textContent=data.lowConfidence?"پیشنهاد قطعی نیست؛ می‌توانید دستی انتخاب کنید.":"پیشنهادهای آیکن";
      const best=data.suggestions?.[0];
      if(!data.lowConfidence && best && canonical.test(best.id) && best.id!==item.iconId){item.iconId=best.id;item.iconMediaId="";refresh();onChange();}
    } catch { if(generation===autoGeneration)status.textContent="پیشنهاد آیکن فعلاً در دسترس نیست؛ آیکن ذخیره‌شده حفظ می‌شود."; }
    finally {clearTimeout(timeout);}
  }
  async function browse() {
    const generation=++searchGeneration;searchController?.abort();const controller=searchController=new AbortController();const timeout=setTimeout(()=>controller.abort(),3000);
    const params=new URLSearchParams({q:search.value,source:source.value,style:style.value,category:category.value,limit:24,offset});
    try {
      const data=await request(`/api/icons/search?${params}`,{},controller.signal);
      if(generation!==searchGeneration || !panel.isConnected)return;
      fill(results,data.items || []);nextOffset=data.nextOffset;previous.disabled=offset===0;next.disabled=nextOffset===null;
      status.textContent=data.items?.length?"برای انتخاب، آیکن را بزنید.":"آیکنی پیدا نشد.";
    } catch {if(generation===searchGeneration)status.textContent="کاتالوگ آیکن فعلاً در دسترس نیست.";}
    finally{clearTimeout(timeout);}
  }
  mode.onchange=()=>{
    ++autoGeneration;autoController?.abort();item.iconMode=mode.value;
    if(item.iconMode==="custom"){
      item.iconMediaId=item.media?.[0]?.id || "";
      if(!item.iconMediaId){item.iconMode="none";status.textContent="ابتدا تصویر را از بخش تصاویر همین آیتم بارگذاری کنید.";}
    } else item.iconMediaId="";
    refresh();onChange();if(item.iconMode==="auto")suggest();
  };
  for(const control of [search,source,style,category])control.addEventListener("input",()=>{clearTimeout(searchTimer);offset=0;searchTimer=setTimeout(browse,350);});
  let categoriesLoaded=false;
  details.addEventListener("toggle",async()=>{
    if(!details.open)return;browse();
    if(!categoriesLoaded){categoriesLoaded=true;try{const data=await request("/api/icons/categories",{},AbortSignal.timeout(3000));for(const value of data.categories || []){const option=node("option",value.category);option.value=value.category;category.append(option);}}catch{categoriesLoaded=false;}}
  });
  panel.notify=()=>{
    refresh();const text=JSON.stringify([item[titleField],item.description,item.summary]);
    if(text!==lastText){lastText=text;++autoGeneration;autoController?.abort();clearTimeout(autoTimer);if(item.iconMode==="auto")autoTimer=setTimeout(suggest,450);}
  };
  panel.notify();return panel;
}
