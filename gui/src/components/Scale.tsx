// The carriage scale: Mix, Macro, Output, bound to the host parameters.
import { DropHalf, MagicWand, SpeakerHigh } from "@phosphor-icons/react";
import { useMemo } from "react";
import { bridge } from "../bridge";
import { Slider } from "./Slider";

export function Scale() {
  const mix = useMemo(() => bridge.param("mix"), []);
  const macro = useMemo(() => bridge.param("macro"), []);
  const output = useMemo(() => bridge.param("output"), []);
  return (
    <div className="scale">
      <Slider label="Mix" icon={DropHalf} control={mix} defaultNormalised={1} format={(v) => `${Math.round(v)} %`} />
      <Slider label="Macro" icon={MagicWand} control={macro} defaultNormalised={0.5} format={(v) => v.toFixed(2)} />
      <Slider label="Output" icon={SpeakerHigh} control={output} defaultNormalised={24 / 36} format={(v) => `${v.toFixed(1)} dB`} />
    </div>
  );
}
