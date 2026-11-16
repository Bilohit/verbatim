// Payloads exchanged with the plugin (docs/05-ui-design.md section 6; plugin/Bridge.cpp).
export type TokenClass = "word" | "direction" | "op" | "number" | "ignored";
export type Token = { start: number; length: number; class: TokenClass }; // UTF-8 byte offsets within the line

export type Param = { name: string; index: number; u: number; value: number; text: string };
export type Module = { type: number; name: string; held: boolean; p?: number[]; mix?: number; params?: Param[] };
export type Lane = { lane: number; modules: Module[] };

export type TextPayload = {
  text: string;
  tokens: Token[][];
  chain: Lane[];
  seedHash: string;
  ignoredLines: number[];
  full: boolean;
  stateError?: boolean;
};

export type UiState = { w: number; h: number; advanced: boolean };
export type State = TextPayload & { ui: UiState };

export type OverrideOp = "hold" | "release" | "edit" | "remove" | "add";
export type OverrideArgs = { lane: number; op: OverrideOp; slot: number; module?: { type: number; p: number[]; mix: number } };

export type Levels = { lanes: number[]; out: [number, number]; clip: boolean };

// A host parameter (mix, macro, output) as the UI sees it: normalised 0..1 plus the scaled value.
export interface ParamControl {
  normalised(): number;
  scaled(): number;
  range(): { start: number; end: number };
  set(normalised: number): void;
  begin(): void;
  end(): void;
  onChange(cb: () => void): () => void;
}

export interface Bridge {
  setText(text: string): Promise<TextPayload>;
  setOverride(args: OverrideArgs): Promise<{ ok: boolean; chain: Lane[] }>;
  setUi(ui: Partial<UiState>): Promise<void>;
  getState(): Promise<State>;
  param(name: "mix" | "macro" | "output"): ParamControl;
  on(event: "levels", cb: (l: Levels) => void): () => void;
  on(event: "chain", cb: (s: State) => void): () => void;
}
