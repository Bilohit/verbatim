// One row of keys per voice (lane), staggered like a keyboard.
import type { Lane } from "../bridge/types";
import { Key } from "./Key";

type Props = { lanes: Lane[]; onPress: (lane: number, slot: number) => void };

export function KeyRows({ lanes, onPress }: Props) {
  return (
    <div className="rows" data-rows>
      {lanes.map((lane) => (
        <ol key={lane.lane} className="row" aria-label={`Voice ${lane.lane + 1}`} style={{ "--row": lane.lane } as React.CSSProperties}>
          {lane.modules.map((m, slot) => (
            <li key={`${slot}:${m.type}`}>
              <Key module={m} onPress={() => onPress(lane.lane, slot)} />
            </li>
          ))}
        </ol>
      ))}
    </div>
  );
}
