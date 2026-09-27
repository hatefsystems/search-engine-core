import { renderItem } from "./profile-content-ui.js";
for (const button of document.querySelectorAll("[data-more-section]"))
  button.onclick = async () => {
    button.disabled = true;
    try {
      const section = button.dataset.moreSection,
        profileId = button.dataset.profileId,
        offset = Number(button.dataset.offset);
      const response = await fetch(
        `/api/profiles/${encodeURIComponent(profileId)}/content/${encodeURIComponent(section)}?offset=${offset}&limit=3`,
        { cache: "no-store" },
      );
      if (!response.ok) throw new Error();
      const { data } = await response.json();
      for (const item of data.items)
        button.before(renderItem(section, item, profileId));
      button.dataset.offset = offset + data.items.length;
      button.hidden = Number(button.dataset.offset) >= data.total;
      button.textContent = "نمایش موارد بیشتر";
    } catch {
      button.textContent = "بارگیری انجام نشد؛ تلاش مجدد";
    } finally {
      button.disabled = false;
    }
  };
const share = document.getElementById("share-profile");
if (share)
  share.onclick = async () => {
    try {
      if (navigator.share)
        await navigator.share({ title: document.title, url: location.href });
      else {
        await navigator.clipboard.writeText(location.href);
        share.textContent = "آدرس کپی شد";
      }
    } catch {
      share.textContent = "آدرس صفحه را از نوار مرورگر کپی کنید";
    }
  };
