// UI -> C++ bridge (docs/05-ui-design.md section 6). Native functions are registered by plugin/Bridge.cpp.
import { getNativeFunction } from "./juce/index.js";

type State = { text: string };

const inPlugin = typeof window !== "undefined" && "__JUCE__" in window;

export const setText: (text: string) => Promise<unknown> = inPlugin
  ? getNativeFunction("setText")
  : async () => undefined;

export const getState: () => Promise<State> = inPlugin
  ? getNativeFunction("getState")
  : async () => ({ text: "" });
