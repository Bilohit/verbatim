// The typed text drawn from core's tokens: a faint copy, and a full-ink copy over it (living ink, M3, at T27).
import { memo } from "react";
import type { Token } from "../bridge/types";
import { segments } from "../text";

type Props = { lines: string[]; tokens: Token[][] | null; ignored: Set<number> };

function Line({ line, tokens, index, ignored }: { line: string; tokens?: Token[]; index: number; ignored: boolean }) {
  return (
    <div className="ink-line" data-line={index} data-ignored={ignored ? "true" : undefined}>
      {segments(line, tokens).map((s, i) =>
        s.cls ? (
          <span key={i} data-token={s.cls} className={`t-${s.cls}`}>
            {s.text}
          </span>
        ) : (
          s.text
        ),
      )}
      {"\n"}
    </div>
  );
}

export const InkLayer = memo(function InkLayer({ lines, tokens, ignored }: Props) {
  return (
    <div className="ink" aria-hidden="true">
      <div className="ink-layer" data-ink="cold">
        {lines.map((line, i) => (
          <Line key={i} line={line} tokens={tokens?.[i]} index={i} ignored={ignored.has(i)} />
        ))}
      </div>
    </div>
  );
});
