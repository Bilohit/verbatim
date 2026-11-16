// T27: motion table M1-M8 (docs/05-ui-design.md section 5) and reduced motion.
import { expect, test, type Page } from "@playwright/test";

type Mock = { emitLevels: (l: { lanes: number[]; out: [number, number]; clip: boolean }) => void };
const seed = (page: Page) => page.getByRole("textbox", { name: "Seed text" });
const levels = (page: Page, lane0: number) =>
  page.evaluate((v) => (window as unknown as { __mockBridge: Mock }).__mockBridge.emitLevels({ lanes: [v, 0, 0, 0], out: [v, v], clip: false }), lane0);
const hotOpacity = (page: Page, line: number) =>
  page.locator(`[data-ink='hot'] [data-line='${line}']`).evaluate((el) => Number(getComputedStyle(el).opacity));

test("M3 living ink: a line's full-ink copy follows its lane level", async ({ page }) => {
  await page.goto("/?mock");
  await seed(page).fill("hello");
  for (let i = 0; i < 30; ++i) await levels(page, 0.25); // -12 dBFS RMS = full level
  await page.waitForTimeout(600);
  expect(await hotOpacity(page, 0)).toBeGreaterThan(0.7);
  for (let i = 0; i < 30; ++i) await levels(page, 0);
  await page.waitForTimeout(800);
  expect(await hotOpacity(page, 0)).toBeLessThan(0.15);
});

test("M3 lines up to 200 characters carry a per-character phase; longer lines animate per line", async ({ page }) => {
  await page.goto("/?mock");
  await seed(page).fill("short line\n" + "x".repeat(210));
  await expect(page.locator("[data-ink='hot'] [data-line='0'] [data-ch]")).toHaveCount("short line".length);
  await expect(page.locator("[data-ink='hot'] [data-line='1'] [data-ch]")).toHaveCount(0);
});

test("M1 keys drop in and settle fully visible; M6 the postmark re-stamps when the seed changes", async ({ page }) => {
  await page.goto("/?mock");
  await seed(page).fill("hello");
  const key = page.getByRole("list", { name: "Voice 1" }).getByRole("button").first();
  await expect(key).toBeVisible();
  await page.waitForTimeout(500);
  expect(await key.evaluate((el) => Number(getComputedStyle(el.closest("li")!).opacity))).toBe(1);
  const stamp = page.getByRole("img", { name: /seed/i });
  const first = await stamp.getAttribute("data-stamp");
  await seed(page).fill("tape echo");
  await expect(stamp).not.toHaveAttribute("data-stamp", first!);
});

test("M7 the drawer enters and stays fully visible", async ({ page }) => {
  await page.goto("/?mock");
  await seed(page).fill("hello");
  await page.getByRole("button", { name: "Advanced" }).click();
  await page.getByRole("list", { name: "Voice 1" }).getByRole("button", { name: "Comb" }).click();
  const drawer = page.getByRole("region", { name: "Comb" });
  await page.waitForTimeout(400);
  expect(await drawer.evaluate((el) => Number(getComputedStyle(el).opacity))).toBe(1);
});

test.describe("reduced motion", () => {
  test.use({ contextOptions: { reducedMotion: "reduce" } });

  test("No per-character phase, no typebar scale, everything ends visible", async ({ page }) => {
    await page.goto("/?mock");
    await seed(page).fill("hello");
    await seed(page).press("End");
    await page.keyboard.type("!");
    await expect(page.locator("[data-ink='hot'] [data-ch]")).toHaveCount(0);
    await expect(page.locator("[data-strike]")).toHaveCount(0);
    await page.waitForTimeout(200);
    const key = page.getByRole("list", { name: "Voice 1" }).getByRole("button").first();
    expect(await key.evaluate((el) => Number(getComputedStyle(el.closest("li")!).opacity))).toBe(1);
  });
});
