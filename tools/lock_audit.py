#!/usr/bin/env python3
"""RT3 (docs/06-test-plan.md): no locks or allocation in audio-thread code.

Scans dsp/ for function definitions whose signature line ends with the marker `// audio-thread` and fails if
the function body uses std::mutex, CriticalSection, std::lock_guard / unique_lock / scoped_lock, new, malloc or
push_back / emplace_back.

usage: lock_audit.py <dsp dir>          lock_audit.py --self-test
"""
import os
import re
import sys
import tempfile

MARKER = "// audio-thread"
FORBIDDEN = re.compile(r"std::mutex|CriticalSection|std::(lock_guard|unique_lock|scoped_lock)|\bnew\b|\bmalloc\b|\bpush_back\b|\bemplace_back\b")


def strip_comments(line):
    return re.sub(r"//.*$", "", line)


def audit_body(lines, i, name, problems):
    """Audit the body after the marker on line i. Returns False when a ';' comes first (a declaration)."""
    depth, body_started, j = 0, False, i
    while j < len(lines):
        code = strip_comments(lines[j])
        for ch in code:
            if not body_started and ch == ";":
                return False
            if ch == "{":
                depth += 1
                body_started = True
            elif ch == "}":
                depth -= 1
        if body_started and j > i and FORBIDDEN.search(code):
            problems.append(f"{name}:{j + 1}: {lines[j].strip()}")
        if body_started and depth == 0:
            return True
        j += 1
    return False


def audit_text(text, name, counts=None):
    """Problems in every marked function body. A marker on a comment-only line or a declaration is skipped."""
    problems, lines = [], text.split("\n")
    for i, line in enumerate(lines):
        if line.rstrip().endswith(MARKER) and strip_comments(line).strip():
            if audit_body(lines, i, name, problems) and counts is not None:
                counts.append(i + 1)
    return problems


def audit(root):
    problems, audited = [], []
    for dirpath, _, files in os.walk(root):
        for f in files:
            if f.endswith((".cpp", ".h")):
                path = os.path.join(dirpath, f)
                text = open(path, encoding="utf-8").read()
                problems += audit_text(text, path, audited)
    return problems, len(audited)


def self_test():
    good = "void A::process (float* x, int n) // audio-thread\n{\n    for (int i = 0; i < n; ++i) x[i] = 0; // no new here\n}\n"
    bad = "void A::process (float* x, int n) // audio-thread\n{\n    v.push_back (1);\n}\nvoid A::prepare()\n{\n    v.push_back (2);\n}\n"
    assert audit_text(good, "good") == [], "comment mentioning new must not count"
    found = audit_text(bad, "bad")
    assert len(found) == 1 and "bad:3" in found[0], found
    # Marker on a continuation line of a multi-line signature.
    cont = "int C::render (const float* a,\n               float* o) // audio-thread\n{\n    v.push_back (1);\n}\n"
    found = audit_text(cont, "cont")
    assert len(found) == 1 and "cont:4" in found[0], found
    # A marked declaration has no body to audit; the next function must not be blamed.
    decl = "    void process (float* x) noexcept; // audio-thread\nvoid other()\n{\n    v.push_back (1);\n}\n"
    assert audit_text(decl, "decl") == [], audit_text(decl, "decl")
    print("lock_audit self-test passed")
    return 0


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    if sys.argv[1] == "--self-test":
        return self_test()
    problems, audited = audit(sys.argv[1])
    for p in problems:
        print("RT3:", p)
    print(f"lock_audit: {audited} audio-thread function bodies checked, {len(problems)} problems")
    return 1 if problems or audited == 0 else 0


if __name__ == "__main__":
    sys.exit(main())
