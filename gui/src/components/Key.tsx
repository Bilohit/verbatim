// One effect key: a round icon button that latches down while held (aria-pressed).
import type { Module } from "../bridge/types";
import { effect } from "../icons";

type Props = { module: Module; onPress: () => void };

export function Key({ module, onPress }: Props) {
  const { name, icon: Icon } = effect(module.type);
  return (
    <div className="key-wrap">
      <button type="button" className="key" aria-pressed={module.held} aria-label={name} onClick={onPress}>
        <Icon size={22} weight="regular" aria-hidden="true" />
      </button>
      <span className="key-tip" aria-hidden="true">
        {name}
      </span>
    </div>
  );
}
