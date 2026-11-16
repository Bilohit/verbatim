// The typewriter machine: wordmark bar, paper on its platen, and one row of keys per voice.
import { startTransition, useEffect, useRef, useState } from "react";
import { bridge, type Lane, type TextPayload } from "../bridge";
import { KeyRows } from "./KeyRow";
import { Meter } from "./Meter";
import { Paper } from "./Paper";
import { Scale } from "./Scale";

function statusLine(text: string, ignored: number[], lanes: Lane[]) {
  if (ignored.length > 0) return "Only the first four lines make sound.";
  if (!text.trim() || lanes.length === 0) return "";
  const effects = lanes.reduce((n, l) => n + l.modules.length, 0);
  const held = lanes.reduce((n, l) => n + l.modules.filter((m) => m.held).length, 0);
  return `${lanes.length} ${lanes.length === 1 ? "voice" : "voices"}, ${effects} ${effects === 1 ? "effect" : "effects"}, ${held} held.`;
}

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
    setText(next); // paint the character now; tokens and keys follow from core
    bridge.setText(next).then((p) => {
      // Ignore replies for text that has since changed. The reply renders as a transition so it yields to
      // typing: the first key-row mount took ~45 ms and queued keystrokes behind it (U1).
      if (p.text === latest.current) startTransition(() => setPayload(p));
    });
  };

  const onPress = (lane: number, slot: number) => {
    const held = payload?.chain[lane]?.modules[slot]?.held;
    bridge.setOverride({ lane, op: held ? "release" : "hold", slot }).then((r) => {
      if (r.ok) setPayload((p) => (p ? { ...p, chain: r.chain } : p));
    });
  };

  const fresh = payload && payload.text === text ? payload : null;
  const lanes = text.trim() ? (fresh?.chain ?? payload?.chain ?? []) : [];
  const ignored = fresh?.ignoredLines ?? [];
  return (
    <main className="machine">
      <header className="top">
        <span className="wordmark">Verbatim</span>
        <div className="top-r">
          <Meter />
        </div>
      </header>
      <Paper
        text={text}
        tokens={fresh?.tokens ?? null}
        ignored={ignored}
        seedHash={text.trim() ? (fresh?.seedHash ?? payload?.seedHash ?? "") : ""}
        onText={onText}
      />
      <Scale />
      <KeyRows lanes={lanes} onPress={onPress} />
      <p className="sr-only" role="status">
        {statusLine(text, ignored, lanes)}
      </p>
    </main>
  );
}
