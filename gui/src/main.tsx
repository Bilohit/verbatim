import { StrictMode, useEffect, useState } from "react";
import { createRoot } from "react-dom/client";
import { getState, setText } from "./bridge";

// T07 shell: one plain textarea. The typewriter UI replaces it from T23.
function App() {
  const [text, setLocal] = useState("");
  useEffect(() => {
    getState().then((s) => setLocal(s.text));
  }, []);
  return (
    <textarea
      aria-label="Seed text"
      autoFocus
      spellCheck={false}
      value={text}
      onChange={(e) => {
        setLocal(e.target.value);
        setText(e.target.value);
      }}
      style={{ width: "100vw", height: "100vh", boxSizing: "border-box", border: 0, padding: 16, resize: "none", font: "18px monospace" }}
    />
  );
}

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
