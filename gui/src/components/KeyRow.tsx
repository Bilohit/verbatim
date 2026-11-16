// One row of keys per voice (lane), staggered like a keyboard. In Advanced each row ends with a "+" key.
import { Plus } from "@phosphor-icons/react";
import type { Lane } from "../bridge/types";
import { Key } from "./Key";

export const MAX_LANE_MODULES = 8;

type Props = {
  lanes: Lane[];
  advanced: boolean;
  selected: { lane: number; slot: number } | null;
  onPress: (lane: number, slot: number) => void;
  onAdd: (lane: number, button: HTMLButtonElement) => void;
};

export function KeyRows({ lanes, advanced, selected, onPress, onAdd }: Props) {
  return (
    <div className="rows" data-rows>
      {lanes.map((lane) => (
        <ol key={lane.lane} className="row" aria-label={`Voice ${lane.lane + 1}`} style={{ "--row": lane.lane } as React.CSSProperties}>
          {lane.modules.map((m, slot) => (
            <li key={`${slot}:${m.type}`}>
              <Key module={m} selected={selected?.lane === lane.lane && selected.slot === slot} onPress={() => onPress(lane.lane, slot)} />
            </li>
          ))}
          {advanced ? (
            <li>
              <button
                type="button"
                className="key key-ghost"
                data-add-lane={lane.lane}
                aria-label={`Add effect to voice ${lane.lane + 1}`}
                aria-haspopup="menu"
                disabled={lane.modules.length >= MAX_LANE_MODULES}
                onClick={(e) => onAdd(lane.lane, e.currentTarget)}
              >
                <Plus size={22} aria-hidden="true" />
              </button>
            </li>
          ) : null}
        </ol>
      ))}
    </div>
  );
}
