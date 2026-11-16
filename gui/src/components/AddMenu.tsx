// Popover grid of the 16 effects. SAT is disabled when the lane already has 2 (latency budget, R4). Esc closes.
import { useEffect, useRef } from "react";
import { EFFECTS } from "../icons";

const SAT = 0;

type Props = { satCount: number; onPick: (type: number) => void; onClose: () => void };

export function AddMenu({ satCount, onPick, onClose }: Props) {
  const ref = useRef<HTMLDivElement>(null);
  useEffect(() => {
    ref.current?.querySelector<HTMLButtonElement>("button:not(:disabled)")?.focus();
  }, []);
  return (
    <div
      ref={ref}
      className="add-menu"
      role="menu"
      aria-label="Add effect"
      onKeyDown={(e) => {
        if (e.key === "Escape") {
          e.stopPropagation();
          onClose();
        }
      }}
    >
      {EFFECTS.map(({ name, icon: Icon }, type) => (
        <button key={type} type="button" role="menuitem" className="add-item" disabled={type === SAT && satCount >= 2} onClick={() => onPick(type)}>
          <Icon size={22} aria-hidden="true" />
          <span>{name}</span>
        </button>
      ))}
    </div>
  );
}
