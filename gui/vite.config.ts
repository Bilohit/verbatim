import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

// Built files are zipped into the plugin and served by a JUCE resource provider (no network).
export default defineConfig({
  base: "./",
  plugins: [react()],
  build: { outDir: "dist", emptyOutDir: true, assetsInlineLimit: 0 },
});
