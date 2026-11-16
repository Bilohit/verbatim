// Round red stamp with the first 8 hex digits of the lane-seed hash; hidden when there is no text.
// M6: it stamps in again whenever the seed changes. E2: it shakes once when input is refused
// (reduced motion: it turns ink-black for 1 s instead).
import { motion, useAnimate } from "framer-motion";
import { useEffect, useState } from "react";
import { stamp, useMotionPrefs } from "../motion";

export function Postmark({ hash, shake = 0 }: { hash: string; shake?: number }) {
  const { reduce } = useMotionPrefs();
  const [scope, animate] = useAnimate();
  const [alert, setAlert] = useState(false);
  useEffect(() => {
    if (shake === 0 || !scope.current) return;
    setAlert(true);
    if (reduce) {
      const t = window.setTimeout(() => setAlert(false), 1000);
      return () => window.clearTimeout(t);
    }
    animate(scope.current, { x: [0, -5, 5, -3, 3, 0] }, { duration: 0.3 }).then(() => setAlert(false));
  }, [shake, reduce, animate, scope]);
  if (!hash) return null;
  return (
    <motion.span
      ref={scope}
      key={hash}
      className="postmark"
      role="img"
      aria-label={`Seed ${hash}`}
      data-stamp={hash}
      data-shake={alert ? "true" : undefined}
      data-alert={alert && reduce ? "true" : undefined}
      {...stamp(reduce)}
    >
      {hash}
    </motion.span>
  );
}
