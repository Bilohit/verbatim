// Mock bridge for browser tests (/?mock). Worked examples come from core via tests/fixtures/core.json
// (vb_spec --ui-fixture); other text gets a plain word split, which is enough for layout and timing tests.
import fixture from "../../tests/fixtures/core.json";
import type { Bridge, Lane, Levels, OverrideArgs, ParamControl, State, TextPayload, Token, UiState } from "./types";

// Host parameter ranges and defaults (docs/02-spec.md section 1).
const PARAMS = { mix: { start: 0, end: 100, def: 100 }, macro: { start: 0, end: 1, def: 0.5 }, output: { start: -24, end: 12, def: 0 } };

const known = new Map((fixture as unknown as TextPayload[]).map((f) => [f.text, f]));

function plainTokens(text: string): Token[][] {
  const enc = new TextEncoder();
  return text.split("\n").map((line) => {
    const out: Token[] = [];
    let byte = 0;
    for (const part of line.split(/(\s+)/)) {
      const len = enc.encode(part).length;
      if (part && !/^\s+$/.test(part)) out.push({ start: byte, length: len, class: "word" });
      byte += len;
    }
    return out;
  });
}

// Deterministic stand-in chain for text outside the fixture: 2-6 effects per non-blank line, max 4 lanes.
function pseudoChain(text: string): Lane[] {
  const lanes: Lane[] = [];
  for (const line of text.split("\n")) {
    if (!line.trim() || lanes.length === 4) continue;
    let h = 2166136261;
    for (const c of line) h = Math.imul(h ^ c.charCodeAt(0), 16777619) >>> 0;
    const n = 2 + (h % 5);
    lanes.push({ lane: lanes.length, modules: Array.from({ length: n }, (_, k) => ({ type: (h >>> (k * 3)) % 16, name: "FX", held: false })) });
  }
  return lanes;
}

export function createMockBridge(delayMs = 0): Bridge & { calls: { name: string; args: unknown }[] } {
  let current: TextPayload = { text: "", tokens: [[]], chain: [], seedHash: "", ignoredLines: [], full: false };
  let ui: UiState = { w: 900, h: 620, advanced: false };
  const calls: { name: string; args: unknown }[] = [];
  const levelsListeners = new Set<(l: Levels) => void>();
  const params = new Map<string, { value: number; listeners: Set<() => void> }>();
  const paramState = (name: keyof typeof PARAMS) => {
    if (!params.has(name)) {
      const r = PARAMS[name];
      params.set(name, { value: (r.def - r.start) / (r.end - r.start), listeners: new Set() });
    }
    return params.get(name)!;
  };
  const later = <T,>(v: T) => new Promise<T>((r) => setTimeout(() => r(v), delayMs));

  const bridge = {
    calls,
    setText(text) {
      calls.push({ name: "setText", args: text });
      const hit = known.get(text);
      current = hit ?? { text, tokens: plainTokens(text), chain: pseudoChain(text), seedHash: text.trim() ? "0000beef" : "", ignoredLines: [], full: new TextEncoder().encode(text).length >= 4096 };
      return later(current);
    },
    setOverride(args: OverrideArgs) {
      calls.push({ name: "setOverride", args });
      const chain: Lane[] = current.chain.map((l) => ({ ...l, modules: l.modules.map((m) => ({ ...m })) }));
      const lane = chain[args.lane];
      let ok = !!lane;
      if (lane && (args.op === "hold" || args.op === "release") && lane.modules[args.slot]) lane.modules[args.slot].held = args.op === "hold";
      else if (lane && args.op === "remove") lane.modules.splice(args.slot, 1);
      else if (lane && args.op === "add" && args.module) lane.modules.push({ type: args.module.type, name: "FX", held: true });
      else if (lane && args.op === "edit" && lane.modules[args.slot]) lane.modules[args.slot].held = true;
      else ok = false;
      current = { ...current, chain };
      return later({ ok, chain });
    },
    setUi(next) {
      calls.push({ name: "setUi", args: next });
      ui = { ...ui, ...next };
      return later(undefined);
    },
    getState() {
      calls.push({ name: "getState", args: null });
      return later({ ...current, ui } as State);
    },
    param(name: keyof typeof PARAMS): ParamControl {
      const st = paramState(name), r = PARAMS[name];
      return {
        normalised: () => st.value,
        scaled: () => r.start + (r.end - r.start) * st.value,
        range: () => ({ start: r.start, end: r.end }),
        set: (v) => {
          st.value = Math.min(1, Math.max(0, v));
          calls.push({ name: `param:${name}`, args: { op: "set", value: st.value } });
        },
        begin: () => calls.push({ name: `param:${name}`, args: { op: "begin" } }),
        end: () => calls.push({ name: `param:${name}`, args: { op: "end" } }),
        onChange: (cb) => {
          st.listeners.add(cb);
          return () => st.listeners.delete(cb);
        },
      };
    },
    hostSet(name: keyof typeof PARAMS, v: number) {
      const st = paramState(name);
      st.value = v;
      st.listeners.forEach((cb) => cb());
    },
    emitLevels(l: Levels) {
      levelsListeners.forEach((cb) => cb(l));
    },
    on(event: string, cb: (x: never) => void) {
      if (event === "levels") levelsListeners.add(cb as (l: Levels) => void);
      return () => levelsListeners.delete(cb as (l: Levels) => void);
    },
  } as Bridge & { calls: { name: string; args: unknown }[] };
  (globalThis as unknown as { __mockBridge: unknown }).__mockBridge = bridge; // tests read the recorded calls
  return bridge;
}
