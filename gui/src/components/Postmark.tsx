// Round red stamp with the first 8 hex digits of the lane-seed hash; hidden when there is no text.
// M6: it stamps in again whenever the seed changes.
import { motion } from "framer-motion";
import { stamp, useMotionPrefs } from "../motion";

export function Postmark({ hash }: { hash: string }) {
  const { reduce } = useMotionPrefs();
  if (!hash) return null;
  return (
    <motion.span key={hash} className="postmark" role="img" aria-label={`Seed ${hash}`} data-stamp={hash} {...stamp(reduce)}>
      {hash}
    </motion.span>
  );
}
