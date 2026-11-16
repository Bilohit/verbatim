// T25: carriage scale (Mix, Macro, Output) bound to host params, output meter with clip tick.
import { expect, test, type Page } from "@playwright/test";

type Mock = {
  calls: { name: string; args: unknown }[];
  hostSet: (name: string, normalised: number) => void;
  emitLevels: (l: { lanes: number[]; out: [number, number]; clip: boolean }) => void;
};
const sliderCalls = (page: Page, name: string) =>
  page.evaluate((n) => (window as unknown as { __mockBridge: Mock }).__mockBridge.calls.filter((c) => c.name === `param:${n}`).map((c) => c.args), name);

test("Three sliders named Mix, Macro, Output show host values with units", async ({ page }) => {
  await page.goto("/?mock");
  await expect(page.getByRole("slider", { name: "Mix" })).toHaveAttribute("aria-valuetext", "100 %");
  await expect(page.getByRole("slider", { name: "Macro" })).toHaveAttribute("aria-valuetext", "0.50");
  await expect(page.getByRole("slider", { name: "Output" })).toHaveAttribute("aria-valuetext", "0.0 dB");
});

test("Arrow keys move a slider with gesture begin, value, end", async ({ page }) => {
  await page.goto("/?mock");
  const mix = page.getByRole("slider", { name: "Mix" });
  await mix.focus();
  await page.keyboard.press("ArrowLeft");
  await expect(mix).toHaveAttribute("aria-valuetext", "99 %");
  const c = (await sliderCalls(page, "mix")) as { op: string; value?: number }[];
  expect(c.map((x) => x.op)).toEqual(["begin", "set", "end"]);
  expect(c[1].value).toBeCloseTo(0.99, 3);
});

test("Dragging shows the value above the marker; double-click resets to default", async ({ page }) => {
  await page.goto("/?mock");
  const out = page.getByRole("slider", { name: "Output" });
  const box = (await out.boundingBox())!;
  await page.mouse.move(box.x + box.width * (24 / 36), box.y + box.height / 2);
  await page.mouse.down();
  await page.mouse.move(box.x + box.width * 0.3, box.y + box.height / 2, { steps: 5 });
  await expect(page.locator(".slider-value", { hasText: "dB" })).toBeVisible();
  await page.mouse.up();
  await expect(out).not.toHaveAttribute("aria-valuetext", "0.0 dB");
  const ops = ((await sliderCalls(page, "output")) as { op: string }[]).map((x) => x.op);
  expect(ops[0]).toBe("begin");
  expect(ops[ops.length - 1]).toBe("end");
  await out.dblclick();
  await expect(out).toHaveAttribute("aria-valuetext", "0.0 dB");
});

test("Host automation moves the slider", async ({ page }) => {
  await page.goto("/?mock");
  await page.evaluate(() => (window as unknown as { __mockBridge: Mock }).__mockBridge.hostSet("macro", 0.25));
  await expect(page.getByRole("slider", { name: "Macro" })).toHaveAttribute("aria-valuetext", "0.25");
});

test("Output meter follows levels; the clip tick holds for 1 s", async ({ page }) => {
  await page.goto("/?mock");
  const meter = page.getByRole("meter", { name: "Output level" });
  await expect(meter).toBeVisible();
  await page.evaluate(() => (window as unknown as { __mockBridge: Mock }).__mockBridge.emitLevels({ lanes: [0, 0, 0, 0], out: [1, 0.1], clip: true }));
  await expect(page.locator(".clip-tick")).toBeVisible();
  await expect(meter).toHaveAttribute("aria-valuenow", "0");
  await page.waitForTimeout(1300);
  await expect(page.locator(".clip-tick")).toBeHidden();
});
