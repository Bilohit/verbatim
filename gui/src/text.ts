// Splits a line into plain and token segments from core's byte offsets (the UI never re-implements the grammar).
import type { Token, TokenClass } from "./bridge/types";

export type Segment = { text: string; cls?: TokenClass };

const enc = new TextEncoder();

// UTF-16 index for each UTF-8 byte boundary in the line.
function byteToIndex(line: string): Map<number, number> {
  const map = new Map<number, number>();
  let byte = 0, idx = 0;
  map.set(0, 0);
  for (const ch of line) {
    byte += enc.encode(ch).length;
    idx += ch.length;
    map.set(byte, idx);
  }
  return map;
}

export function segments(line: string, tokens: Token[] | undefined): Segment[] {
  if (!tokens || tokens.length === 0) return line ? [{ text: line }] : [];
  const at = byteToIndex(line);
  const out: Segment[] = [];
  let pos = 0;
  for (const t of tokens) {
    const s = at.get(t.start), e = at.get(t.start + t.length);
    if (s === undefined || e === undefined || s < pos) return [{ text: line }]; // stale tokens: plain text
    if (s > pos) out.push({ text: line.slice(pos, s) });
    out.push({ text: line.slice(s, e), cls: t.class });
    pos = e;
  }
  if (pos < line.length) out.push({ text: line.slice(pos) });
  return out;
}
