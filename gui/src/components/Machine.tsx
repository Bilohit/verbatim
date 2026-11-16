// The typewriter machine: top bar, paper on its platen, carriage scale, key rows, and the Advanced drawer.
import { SlidersHorizontal } from "@phosphor-icons/react";
import { startTransition, useEffect, useRef, useState } from "react";
import { bridge, type Lane, type OverrideArgs, type TextPayload } from "../bridge";
import { AddMenu } from "./AddMenu";
import { Drawer } from "./Drawer";
import { KeyRows } from "./KeyRow";
import { Meter } from "./Meter";
import { Paper } from "./Paper";
import { Scale } from "./Scale";

const SAT = 0;

function statusLine(text: string, ignored: number[], lanes: Lane[]) {
  if (ignored.length > 0) return "Only the first four lines make sound.";
  if (!text.trim() || lanes.length === 0) return "";
  const effects = lanes.reduce((n, l) => n + l.modules.length, 0);
  const held = lanes.reduce((n, l) => n + l.modules.filter((m) => m.held).length, 0);
  return `${lanes.length} ${lanes.length === 1 ? "voice" : "voices"}, ${effects} ${effects === 1 ? "effect" : "effects"}, ${held} held.`;
}

export function Machine() {
  const [text, setText] = useState("");
  const [payload, setPayload] = useState<TextPayload | null>(null);
  const [advanced, setAdvanced] = useState(false);
  const [selected, setSelected] = useState<{ lane: number; slot: number } | null>(null);
  const [menu, setMenu] = useState<{ lane: number; button: HTMLButtonElement } | null>(null);
  const latest = useRef("");

  useEffect(() => {
    bridge.getState().then((s) => {
      latest.current = s.text;
      setText(s.text);
      setPayload(s);
      setAdvanced(!!s.ui?.advanced);
    });
  }, []);

  const onText = (next: string) => {
    latest.current = next;
    setText(next); // paint the character now; tokens and keys follow from core
    setSelected(null);
    bridge.setText(next).then((p) => {
      // Ignore replies for text that has since changed. The reply renders as a transition so it yields to
      // typing: the first key-row mount took ~45 ms and queued keystrokes behind it (U1).
      if (p.text === latest.current) startTransition(() => setPayload(p));
    });
  };

  const override = (args: OverrideArgs) =>
    bridge.setOverride(args).then((r) => {
      if (r.ok) setPayload((p) => (p ? { ...p, chain: r.chain } : p));
      return r.ok;
    });

  const onPress = (lane: number, slot: number) => {
    if (advanced) {
      setSelected({ lane, slot });
      return;
    }
    const held = payload?.chain[lane]?.modules[slot]?.held;
    override({ lane, op: held ? "release" : "hold", slot });
  };

  const focusAdd = (lane: number) =>
    requestAnimationFrame(() => document.querySelector<HTMLButtonElement>(`[data-add-lane="${lane}"]`)?.focus());

  const toggleAdvanced = () => {
    const next = !advanced;
    setAdvanced(next);
    setSelected(null);
    setMenu(null);
    bridge.setUi({ advanced: next });
  };

  useEffect(() => {
    const onKey = (e: KeyboardEvent) => {
      if (e.key !== "Escape") return;
      if (menu) {
        setMenu(null);
        menu.button.focus();
      } else if (selected) setSelected(null);
    };
    document.addEventListener("keydown", onKey);
    return () => document.removeEventListener("keydown", onKey);
  }, [menu, selected]);

  const fresh = payload && payload.text === text ? payload : null;
  const lanes = text.trim() ? (fresh?.chain ?? payload?.chain ?? []) : [];
  const ignored = fresh?.ignoredLines ?? [];
  const sel = selected ? lanes[selected.lane]?.modules[selected.slot] : undefined;

  return (
    <main className="machine" data-advanced={advanced ? "true" : undefined}>
      <header className="top">
        <span className="wordmark">Verbatim</span>
        <div className="top-r">
          <Meter />
          <button type="button" className="icon-btn" aria-label="Advanced" aria-pressed={advanced} onClick={toggleAdvanced}>
            <SlidersHorizontal size={22} aria-hidden="true" />
          </button>
        </div>
      </header>
      <Paper
        text={text}
        tokens={fresh?.tokens ?? null}
        ignored={ignored}
        seedHash={text.trim() ? (fresh?.seedHash ?? payload?.seedHash ?? "") : ""}
        onText={onText}
      />
      <Scale />
      <KeyRows lanes={lanes} advanced={advanced} selected={selected} onPress={onPress} onAdd={(lane, button) => setMenu({ lane, button })} />
      {menu ? (
        <AddMenu
          satCount={lanes[menu.lane]?.modules.filter((m) => m.type === SAT).length ?? 0}
          onClose={() => {
            menu.button.focus();
            setMenu(null);
          }}
          onPick={(type) => {
            const { lane, button } = menu;
            setMenu(null);
            override({ lane, op: "add", slot: -1, module: { type, p: [32768, 32768, 32768, 32768, 32768, 32768], mix: 32768 } }).then(() => button.focus());
          }}
        />
      ) : null}
      {advanced && selected && sel ? (
        <Drawer
          module={sel}
          onEdit={(p, mix) => override({ ...selected, op: "edit", module: { type: sel.type, p, mix } })}
          onHold={() => override({ ...selected, op: sel.held ? "release" : "hold" })}
          onRemove={() => {
            const lane = selected.lane;
            setSelected(null);
            override({ ...selected, op: "remove" }).then(() => focusAdd(lane));
          }}
        />
      ) : null}
      <p className="sr-only" role="status">
        {statusLine(text, ignored, lanes)}
      </p>
    </main>
  );
}
