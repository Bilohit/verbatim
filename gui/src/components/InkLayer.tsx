// The typed text drawn from core's tokens: a faint copy, and a full-ink copy over it whose per-line opacity
// follows that line's lane level (M3, living ink). The newest character strikes in (M4).
import { motion, useMotionValue, useSpring, type MotionValue } from "framer-motion";
import { memo, useEffect } from "react";
import { bridge } from "../bridge";
import type { Token } from "../bridge/types";
import { INK_PHASE_MAX_CHARS, INK_PHASE_MAX_TEXT, INK_SPRING, REDUCED_INK_HZ, strike, useMotionPrefs } from "../motion";
import { segments } from "../text";

type Props = { lines: string[]; tokens: Token[][] | null; ignored: Set<number>; struck: { line: number; col: number } | null };

const ColdLine = memo(function ColdLine({ line, tokens, index, ignored }: { line: string; tokens?: Token[]; index: number; ignored: boolean }) {
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
});

const HotLine = memo(function HotLine({ line, index, level, phase, struckCol }: { line: string; index: number; level: MotionValue<number>; phase: boolean; struckCol: number | null }) {
  const chars = [...line];
  return (
    <motion.div className="ink-line" data-line={index} style={{ opacity: level }}>
      {phase
        ? chars.map((c, i) =>
            i === struckCol ? (
              <motion.span key={i} data-ch data-strike className="strike" style={{ "--i": i } as React.CSSProperties} {...strike}>
                {c}
              </motion.span>
            ) : (
              <span key={i} data-ch style={{ "--i": i } as React.CSSProperties}>
                {c}
              </span>
            ),
          )
        : line}
      {"\n"}
    </motion.div>
  );
});

// One spring per lane, fed by the 30 Hz `levels` event (reduced motion: plain value updated at 5 Hz).
function useLaneLevels(reduce: boolean) {
  const raw = [useMotionValue(0), useMotionValue(0), useMotionValue(0), useMotionValue(0)];
  const sprung = [useSpring(raw[0], INK_SPRING), useSpring(raw[1], INK_SPRING), useSpring(raw[2], INK_SPRING), useSpring(raw[3], INK_SPRING)];
  useEffect(() => {
    let last = 0;
    return bridge.on("levels", (l) => {
      const now = performance.now();
      if (reduce && now - last < 1000 / REDUCED_INK_HZ) return;
      last = now;
      l.lanes.forEach((rms, i) => raw[i]?.set(Math.min(1, rms / 0.2512)));
    });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [reduce]);
  return reduce ? raw : sprung;
}

export const InkLayer = memo(function InkLayer({ lines, tokens, ignored, struck }: Props) {
  const { reduce } = useMotionPrefs();
  const lanes = useLaneLevels(reduce);
  const quiet = useMotionValue(0);
  const shortText = lines.reduce((n, l) => n + l.length, 0) <= INK_PHASE_MAX_TEXT;
  // Line -> lane: non-blank lines in order (grammar section 2, P4/P5).
  let lane = 0;
  const laneOf = lines.map((l) => (l.trim() && lane < 4 ? lane++ : -1));
  return (
    <div className="ink" aria-hidden="true">
      <div className="ink-layer" data-ink="cold">
        {lines.map((line, i) => (
          <ColdLine key={i} line={line} tokens={tokens?.[i]} index={i} ignored={ignored.has(i)} />
        ))}
      </div>
      <div className="ink-layer ink-hot" data-ink="hot">
        {lines.map((line, i) => (
          <HotLine
            key={i}
            line={line}
            index={i}
            level={laneOf[i] >= 0 ? lanes[laneOf[i]] : quiet}
            phase={!reduce && shortText && line.length <= INK_PHASE_MAX_CHARS}
            struckCol={!reduce && struck?.line === i ? struck.col : null}
          />
        ))}
      </div>
    </div>
  );
});
