// The paper: a transparent native textarea over the drawn ink layers, so typing stays native-fast.
// M4: the newest character strikes in. M5: Enter feeds the paper up one line.
import { useAnimate } from "framer-motion";
import { useRef, useState } from "react";
import type { Token } from "../bridge/types";
import { LINE_FEED, useMotionPrefs } from "../motion";
import { InkLayer } from "./InkLayer";
import { Postmark } from "./Postmark";

type Props = {
  text: string;
  tokens: Token[][] | null;
  ignored: number[];
  seedHash: string;
  onText: (text: string) => void;
  readOnly?: boolean;
  shake?: number; // E2: changes when input was refused
};

export function Paper({ text, tokens, ignored, seedHash, onText, readOnly = false, shake = 0 }: Props) {
  const { reduce } = useMotionPrefs();
  const [scope, animate] = useAnimate();
  const [struck, setStruck] = useState<{ line: number; col: number } | null>(null);
  const prev = useRef(text);
  const lines = text.split("\n");

  const change = (next: string, caret: number) => {
    const before = prev.current;
    prev.current = next;
    if (!reduce && next.length === before.length + 1) {
      const head = next.slice(0, caret);
      const line = head.split("\n").length - 1;
      if (next[caret - 1] === "\n") {
        setStruck(null);
        // M5 line feed: the paper moves up one line like a carriage return.
        animate(scope.current, { y: ["calc(var(--text-size) * var(--text-line))", "0px"] }, LINE_FEED);
      } else setStruck({ line, col: [...head.slice(head.lastIndexOf("\n") + 1)].length - 1 });
    } else setStruck(null);
    onText(next);
  };

  return (
    <div className="paper" data-paper>
      <Postmark hash={seedHash} shake={shake} />
      <div className="paper-scroll">
        <div className="editor" ref={scope}>
          <InkLayer lines={lines} tokens={tokens} ignored={new Set(ignored)} struck={struck} />
          <textarea
            aria-label="Seed text"
            autoFocus
            spellCheck={false}
            autoComplete="off"
            autoCorrect="off"
            autoCapitalize="off"
            value={text}
            readOnly={readOnly}
            onChange={(e) => change(e.target.value, e.target.selectionStart ?? e.target.value.length)}
          />
        </div>
      </div>
    </div>
  );
}
