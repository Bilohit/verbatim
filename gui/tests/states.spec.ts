// T28: Roll (M8), states S0-S3 and E1-E3, tab order, U5 screenshot baselines, U6 accessibility.
import AxeBuilder from "@axe-core/playwright";
import { expect, test, type Page } from "@playwright/test";
import { WORDS } from "../src/words";

const seed = (page: Page) => page.getByRole("textbox", { name: "Seed text" });
const voice = (page: Page, n: number) => page.getByRole("list", { name: `Voice ${n}` });

test("Roll types 2 to 5 words from the bundled list at 25 ms per character; undo restores the text", async ({ page }) => {
  await page.goto("/?mock");
  await seed(page).fill("tape echo");
  await page.getByRole("button", { name: "Roll" }).click();
  await page.waitForTimeout(60);
  const partial = await seed(page).inputValue();
  await expect.poll(async () => (await seed(page).inputValue()).length, { timeout: 5000 }).toBeGreaterThan(partial.length);
  await page.waitForTimeout(1500);
  const rolled = await seed(page).inputValue();
  const words = rolled.trim().split(/\s+/);
  expect(words.length).toBeGreaterThanOrEqual(2);
  expect(words.length).toBeLessThanOrEqual(5);
  for (const w of words) expect(WORDS).toContain(w);
  await seed(page).focus();
  for (let i = 0; i < 60 && (await seed(page).inputValue()) !== "tape echo"; ++i) await page.keyboard.press("Control+z");
  await expect(seed(page)).toHaveValue("tape echo");
});

test("WORDS holds 200 distinct lowercase words", async () => {
  expect(WORDS).toHaveLength(200);
  expect(new Set(WORDS).size).toBe(200);
  for (const w of WORDS) expect(w).toMatch(/^[a-z]+$/);
});

test("S0 empty: blank paper, focused caret, no keys, no postmark", async ({ page }) => {
  await page.goto("/?mock");
  await expect(seed(page)).toBeFocused();
  await expect(page.getByRole("list", { name: /Voice/ })).toHaveCount(0);
  await expect(page.getByRole("img", { name: /seed/i })).toHaveCount(0);
});

test("E2 text full: input past 4096 bytes is ignored, announced, and the postmark shakes", async ({ page }) => {
  await page.goto("/?mock");
  await seed(page).fill("a".repeat(4096));
  await seed(page).press("End");
  await page.keyboard.type("b");
  await expect(seed(page)).toHaveValue("a".repeat(4096));
  await expect(page.getByRole("status")).toHaveText("Text is full.");
  await expect(page.locator("[data-shake]")).toHaveCount(1);
});

test("E3 state not loadable: read-only paper and one line of explanation", async ({ page }) => {
  await page.goto("/?mock&state=error");
  await expect(seed(page)).toHaveAttribute("readonly", "");
  await expect(page.getByText("Saved with a newer Verbatim. Update to edit.")).toBeVisible();
});

test("Tab order: text, Roll, Advanced, Mix, Macro, Output, keys", async ({ page }) => {
  await page.goto("/?mock");
  await seed(page).fill("hello");
  await seed(page).focus();
  const order: string[] = [];
  for (let i = 0; i < 6; ++i) {
    await page.keyboard.press("Tab");
    order.push(await page.evaluate(() => document.activeElement?.getAttribute("aria-label") ?? ""));
  }
  expect(order).toEqual(["Roll", "Advanced", "Mix", "Macro", "Output", "Saturation"]);
});

