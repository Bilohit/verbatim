// The paper: a transparent native textarea over the drawn ink layers, so typing stays native-fast.
import type { Token } from "../bridge/types";
import { InkLayer } from "./InkLayer";
import { Postmark } from "./Postmark";

type Props = {
  text: string;
  tokens: Token[][] | null;
  ignored: number[];
  seedHash: string;
  onText: (text: string) => void;
};

export function Paper({ text, tokens, ignored, seedHash, onText }: Props) {
  const lines = text.split("\n");
  return (
    <div className="paper" data-paper>
      <Postmark hash={seedHash} />
      <div className="paper-scroll">
        <div className="editor">
          <InkLayer lines={lines} tokens={tokens} ignored={new Set(ignored)} />
          <textarea
            aria-label="Seed text"
            autoFocus
            spellCheck={false}
            autoComplete="off"
            autoCorrect="off"
            autoCapitalize="off"
            value={text}
            onChange={(e) => onText(e.target.value)}
          />
        </div>
      </div>
    </div>
  );
}
