// Round red stamp with the first 8 hex digits of the lane-seed hash; hidden when there is no text.
export function Postmark({ hash }: { hash: string }) {
  if (!hash) return null;
  return (
    <span className="postmark" role="img" aria-label={`Seed ${hash}`}>
      {hash}
    </span>
  );
}
