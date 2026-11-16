// One effect key: a round icon button that latches down while held (aria-pressed). Selected in Advanced: ink ring.
import type { Module } from "../bridge/types";
import { effect } from "../icons";

type Props = { module: Module; selected?: boolean; onPress: () => void };

export function Key({ module, selected = false, onPress }: Props) {
  const { name, icon: Icon } = effect(module.type);
  return (
    <div className="key-wrap">
      <button
        type="button"
        className="key"
        data-selected={selected ? "true" : undefined}
        aria-pressed={module.held}
        aria-label={name}
        onClick={onPress}
      >
        <Icon size={22} weight="regular" aria-hidden="true" />
      </button>
      <span className="key-tip" aria-hidden="true">
        {name}
      </span>
    </div>
  );
}