test("U6 producer path by keyboard only: type, roll, hold, advanced, edit, remove, add", async ({ page }) => {
  await page.goto("/?mock");
  await page.keyboard.type("hello");
  await expect(voice(page, 1).getByRole("button")).toHaveCount(4);
  // Hold Comb with Space.
  await voice(page, 1).getByRole("button", { name: "Comb" }).focus();
  await page.keyboard.press("Space");
  await expect(voice(page, 1).getByRole("button", { name: "Comb" })).toHaveAttribute("aria-pressed", "true");
  // Advanced, select Freeze, nudge a param, remove it.
  await page.getByRole("button", { name: "Advanced" }).focus();
  await page.keyboard.press("Enter");
  await voice(page, 1).getByRole("button", { name: "Freeze" }).focus();
  await page.keyboard.press("Enter");
  await page.getByRole("region", { name: "Freeze" }).getByRole("slider").first().focus();
  await page.keyboard.press("ArrowRight");
  await page.getByRole("region", { name: "Freeze" }).getByRole("button", { name: "Remove" }).focus();
  await page.keyboard.press("Enter");
  await expect(page.getByRole("button", { name: "Add effect to voice 1" })).toBeFocused();
  await expect(page.getByRole("region", { name: "Freeze" })).toHaveCount(0);
  // Add Delay from the menu by keyboard.
  await page.keyboard.press("Enter");
  await page.getByRole("menuitem", { name: "Delay" }).focus();
  await page.keyboard.press("Enter");
  await expect(voice(page, 1).getByRole("button", { name: "Delay" })).toBeVisible();
  // Mix by arrows.
  await page.getByRole("slider", { name: "Mix" }).focus();
  await page.keyboard.press("ArrowLeft");
  await expect(page.getByRole("slider", { name: "Mix" })).toHaveAttribute("aria-valuetext", "99 %");
});

const AXE_STATES: [string, (page: Page) => Promise<void>][] = [
  ["S0", async () => {}],
  ["S1", async (page) => seed(page).fill("dark tape, slow echo!\nwarm tape")],
  ["S2", async (page) => {
    await seed(page).fill("hello");
    await voice(page, 1).getByRole("button", { name: "Comb" }).click();
  }],
  ["S3", async (page) => {
    await seed(page).fill("hello");
    await page.getByRole("button", { name: "Advanced" }).click();
    await voice(page, 1).getByRole("button", { name: "Comb" }).click();
  }],
  ["E1", async (page) => seed(page).fill("a\nb\nc\nd\ne")],
];

for (const [name, setUp] of AXE_STATES) {
  test(`U6 axe: no serious or critical violations in ${name}`, async ({ page }) => {
    await page.goto("/?mock");
    await setUp(page);
    await page.waitForTimeout(400);
    // Scan the settled state: a mid-fade element (the hover tip after a click) blends into its ground.
    await page.waitForFunction(() =>
      document.getAnimations().every((a) => a.playState !== "running" || a.effect?.getComputedTiming().endTime === Infinity));
    const result = await new AxeBuilder({ page }).analyze();
    const bad = result.violations.filter((v) => v.impact === "serious" || v.impact === "critical");
    expect(bad.map((v) => `${v.id}: ${v.nodes.map((n) => `${n.target} ${n.failureSummary}`).join(" | ")}`)).toEqual([]);
  });
}

// U5: reviewed screenshot baselines per state and width (gui/tests/states.spec.ts-snapshots).
for (const width of [720, 900, 1600]) {
  for (const [name, setUp] of [...AXE_STATES, ["E3", async () => {}] as [string, (page: Page) => Promise<void>]]) {
    test(`U5 ${name} at ${width} px`, async ({ page }) => {
      await page.setViewportSize({ width, height: width === 1600 ? 1000 : width === 720 ? 520 : 620 });
      await page.emulateMedia({ reducedMotion: "reduce" });
      await page.goto(name === "E3" ? "/?mock&state=error" : "/?mock");
      await setUp(page);
      await page.waitForTimeout(500);
      const overflow = await page.evaluate(() => document.documentElement.scrollWidth - document.documentElement.clientWidth);
      expect(overflow).toBeLessThanOrEqual(0);
      await expect(page).toHaveScreenshot(`${name}-${width}.png`, { animations: "disabled", caret: "hide", maxDiffPixelRatio: 0.01 });
    });
  }
}
