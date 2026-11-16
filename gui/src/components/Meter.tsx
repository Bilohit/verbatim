// Output meter: two peak bars fed by `levels` (30 Hz) and a red clip tick that holds for 1 s.
// Updated through refs, never React state, so 30 Hz levels do not re-render the editor.
import { useEffect, useRef } from "react";
import { bridge } from "../bridge";

const toBar = (peak: number) => Math.max(0, Math.min(1, (20 * Math.log10(Math.max(peak, 1e-6)) + 60) / 60)); // -60..0 dBFS

export function Meter() {
  const root = useRef<HTMLDivElement>(null);
  const bars = useRef<(HTMLElement | null)[]>([]);
  const tick = useRef<HTMLElement>(null);
  useEffect(() => {
    let clipTimer = 0;
    return bridge.on("levels", (l) => {
      l.out.forEach((p, i) => bars.current[i]?.style.setProperty("transform", `scaleX(${toBar(p)})`));
      const db = Math.round(Math.max(-60, Math.min(0, 20 * Math.log10(Math.max(l.out[0], l.out[1], 1e-6)))));
      root.current?.setAttribute("aria-valuenow", String(db));
      if (l.clip && tick.current) {
        tick.current.hidden = false;
        window.clearTimeout(clipTimer);
        clipTimer = window.setTimeout(() => tick.current && (tick.current.hidden = true), 1000);
      }
    });
  }, []);
  return (
    <div ref={root} className="meter" role="meter" aria-label="Output level" aria-valuemin={-60} aria-valuemax={0} aria-valuenow={-60}>
      <i ref={(el) => (bars.current[0] = el)} />
      <i ref={(el) => (bars.current[1] = el)} />
      <b ref={tick} className="clip-tick" hidden />
    </div>
  );
}
