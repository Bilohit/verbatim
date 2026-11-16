// T23: U1 (input latency), U4 (token colouring from core), U8 (long text). docs/06-test-plan.md.
import { expect, test, type Page } from "@playwright/test";
import fixture from "./fixtures/core.json" with { type: "json" };

const seed = (page: Page) => page.getByRole("textbox", { name: "Seed text" });

// Byte offsets from core -> UTF-16 offsets in the line, as the page must do.
function utf16Slice(line: string, start: number, length: number) {
  const bytes = new TextEncoder().encode(line);
  return new TextDecoder().decode(bytes.slice(start, start + length));
}

// Measures keydown -> next animation frame with the typed character present in the ink layer.
async function typeAndMeasure(page: Page, chars: string) {
  await page.evaluate(() => {
    const w = window as unknown as { __lat: number[] };
    w.__lat = [];
    document.addEventListener("keydown", () => {
      const t0 = performance.now();
      const before = document.querySelector("[data-ink='cold']")?.textContent?.length ?? 0;
      const check = () => {
        const now = document.querySelector("[data-ink='cold']")?.textContent?.length ?? 0;
        if (now !== before) w.__lat.push(performance.now() - t0);
        else requestAnimationFrame(check);
      };
      requestAnimationFrame(check);
    }, true);
  });
  // Fast human pace (~33 keys/s). Typing 1 ms apart queues ~20 keys behind the one-time cold-start frame of a
  // fresh browser and measures the queue, not the input path.
  for (const c of chars) await page.keyboard.type(c, { delay: 0 }).then(() => page.waitForTimeout(30));
  await page.waitForTimeout(100);
  const lat = await page.evaluate(() => (window as unknown as { __lat: number[] }).__lat);
  lat.sort((a, b) => a - b);
  return lat;
}

test("U1 input latency: keydown to painted character p95 <= 16 ms over 200 key presses", async ({ page }) => {
  await page.goto("/?mock");
  await seed(page).click();
  const lat = await typeAndMeasure(page, "tape echo dark ".repeat(14).slice(0, 200));
  expect(lat.length).toBeGreaterThanOrEqual(190);
  const p95 = lat[Math.floor(lat.length * 0.95)];
  console.log(`U1 p95 ${p95.toFixed(1)} ms over ${lat.length}; worst ${lat.slice(-12).map((x) => x.toFixed(0)).join(",")}; median ${lat[lat.length >> 1].toFixed(1)}`);
  expect(p95).toBeLessThanOrEqual(16);
});

test("U4 token colouring: DOM classes match core's token list for the 14 worked examples", async ({ page }) => {
  await page.goto("/?mock");
  for (const ex of fixture) {
    await seed(page).fill(ex.text);
    const lines = ex.text.replace(/\r\n?/g, "\n").split("\n");
    await expect(page.locator("[data-ink='cold'] [data-line]")).toHaveCount(lines.length);
    for (let l = 0; l < lines.length; ++l) {
      const spans = page.locator(`[data-ink='cold'] [data-line='${l}'] [data-token]`);
      const tokens = ex.tokens[l] ?? [];
      await expect(spans, `${ex.text} line ${l}`).toHaveCount(tokens.length);
      for (let k = 0; k < tokens.length; ++k) {
        const t = tokens[k];
        await expect(spans.nth(k)).toHaveAttribute("data-token", t.class);
        await expect(spans.nth(k)).toHaveText(utf16Slice(lines[l], t.start, t.length));
      }
    }
  }
});

test("U8 long text: 4096 bytes and 200 lines, no horizontal overflow, typing stays within a frame", async ({ page }) => {
  await page.goto("/?mock");
  const long = Array.from({ length: 200 }, (_, i) => `line ${i} tape echo`).join("\n").slice(0, 4096);
  await seed(page).fill(long);
  const overflow = await page.evaluate(() => document.documentElement.scrollWidth - document.documentElement.clientWidth);
  expect(overflow).toBeLessThanOrEqual(0);
  // The paper scrolls after 8 visible lines instead of growing the machine.
  const paperBox = await page.locator("[data-paper]").boundingBox();
  expect(paperBox!.height).toBeLessThan(620);
  await seed(page).press("End");
  const lat = await typeAndMeasure(page, "abcdefghijklmnopqrst");
  const p95 = lat[Math.floor(lat.length * 0.95)];
  console.log(`U8 p95 ${p95.toFixed(1)} ms`);
  expect(p95).toBeLessThanOrEqual(16);
});

test("Ignored lines are struck through and the postmark shows core's seed hash", async ({ page }) => {
  await page.goto("/?mock");
  const ex = fixture.find((f) => f.text === "a\nb\nc\nd\ne")!;
  await seed(page).fill(ex.text);
  await expect(page.locator("[data-ink='cold'] [data-line='4']")).toHaveAttribute("data-ignored", "true");
  await expect(page.getByRole("img", { name: /seed/i })).toHaveText(ex.seedHash);
  await seed(page).fill("");
  await expect(page.getByRole("img", { name: /seed/i })).toHaveCount(0);
});
