// Advanced drawer for the selected effect: labelled sliders with units (values come mapped from core),
// hold toggle, remove. Any slider edit auto-holds (docs/02-spec.md R2).
import { LockSimple, LockSimpleOpen, Trash } from "@phosphor-icons/react";
import { motion } from "framer-motion";
import type { Module } from "../bridge/types";
import { drawer, useMotionPrefs } from "../motion";
import { effect } from "../icons";

type Props = {
  module: Module;
  onEdit: (p: number[], mix: number) => void;
  onHold: () => void;
  onRemove: () => void;
};

export function Drawer({ module, onEdit, onHold, onRemove }: Props) {
  const { reduce } = useMotionPrefs();
  const { name } = effect(module.type);
  const p = module.p ?? [32768, 32768, 32768, 32768, 32768, 32768];
  const mix = module.mix ?? 32768;
  const send = (index: number, u: number) => (index === 6 ? onEdit(p, u) : onEdit(p.map((v, i) => (i === index ? u : v)), mix));
  return (
    <motion.section className="drawer" aria-label={name} {...drawer(reduce)}>
      <h2 className="drawer-name">{name}</h2>
      <div className="drawer-params">
        {(module.params ?? []).map((q) => (
          <label key={q.index} className="param">
            <span className="param-name">{q.name}</span>
            <input
              type="range"
              min={0}
              max={65535}
              step={1} // core values are not on any step grid; a grid would snap them and fire a spurious edit
              value={q.u}
              aria-label={q.name}
              aria-valuetext={q.text}
              onKeyDown={(e) => {
                const delta = { ArrowRight: 656, ArrowUp: 656, ArrowLeft: -656, ArrowDown: -656, PageUp: 6554, PageDown: -6554 }[e.key];
                const to = e.key === "Home" ? 0 : e.key === "End" ? 65535 : delta === undefined ? null : Math.min(65535, Math.max(0, q.u + delta));
                if (to === null) return;
                e.preventDefault();
                send(q.index, to);
              }}
              onChange={(e) => send(q.index, Number(e.target.value))}
            />
            <span className="param-value">{q.text}</span>
          </label>
        ))}
      </div>
      <div className="drawer-actions">
        <button type="button" className="icon-btn" aria-label="Hold" aria-pressed={module.held} onClick={onHold}>
          {module.held ? <LockSimple size={20} aria-hidden="true" /> : <LockSimpleOpen size={20} aria-hidden="true" />}
        </button>
        <button type="button" className="icon-btn" aria-label="Remove" onClick={onRemove}>
          <Trash size={20} aria-hidden="true" />
        </button>
      </div>
    </motion.section>
  );
}
