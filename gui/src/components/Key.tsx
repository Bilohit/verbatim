// One effect key: a round icon button that latches down while held (aria-pressed; M2). Selected in Advanced: ink ring.
import { motion } from "framer-motion";
import type { Module } from "../bridge/types";
import { effect } from "../icons";
import { latch, useMotionPrefs } from "../motion";

type Props = { module: Module; selected?: boolean; onPress: () => void };

export function Key({ module, selected = false, onPress }: Props) {
  const { reduce } = useMotionPrefs();
  const { name, icon: Icon } = effect(module.type);
  return (
    <div className="key-wrap">
      <motion.button
        type="button"
        className="key"
        data-selected={selected ? "true" : undefined}
        aria-pressed={module.held}
        aria-label={name}
        onClick={onPress}
        {...latch(reduce, module.held)}
      >
        <Icon size={22} weight="regular" aria-hidden="true" />
      </motion.button>
      <span className="key-tip" aria-hidden="true">
        {name}
      </span>
    </div>
  );
}
