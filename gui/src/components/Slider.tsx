// A carriage-scale slider bound to a host parameter: ruler track, red marker, value shown while dragging,
// double-click resets to the default. Gestures map to the host's begin / value / end.
import { useEffect, useReducer, useRef, useState } from "react";
import type { Icon } from "@phosphor-icons/react";
import type { ParamControl } from "../bridge/types";

type Props = { label: string; icon: Icon; control: ParamControl; defaultNormalised: number; format: (scaled: number) => string };

export function Slider({ label, icon: IconC, control, defaultNormalised, format }: Props) {
  const [, refresh] = useReducer((n: number) => n + 1, 0);
  const [dragging, setDragging] = useState(false);
  const gesture = useRef(false);
  useEffect(() => control.onChange(refresh), [control]);

  const n = control.normalised();
  const { start, end } = control.range();
  const text = format(start + (end - start) * n);

  const commit = (v: number) => {
    if (gesture.current) control.set(v);
    else {
      control.begin(); // keyboard or double-click: one complete gesture
      control.set(v);
      control.end();
    }
    refresh();
  };
  const stop = () => {
    if (!gesture.current) return;
    gesture.current = false;
    setDragging(false);
    control.end();
  };

  return (
    <label className="slider" style={{ "--pos": n } as React.CSSProperties}>
      <IconC size={20} aria-hidden="true" />
      <span className="slider-track">
        {dragging ? <span className="slider-value">{text}</span> : null}
        <input
          type="range"
          min={0}
          max={1}
          step={0.01}
          value={n}
          aria-label={label}
          aria-valuetext={text}
          onPointerDown={() => {
            gesture.current = true;
            setDragging(true);
            control.begin();
          }}
          onPointerUp={stop}
          onLostPointerCapture={stop}
          onChange={(e) => commit(Number(e.target.value))}
          onDoubleClick={() => commit(defaultNormalised)}
        />
      </span>
    </label>
  );
}
