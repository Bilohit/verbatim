// Bridge selection: the plugin's JUCE backend when present, the mock for browser tests (/?mock).
import { getNativeFunction } from "../juce/index.js";
import { createMockBridge } from "./mock";
import type { Bridge } from "./types";

type JuceWindow = { __JUCE__?: { backend: { addEventListener: (id: string, fn: (p: unknown) => void) => unknown; removeEventListener: (token: unknown) => void } } };

function createJuceBridge(): Bridge {
  const backend = (window as unknown as JuceWindow).__JUCE__!.backend;
  const setText = getNativeFunction("setText");
  const setOverride = getNativeFunction("setOverride");
  const setUi = getNativeFunction("setUi");
  const getState = getNativeFunction("getState");
  return {
    setText: (text) => setText(text),
    setOverride: (args) => setOverride(args),
    setUi: (ui) => setUi(ui),
    getState: () => getState(),
    on(event: string, cb: (x: never) => void) {
      const token = backend.addEventListener(event, cb as (p: unknown) => void);
      return () => backend.removeEventListener(token);
    },
  } as Bridge;
}

// JUCE's frontend library defines a placeholder window.__JUCE__ on import, so detect the real plugin by the
// native functions its backend registered.
type JuceInit = { __JUCE__?: { initialisationData?: { __juce__functions?: string[] } } };
const inPlugin =
  typeof window !== "undefined" &&
  ((window as unknown as JuceInit).__JUCE__?.initialisationData?.__juce__functions ?? []).includes("setText");
const mockDelay = Number(new URLSearchParams(typeof location !== "undefined" ? location.search : "").get("delay") ?? 0);

export const bridge: Bridge = inPlugin ? createJuceBridge() : createMockBridge(mockDelay);
export type * from "./types";
