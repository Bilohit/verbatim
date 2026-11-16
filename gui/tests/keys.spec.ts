// T24: key rows. U7 (hold through the bridge), U2 (chain feedback latency), names, stagger, live region.
import { expect, test, type Page } from "@playwright/test";

const seed = (page: Page) => page.getByRole("textbox", { name: "Seed text" });
const voice = (page: Page, n: number) => page.getByRole("list", { name: `Voice ${n}` });
type Calls = { name: string; args: { lane: number; op: string; slot: number } }[];
const calls = (page: Page) => page.evaluate(() => (window as unknown as { __mockBridge: { calls: Calls } }).__mockBridge.calls);

test("One row of icon keys per voice, named for screen readers", async ({ page }) => {
  await page.goto("/?mock");
  await seed(page).fill("hello"); // core: SAT, FREEZE, COMP, COMB
  const keys = voice(page, 1).getByRole("button");
  await expect(keys).toHaveCount(4);
  await expect(keys.nth(0)).toHaveAccessibleName("Saturation");
  await expect(keys.nth(1)).toHaveAccessibleName("Freeze");
  await expect(keys.nth(2)).toHaveAccessibleName("Compressor");
  await expect(keys.nth(3)).toHaveAccessibleName("Comb");
  await expect(keys.nth(0).locator("svg")).toHaveCount(1);
});

test("U7 hold: clicking a key latches it and sends setOverride hold, clicking again releases", async ({ page }) => {
  await page.goto("/?mock");
  await seed(page).fill("hello");
  const key = voice(page, 1).getByRole("button", { name: "Freeze" });
  await expect(key).toHaveAttribute("aria-pressed", "false");
  await key.click();
  await expect(key).toHaveAttribute("aria-pressed", "true");
  await key.click();
  await expect(key).toHaveAttribute("aria-pressed", "false");
  const ov = (await calls(page)).filter((c) => c.name === "setOverride").map((c) => c.args);
  expect(ov).toEqual([
    { lane: 0, op: "hold", slot: 1 },
    { lane: 0, op: "release", slot: 1 },
  ]);
});

test("Space and Enter activate a focused key", async ({ page }) => {
  await page.goto("/?mock");
  await seed(page).fill("hello");
  const key = voice(page, 1).getByRole("button", { name: "Comb" });
  await key.focus();
  await page.keyboard.press("Space");
  await expect(key).toHaveAttribute("aria-pressed", "true");
  await page.keyboard.press("Enter");
  await expect(key).toHaveAttribute("aria-pressed", "false");
});

test("The name shows at once on keyboard focus", async ({ page }) => {
  await page.goto("/?mock");
  await seed(page).fill("hello");
  await seed(page).press("Tab");
  const tips = page.locator(".key-tip");
  // Tab from the text reaches the first key (Roll / Advanced / sliders arrive in later tasks).
  await expect(page.locator(".key:focus-visible")).toHaveCount(1);
  await expect(tips.filter({ hasText: "Saturation" })).toBeVisible();
});

test("Rows are staggered 26 px per lane like a keyboard", async ({ page }) => {
  await page.goto("/?mock");
  await seed(page).fill("a\nb\nc\nd\ne");
  const x = async (n: number) => (await voice(page, n).getByRole("button").first().boundingBox())!.x;
  expect((await x(2)) - (await x(1))).toBeCloseTo(26, 0);
  expect((await x(3)) - (await x(2))).toBeCloseTo(26, 0);
});

test("Live region announces voices, effects, holds, and the four-line limit", async ({ page }) => {
  await page.goto("/?mock");
  const status = page.getByRole("status");
  await seed(page).fill("hello");
  await expect(status).toHaveText("1 voice, 4 effects, 0 held.");
  await voice(page, 1).getByRole("button", { name: "Comb" }).click();
  await expect(status).toHaveText("1 voice, 4 effects, 1 held.");
  await seed(page).fill("a\nb\nc\nd\ne");
  await expect(status).toHaveText("Only the first four lines make sound.");
});

test("U2 chain feedback: keydown to updated key row p95 <= 100 ms (mock bridge, 5 ms delay)", async ({ page }) => {
  await page.goto("/?mock&delay=5");
  await seed(page).click();
  await page.evaluate(() => {
    const w = window as unknown as { __u2: number[] };
    w.__u2 = [];
    const rows = document.querySelector("[data-rows]")!;
    let t0 = 0;
    document.addEventListener("keydown", () => (t0 = performance.now()), true);
    new MutationObserver(() => {
      if (t0) {
        w.__u2.push(performance.now() - t0);
        t0 = 0;
      }
    }).observe(rows, { childList: true, subtree: true, attributes: true });
  });
  for (const c of "tape echo radio glass river static noise dust neon velvet") {
    await page.keyboard.type(c);
    await page.waitForTimeout(30);
  }
  const lat = (await page.evaluate(() => (window as unknown as { __u2: number[] }).__u2)).sort((a, b) => a - b);
  expect(lat.length).toBeGreaterThan(30);
  const p95 = lat[Math.floor(lat.length * 0.95)];
  console.log(`U2 p95 ${p95.toFixed(1)} ms over ${lat.length}`);
  expect(p95).toBeLessThanOrEqual(100);
});
