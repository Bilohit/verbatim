import { defineConfig } from "@playwright/test";

// Browser tests run the built UI against the mock bridge (/?mock) in the installed Google Chrome,
// the same Chromium engine as the plugin's WebView2.
export default defineConfig({
  testDir: "tests",
  timeout: 60_000,
  expect: { timeout: 3_000 },
  use: { baseURL: "http://127.0.0.1:4173", channel: "chrome", viewport: { width: 900, height: 620 } },
  webServer: { command: "npm run build && npx vite preview --host 127.0.0.1 --port 4173 --strictPort", url: "http://127.0.0.1:4173", reuseExistingServer: false, timeout: 120_000 },
});
