// Shared motion: easings, durations and variants for M1-M8 (docs/05-ui-design.md section 5), and the
// reduced-motion switch. Only transform and opacity animate; continuous values use motion values.
import { type Transition, type Variants } from "framer-motion";
import { useSyncExternalStore } from "react";

export const EASE_OUT: [number, number, number, number] = [0.16, 1, 0.3, 1];

// Reduced motion: M1, M6, M7 become 120 ms opacity fades; M2 instant; M3 5 Hz, no spring, no phase;
// M4, M5 off; M8 instant.
const query = typeof window !== "undefined" ? window.matchMedia("(prefers-reduced-motion: reduce)") : null;
const subscribe = (cb: () => void) => {
  query?.addEventListener("change", cb);
  return () => query?.removeEventListener("change", cb);
};

export function useMotionPrefs() {
  const reduce = useSyncExternalStore(subscribe, () => query?.matches ?? false);
  return { reduce };
}

const fade: Transition = { duration: 0.12, ease: "linear" };

// M1 keys drop in / lift out; unchanged keys keep their element and do not move.
export const keyVariants = (reduce: boolean): Variants =>
  reduce
    ? { initial: { opacity: 0 }, enter: { opacity: 1, transition: fade }, exit: { opacity: 0, transition: fade } }
    : {
        initial: { opacity: 0, y: -8 },
        enter: (i: number) => ({ opacity: 1, y: 0, transition: { duration: 0.24, ease: EASE_OUT, delay: i * 0.04 } }),
        exit: { opacity: 0, transition: { duration: 0.12, ease: EASE_OUT } },
      };

// M2 key latch.
export const latch = (reduce: boolean, held: boolean) => ({
  animate: { y: held ? 4 : 0 },
  whileTap: { y: 4 },
  transition: reduce ? { duration: 0 } : { duration: 0.09, ease: EASE_OUT },
});

// M3 living ink: spring for the per-line level.
export const INK_SPRING = { stiffness: 180, damping: 24 };
export const INK_PHASE_MAX_CHARS = 200;
// Per-character phase only while the whole text is short: thousands of animating spans cost a frame a keystroke (U8).
export const INK_PHASE_MAX_TEXT = 1200;
export const REDUCED_INK_HZ = 5;

// M4 typebar strike.
export const strike = { initial: { scale: 1.12, opacity: 1 }, animate: { scale: 1 }, transition: { duration: 0.09, ease: EASE_OUT } };

// M5 line feed.
export const LINE_FEED = { duration: 0.16, ease: EASE_OUT };

// M6 postmark stamp.
export const stamp = (reduce: boolean) =>
  reduce
    ? { initial: { opacity: 0 }, animate: { opacity: 1 }, transition: fade }
    : { initial: { opacity: 0, scale: 1.25 }, animate: { opacity: 1, scale: 1 }, transition: { duration: 0.15, ease: EASE_OUT } };

// M7 drawer.
export const drawer = (reduce: boolean) =>
  reduce
    ? { initial: { opacity: 0 }, animate: { opacity: 1 }, exit: { opacity: 0 }, transition: fade }
    : { initial: { opacity: 0, y: 12 }, animate: { opacity: 1, y: 0 }, exit: { opacity: 0, y: 12 }, transition: { duration: 0.22, ease: EASE_OUT } };

// M8 roll typing.
export const ROLL_MS_PER_CHAR = 25;
