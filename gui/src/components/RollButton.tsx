// Roll: replaces the text with 2 to 5 random words from the bundled list, typed at 25 ms per character (M8;
// reduced motion: at once). Typing goes through the native editing commands, so Ctrl+Z restores the old text.
import { DiceFive } from "@phosphor-icons/react";
import { useRef } from "react";
import { ROLL_MS_PER_CHAR, useMotionPrefs } from "../motion";
import { WORDS } from "../words";

const pick = () => {
  const n = 2 + Math.floor(Math.random() * 4);
  return Array.from({ length: n }, () => WORDS[Math.floor(Math.random() * WORDS.length)]).join(" ");
};

export function RollButton({ disabled }: { disabled?: boolean }) {
  const { reduce } = useMotionPrefs();
  const timer = useRef(0);
  const roll = () => {
    const area = document.querySelector<HTMLTextAreaElement>("textarea[aria-label='Seed text']");
    if (!area) return;
    window.clearTimeout(timer.current);
    const phrase = pick();
    area.focus();
    area.select();
    if (reduce) {
      document.execCommand("insertText", false, phrase);
      return;
    }
    let i = 0;
    const step = () => {
      document.execCommand("insertText", false, phrase[i]); // the first character replaces the selection
      if (++i < phrase.length) timer.current = window.setTimeout(step, ROLL_MS_PER_CHAR);
    };
    step();
  };
  return (
    <button type="button" className="icon-btn" aria-label="Roll" onClick={roll} disabled={disabled}>
      <DiceFive size={22} aria-hidden="true" />
    </button>
  );
}
