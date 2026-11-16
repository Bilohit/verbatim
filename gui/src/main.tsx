import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import "@fontsource/courier-prime/400.css";
import "@fontsource/courier-prime/700.css";
import "@fontsource-variable/archivo";
import "@fontsource/ibm-plex-mono/400.css";
import "./styles/tokens.css";
import "./styles/app.css";
import { Machine } from "./components/Machine";

// Load the faces now, not on the first typed character (a lazy font load stalled typing by ~120 ms, U1).
for (const face of ['400 1em "Courier Prime"', '700 1em "Courier Prime"', '1em "Archivo Variable"', '1em "IBM Plex Mono"'])
  document.fonts.load(face);

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <Machine />
  </StrictMode>,
);
