// T26: Advanced mode. U7 (hold, edit, add, remove through the bridge), drawer, add menu limits, focus, Esc.
import { expect, test, type Page } from "@playwright/test";

type Call = { name: string; args: { lane: number; op: string; slot: number; module?: { type: number; p: number[]; mix: number } } };
const seed = (page: Page) => page.getByRole("textbox", { name: "Seed text" });
const voice = (page: Page, n: number) => page.getByRole("list", { name: `Voice ${n}` });
const overrides = (page: Page) =>
  page.evaluate(() => (window as unknown as { __mockBridge: { calls: Call[] } }).__mockBridge.calls.filter((c) => c.name === "setOverride").map((c) => c.args));
const advanced = (page: Page) => page.getByRole("button", { name: "Advanced" });

async function openAdvanced(page: Page, text = "hello") {
  await page.goto("/?mock");
  await seed(page).fill(text);
  await advanced(page).click();
  await expect(advanced(page)).toHaveAttribute("aria-pressed", "true");
}

test("Advanced toggles the + keys and saves the flag through setUi", async ({ page }) => {
  await page.goto("/?mock");
  await seed(page).fill("hello");
  await expect(page.getByRole("button", { name: "Add effect to voice 1" })).toHaveCount(0);
  await advanced(page).click();
  await expect(page.getByRole("button", { name: "Add effect to voice 1" })).toBeVisible();
  const ui = await page.evaluate(() => (window as unknown as { __mockBridge: { calls: { name: string; args: unknown }[] } }).__mockBridge.calls.filter((c) => c.name === "setUi").map((c) => c.args));
  expect(ui).toContainEqual({ advanced: true });
});

test("In Advanced a key click selects it and opens its drawer with labelled sliders and units", async ({ page }) => {
  await openAdvanced(page);
  await voice(page, 1).getByRole("button", { name: "Freeze" }).click();
  const drawer = page.getByRole("region", { name: "Freeze" });
  await expect(drawer).toBeVisible();
  await expect(drawer.getByRole("slider")).not.toHaveCount(0);
  await expect(drawer.getByRole("slider").first()).toHaveAttribute("aria-valuetext", /\S/);
  expect(await overrides(page)).toEqual([]); // selecting does not hold in Advanced
});

test("U7 edit: moving a drawer slider sends edit with the whole module and auto-holds", async ({ page }) => {
  await openAdvanced(page);
  await voice(page, 1).getByRole("button", { name: "Comb" }).click();
  const slider = page.getByRole("region", { name: "Comb" }).getByRole("slider").first();
  await slider.focus();
  await page.keyboard.press("ArrowRight");
  const ov = await overrides(page);
  expect(ov).toHaveLength(1);
  expect(ov[0]).toMatchObject({ lane: 0, op: "edit", slot: 3 });
  expect(ov[0].module!.type).toBe(10);
  expect(ov[0].module!.p).toHaveLength(6);
  await expect(page.getByRole("region", { name: "Comb" }).getByRole("button", { name: "Hold" })).toHaveAttribute("aria-pressed", "true");
});

test("U7 hold toggle in the drawer sends hold and release", async ({ page }) => {
  await openAdvanced(page);
  await voice(page, 1).getByRole("button", { name: "Saturation" }).click();
  const hold = page.getByRole("region", { name: "Saturation" }).getByRole("button", { name: "Hold" });
  await hold.click();
  await expect(hold).toHaveAttribute("aria-pressed", "true");
  await hold.click();
  await expect(hold).toHaveAttribute("aria-pressed", "false");
  expect((await overrides(page)).map((o) => o.op)).toEqual(["hold", "release"]);
});

test("U7 remove: the bin removes the module and focus moves to the row's + key", async ({ page }) => {
  await openAdvanced(page);
  await voice(page, 1).getByRole("button", { name: "Compressor" }).click();
  await page.getByRole("region", { name: "Compressor" }).getByRole("button", { name: "Remove" }).click();
  expect(await overrides(page)).toEqual([{ lane: 0, op: "remove", slot: 2 }]);
  await expect(voice(page, 1).getByRole("button", { name: "Compressor" })).toHaveCount(0);
  await expect(page.getByRole("button", { name: "Add effect to voice 1" })).toBeFocused();
});

test("U7 add: the + key opens a menu of 16 effects; picking one adds it; Esc closes and returns focus", async ({ page }) => {
  await openAdvanced(page);
  const plus = page.getByRole("button", { name: "Add effect to voice 1" });
  await plus.click();
  const menu = page.getByRole("menu", { name: "Add effect" });
  await expect(menu.getByRole("menuitem")).toHaveCount(16);
  await page.keyboard.press("Escape");
  await expect(menu).toHaveCount(0);
  await expect(plus).toBeFocused();
  await plus.click();
  await menu.getByRole("menuitem", { name: "Delay" }).click();
  const ov = await overrides(page);
  expect(ov[0]).toMatchObject({ lane: 0, op: "add", slot: -1, module: { type: 3 } });
  await expect(voice(page, 1).getByRole("button", { name: "Delay" })).toHaveAttribute("aria-pressed", "true");
});

test("Add menu disables SAT at 2 per lane; the + key disables at 8 modules", async ({ page }) => {
  await openAdvanced(page); // hello: one SAT
  const plus = page.getByRole("button", { name: "Add effect to voice 1" });
  await plus.click();
  await page.getByRole("menu", { name: "Add effect" }).getByRole("menuitem", { name: "Saturation" }).click();
  await plus.click();
  await expect(page.getByRole("menu", { name: "Add effect" }).getByRole("menuitem", { name: "Saturation" })).toBeDisabled();
  await page.keyboard.press("Escape");

  await seed(page).fill("wait.. what..."); // 6 generated modules
  for (let i = 0; i < 2; ++i) {
    await plus.click();
    await page.getByRole("menu", { name: "Add effect" }).getByRole("menuitem", { name: "Ring mod" }).click();
  }
  await expect(plus).toBeDisabled();
});

test("Esc closes the drawer", async ({ page }) => {
  await openAdvanced(page);
  await voice(page, 1).getByRole("button", { name: "Freeze" }).click();
  await expect(page.getByRole("region", { name: "Freeze" })).toBeVisible();
  await page.keyboard.press("Escape");
  await expect(page.getByRole("region", { name: "Freeze" })).toHaveCount(0);
});
