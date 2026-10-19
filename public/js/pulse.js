(function () {
  const app = document.getElementById("pulse-app");
  if (!app) return;

  const apiBase = app.dataset.apiBase;
  const copy = {
    empty: app.dataset.empty || "There is not enough data yet",
    loading: app.dataset.loading || "Loading...",
    score: app.dataset.score || "Score",
    noTrend: app.dataset.noTrend || "No meaningful trend exists for this query yet"
  };
  const allowedRanges = new Set(["today", "week", "month", "year", "all"]);
  let currentRange = getInitialRange();

  const endpoint = (path) => `${apiBase}${path}`;
  const tooltip = document.createElement("div");
  tooltip.className = "pulse-tooltip";
  tooltip.setAttribute("aria-hidden", "true");
  document.body.appendChild(tooltip);

  function getInitialRange() {
    const params = new URLSearchParams(window.location.search);
    const range = (params.get("range") || "today").toLowerCase();
    return allowedRanges.has(range) ? range : "today";
  }

  function pathWithRange(path) {
    const separator = path.includes("?") ? "&" : "?";
    return `${path}${separator}range=${encodeURIComponent(currentRange)}`;
  }

  async function fetchJson(path) {
    const response = await fetch(endpoint(pathWithRange(path)), { headers: { Accept: "application/json" } });
    if (!response.ok) throw new Error(`Pulse API failed: ${response.status}`);
    return response.json();
  }

  function syncRangeButtons() {
    app.querySelectorAll("[data-range]").forEach((button) => {
      const isActive = button.dataset.range === currentRange;
      button.classList.toggle("is-active", isActive);
      button.setAttribute("aria-pressed", isActive ? "true" : "false");
    });
  }

  function syncRangeUrl() {
    const url = new URL(window.location.href);
    if (currentRange === "today") {
      url.searchParams.delete("range");
    } else {
      url.searchParams.set("range", currentRange);
    }
    window.history.replaceState({}, "", url);
  }

  function setLoading() {
    ["top", "rising", "zero"].forEach((name) => {
      const target = app.querySelector(`[data-list="${name}"]`);
      if (!target) return;
      target.textContent = "";
      const loading = document.createElement("p");
      loading.className = "pulse-empty";
      loading.textContent = copy.loading;
      target.appendChild(loading);
    });
  }

  function scoreText(score, detail) {
    const text = `${copy.score} ${Math.max(0, Math.min(100, score || 0))}`;
    return detail ? `${text} - ${detail}` : text;
  }

  function positionTooltip(anchor, event) {
    const rect = anchor.getBoundingClientRect();
    const x = event && event.clientX ? event.clientX : rect.left + rect.width / 2;
    const y = event && event.clientY ? event.clientY : rect.top;
    tooltip.style.left = `${Math.max(12, Math.min(window.innerWidth - 12, x))}px`;
    tooltip.style.top = `${Math.max(12, y - 10)}px`;
  }

  function showTooltip(anchor, text, event) {
    tooltip.textContent = text;
    tooltip.classList.add("is-visible");
    tooltip.setAttribute("aria-hidden", "false");
    positionTooltip(anchor, event);
  }

  function hideTooltip() {
    tooltip.classList.remove("is-visible");
    tooltip.setAttribute("aria-hidden", "true");
  }

  function wireTooltip(anchor, text) {
    anchor.setAttribute("title", text);
    anchor.addEventListener("mouseenter", (event) => showTooltip(anchor, text, event));
    anchor.addEventListener("mousemove", (event) => positionTooltip(anchor, event));
    anchor.addEventListener("mouseleave", hideTooltip);
    anchor.addEventListener("focus", (event) => showTooltip(anchor, text, event));
    anchor.addEventListener("blur", hideTooltip);
  }

  function formatPointTime(timestamp) {
    if (!timestamp) return "";
    const date = new Date(timestamp);
    if (Number.isNaN(date.getTime())) return timestamp;
    return date.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
  }

  function setSummary(summary) {
    const data = summary && summary.data ? summary.data : {};
    const values = {
      activity: data.activityScore || 0,
      success: data.successScore || 0,
      opportunity: data.zeroResultOpportunityScore || 0,
      speed: data.speedScore || 0
    };

    Object.entries(values).forEach(([key, value]) => {
      const card = app.querySelector(`[data-summary-card="${key}"] strong`);
      if (card) card.textContent = String(value);
    });
  }

  function renderList(name, payload) {
    const target = app.querySelector(`[data-list="${name}"]`);
    if (!target) return;

    const items = payload && payload.data && Array.isArray(payload.data.items) ? payload.data.items : [];
    target.textContent = "";

    if (!items.length) {
      const empty = document.createElement("p");
      empty.className = "pulse-empty";
      empty.textContent = copy.empty;
      target.appendChild(empty);
      return;
    }

    items.forEach((item) => {
      const button = document.createElement("button");
      button.type = "button";
      button.className = "pulse-item";
      button.dataset.query = item.query || "";
      const itemScore = Math.max(0, Math.min(100, item.score || 0));
      const tooltipText = scoreText(itemScore, item.query || "");

      const query = document.createElement("span");
      query.className = "pulse-query";
      query.textContent = item.query || "";

      const bar = document.createElement("span");
      bar.className = "pulse-bar";
      bar.setAttribute("aria-label", scoreText(itemScore));
      const fill = document.createElement("span");
      fill.style.setProperty("--score-width", `${itemScore}%`);
      bar.appendChild(fill);

      button.appendChild(query);
      button.appendChild(bar);
      wireTooltip(button, tooltipText);
      button.addEventListener("click", () => loadTrend(item.query || ""));
      target.appendChild(button);
    });
  }

  function renderTrend(query, payload) {
    const target = app.querySelector("[data-trend]");
    if (!target) return;

    const points = payload && payload.data && Array.isArray(payload.data.points) ? payload.data.points : [];
    target.textContent = "";

    const enoughData = payload && payload.data && payload.data.enoughData;
    if (!query || !points.length || !enoughData) {
      const empty = document.createElement("p");
      empty.className = "pulse-empty";
      empty.textContent = copy.noTrend;
      target.appendChild(empty);
      return;
    }

    const title = document.createElement("p");
    title.className = "pulse-trend-title";
    title.textContent = query;
    target.appendChild(title);

    const width = 600;
    const height = 150;
    const step = points.length > 1 ? width / (points.length - 1) : width;
    const chartPoints = points.map((point, index) => {
      const score = Math.max(0, Math.min(100, point.score || 0));
      const x = index * step;
      const y = height - (score / 100) * (height - 18) - 9;
      return { point, score, x, y };
    });
    const polylinePoints = chartPoints.map(({ x, y }) => `${x.toFixed(1)},${y.toFixed(1)}`).join(" ");

    const svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    svg.setAttribute("viewBox", `0 0 ${width} ${height}`);
    svg.setAttribute("class", "pulse-sparkline");
    svg.setAttribute("role", "img");

    const line = document.createElementNS("http://www.w3.org/2000/svg", "polyline");
    line.setAttribute("points", polylinePoints);
    svg.appendChild(line);

    chartPoints.forEach(({ point, score, x, y }) => {
      const marker = document.createElementNS("http://www.w3.org/2000/svg", "circle");
      const tooltipText = scoreText(score, formatPointTime(point.timestamp));
      marker.setAttribute("class", "pulse-spark-point");
      marker.setAttribute("cx", x.toFixed(1));
      marker.setAttribute("cy", y.toFixed(1));
      marker.setAttribute("r", "5");
      marker.setAttribute("tabindex", "0");
      marker.setAttribute("aria-label", tooltipText);

      const nativeTitle = document.createElementNS("http://www.w3.org/2000/svg", "title");
      nativeTitle.textContent = tooltipText;
      marker.appendChild(nativeTitle);

      marker.addEventListener("mouseenter", (event) => showTooltip(marker, tooltipText, event));
      marker.addEventListener("mousemove", (event) => positionTooltip(marker, event));
      marker.addEventListener("mouseleave", hideTooltip);
      marker.addEventListener("focus", (event) => showTooltip(marker, tooltipText, event));
      marker.addEventListener("blur", hideTooltip);
      svg.appendChild(marker);
    });

    target.appendChild(svg);
  }

  async function loadTrend(query) {
    if (!query) return;
    try {
      const payload = await fetchJson(`/query?q=${encodeURIComponent(query)}`);
      renderTrend(query, payload);
    } catch (_) {
      renderTrend(query, null);
    }
  }

  async function boot() {
    syncRangeButtons();
    syncRangeUrl();
    setLoading();

    const [summary, top, rising, zero] = await Promise.allSettled([
      fetchJson("/summary"),
      fetchJson("/top-queries"),
      fetchJson("/rising"),
      fetchJson("/zero-results")
    ]);

    setSummary(summary.status === "fulfilled" ? summary.value : null);
    renderList("top", top.status === "fulfilled" ? top.value : null);
    renderList("rising", rising.status === "fulfilled" ? rising.value : null);
    renderList("zero", zero.status === "fulfilled" ? zero.value : null);

    const first = top.status === "fulfilled" && top.value.data && top.value.data.items
      ? top.value.data.items[0]
      : null;
    if (first && first.query) {
      loadTrend(first.query);
    }
  }

  boot().catch(() => {
    renderList("top", null);
    renderList("rising", null);
    renderList("zero", null);
  });

  app.querySelectorAll("[data-range]").forEach((button) => {
    button.addEventListener("click", () => {
      const range = button.dataset.range || "today";
      if (!allowedRanges.has(range) || range === currentRange) return;
      currentRange = range;
      boot();
    });
  });
})();
