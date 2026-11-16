// One row of keys per voice (lane), staggered like a keyboard. In Advanced each row ends with a "+" key.
import { Plus } from "@phosphor-icons/react";
import { AnimatePresence, motion } from "framer-motion";
import { keyVariants, useMotionPrefs } from "../motion";
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
  const { reduce } = useMotionPrefs();
  const variants = keyVariants(reduce);
  return (
    <div className="rows" data-rows>
      {lanes.map((lane) => (
        <ol key={lane.lane} className="row" aria-label={`Voice ${lane.lane + 1}`} style={{ "--row": lane.lane } as React.CSSProperties}>
          {/* M1: changed keys drop in, gone keys lift out; a key with the same slot and type keeps its element. */}
          <AnimatePresence initial={false} mode="popLayout">
            {lane.modules.map((m, slot) => (
              <motion.li key={`${slot}:${m.type}`} custom={slot} variants={variants} initial="initial" animate="enter" exit="exit">
                <Key module={m} selected={selected?.lane === lane.lane && selected.slot === slot} onPress={() => onPress(lane.lane, slot)} />
              </motion.li>
            ))}
          </AnimatePresence>
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
