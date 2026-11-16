// The typewriter machine: wordmark bar, paper on its platen, and (from T24) the key rows.
import { useEffect, useRef, useState } from "react";
import { bridge, type TextPayload } from "../bridge";
import { Paper } from "./Paper";

export function Machine() {
  const [text, setText] = useState("");
  const [payload, setPayload] = useState<TextPayload | null>(null);
  const latest = useRef("");

  useEffect(() => {
    bridge.getState().then((s) => {
      latest.current = s.text;
      setText(s.text);
      setPayload(s);
    });
  }, []);

  const onText = (next: string) => {
    latest.current = next;
    setText(next); // paint the character now; tokens follow from core
    bridge.setText(next).then((p) => {
      if (p.text === latest.current) setPayload(p); // ignore replies for text that has since changed
    });
  };

  const fresh = payload && payload.text === text ? payload : null;
  return (
    <main className="machine">
      <header className="top">
        <span className="wordmark">Verbatim</span>
      </header>
      <Paper
        text={text}
        tokens={fresh?.tokens ?? null}
        ignored={fresh?.ignoredLines ?? []}
        seedHash={text.trim() ? (fresh?.seedHash ?? payload?.seedHash ?? "") : ""}
        onText={onText}
      />
    </main>
  );
}
