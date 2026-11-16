// Effect icons and names (docs/05-ui-design.md section 4), indexed by catalog ID.
import {
  AirplaneTilt, ArrowsInLineVertical, ArrowsVertical, BellSimple, DotsNine, Fire, Funnel, Hash, Snowflake, Spiral,
  Stairs, Timer, UsersThree, VinylRecord, WaveSine, Waves, type Icon,
} from "@phosphor-icons/react";

export const EFFECTS: { name: string; icon: Icon }[] = [
  { name: "Saturation", icon: Fire },
  { name: "Bitcrush", icon: Stairs },
  { name: "Filter", icon: Funnel },
  { name: "Delay", icon: Timer },
  { name: "Reverb", icon: Waves },
  { name: "Chorus", icon: UsersThree },
  { name: "Phaser", icon: Spiral },
  { name: "Tremolo", icon: WaveSine },
  { name: "Tape wow", icon: VinylRecord },
  { name: "Freeze", icon: Snowflake },
  { name: "Comb", icon: Hash },
  { name: "Ring mod", icon: BellSimple },
  { name: "Pitch", icon: ArrowsVertical },
  { name: "Granular", icon: DotsNine },
  { name: "Compressor", icon: ArrowsInLineVertical },
  { name: "Flanger", icon: AirplaneTilt },
];

export const effect = (type: number) => EFFECTS[type] ?? EFFECTS[0];
