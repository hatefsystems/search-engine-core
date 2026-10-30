// Static content/layout checks. Set LEGAL_BASE_URL to check the actual backend instead.
const { chromium } = require("playwright");
const assert = require("node:assert/strict");
const fs = require("node:fs");
const path = require("node:path");
const http = require("node:http");
const root = path.resolve(__dirname, "../..");
const shots = path.join(root, "build/legal/screenshots");
const paths = {
  privacy: { fa: "/حریم-خصوصی", en: "/privacy" },
  terms: { fa: "/شرایط-استفاده", en: "/terms" },
};
fs.mkdirSync(shots, { recursive: true });
const fixture = http.createServer((req, res) => {
  const url = decodeURIComponent(new URL(req.url, "http://localhost").pathname);
  let file;
  for (const [doc, langs] of Object.entries(paths)) {
    for (const [lang, route] of Object.entries(langs)) {
      if (url === route)
        file = path.join(root, `public/legal/${doc}-${lang}.html`);
    }
  }
  if (url.startsWith("/assets/")) {
    file = path.join(root, "public", url.slice(8));
    if (!fs.existsSync(file)) file = path.join(root, "public", url.slice(1));
  }
  if (!file || !fs.existsSync(file)) return res.writeHead(404).end();
  res.setHeader(
    "Content-Type",
    {
      ".html": "text/html; charset=utf-8",
      ".css": "text/css",
      ".woff2": "font/woff2",
    }[path.extname(file)] || "application/octet-stream"
  );
  res.end(fs.readFileSync(file));
});
(async () => {
  let browser;
  try {
    await new Promise((resolve) => fixture.listen(0, "127.0.0.1", resolve));
    const base =
      process.env.LEGAL_BASE_URL ||
      `http://127.0.0.1:${fixture.address().port}`;
    browser = await chromium.launch({
      headless: true,
      ...(process.env.LEGAL_CHROMIUM_EXECUTABLE
        ? { executablePath: process.env.LEGAL_CHROMIUM_EXECUTABLE }
        : {}),
      args: ["--no-sandbox"],
    });
    const page = await browser.newPage({
      viewport: { width: 1440, height: 1120 },
    });
    const errors = [];
    page.on("pageerror", (error) => errors.push(error.message));
    page.on("response", (response) => {
      if (response.status() >= 400)
        errors.push(`${response.status()} ${response.url()}`);
    });
    const headingIds = {};
    for (const [doc, languages] of Object.entries(paths)) {
      for (const [lang, route] of Object.entries(languages)) {
        await page.setViewportSize({ width: 1440, height: 1120 });
        assert.equal((await page.goto(base + route)).status(), 200);
        await page.evaluate(() => document.fonts.ready);
        assert.equal(await page.locator("html").getAttribute("lang"), lang);
        assert.equal(
          await page.locator("html").getAttribute("dir"),
          lang === "fa" ? "rtl" : "ltr"
        );
        assert.equal(
          await page.locator("link[rel=canonical]").getAttribute("href"),
          "https://hatef.ir" + route
        );
        assert.equal(await page.locator("h1").count(), 1);
        assert.equal(await page.locator("article section").count(), 10);
        const ids = await page
          .locator("article section")
          .evaluateAll((nodes) => nodes.map((node) => node.id));
        headingIds[doc] ||= ids;
        assert.deepEqual(
          ids,
          headingIds[doc],
          "Translations must cover the same sections"
        );
        assert.equal(
          await page.locator("link[hreflang=fa]").getAttribute("href"),
          "https://hatef.ir" + languages.fa
        );
        assert.equal(
          await page.locator("link[hreflang=en]").getAttribute("href"),
          "https://hatef.ir" + languages.en
        );
        assert.equal(
          await page.locator("link[hreflang=x-default]").getAttribute("href"),
          "https://hatef.ir" + languages.fa
        );
        assert.equal(
          await page.locator("script").count(),
          0,
          "Legal text works without JavaScript"
        );
        assert.equal(await page.locator('a[href*="?lang="]').count(), 0);
        if (lang === "fa")
          assert(
            await page.evaluate(() => document.fonts.check("15px Vazirmatn"))
          );
        for (const width of [1440, 1024, 768, 390, 320]) {
          await page.setViewportSize({ width, height: 1120 });
          assert(
            await page.evaluate(
              () => document.documentElement.scrollWidth <= innerWidth
            ),
            `${doc}-${lang}: horizontal overflow at ${width}`
          );
          assert(await page.locator("h1").isVisible());
          assert(
            await page
              .locator("article section p")
              .first()
              .evaluate(
                (node) => parseFloat(getComputedStyle(node).fontSize) >= 16
              ),
            "Long-form legal copy stays at least 16px on mobile and desktop"
          );
          assert(await page.locator("a.language").isVisible());
          if (width === 1440 || width === 390) {
            await page.screenshot({
              path: path.join(shots, `${doc}-${lang}-${width}.png`),
            });
          }
        }
        for (const width of [1440, 390, 320]) {
          await page.setViewportSize({ width, height: 1120 });
          await page.evaluate(
            () => (document.documentElement.style.fontSize = "200%")
          );
          assert(
            await page
              .locator("article section p")
              .first()
              .evaluate(
                (node) => parseFloat(getComputedStyle(node).fontSize) >= 32
              )
          );
          assert(
            await page.evaluate(
              () => document.documentElement.scrollWidth <= innerWidth
            ),
            `${doc}-${lang}: overflow with 200% text at ${width}: ` +
              JSON.stringify(
                await page.locator("body *").evaluateAll((nodes) =>
                  nodes
                    .filter((n) => n.getClientRects().length)
                    .filter(
                      (n) =>
                        n.getBoundingClientRect().right > innerWidth + 1 ||
                        n.getBoundingClientRect().left < -1
                    )
                    .map((n) => [
                      n.tagName,
                      n.className,
                      n.getBoundingClientRect().width,
                    ])
                )
              )
          );
          await page.evaluate(() =>
            document.documentElement.style.removeProperty("font-size")
          );
        }
        await page.setViewportSize({ width: 1440, height: 1120 });
        await page.screenshot({
          path: path.join(shots, `${doc}-${lang}-full.png`),
          fullPage: true,
        });
        await page.locator(".toc a").last().click();
        assert.equal(new URL(page.url()).hash, "#updates");
        const other = lang === "fa" ? "en" : "fa";
        await page.locator("a.language").click();
        assert.equal(
          decodeURIComponent(new URL(page.url()).pathname),
          languages[other]
        );
        assert.equal(new URL(page.url()).search, "");
        assert.equal(await page.locator("html").getAttribute("lang"), other);
        await page.locator(".document-tabs a:not([aria-current])").click();
        const otherDoc = doc === "privacy" ? "terms" : "privacy";
        assert.equal(
          decodeURIComponent(new URL(page.url()).pathname),
          paths[otherDoc][other]
        );
        console.log(
          `PASS ${doc}-${lang}: desktop/mobile, anchors, language/document navigation, metadata, fonts`
        );
      }
    }
    assert.deepEqual(errors, []);
    console.log(`PASS all 4 legal documents; screenshots: ${shots}`);
  } finally {
    await browser?.close();
    await new Promise((resolve) => fixture.close(resolve));
  }
})().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
