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


def audit_text(text, name):
    problems, lines, i = [], text.split("\n"), 0
    while i < len(lines):
        if lines[i].rstrip().endswith(MARKER) and "(" in lines[i]:
            start, depth, body_started, j = i, 0, False, i
            while j < len(lines):
                code = strip_comments(lines[j])
                for ch in code:
                    if ch == "{":
                        depth += 1
                        body_started = True
                    elif ch == "}":
                        depth -= 1
                if body_started and j > start and FORBIDDEN.search(code):
                    problems.append(f"{name}:{j + 1}: {lines[j].strip()}")
                if body_started and depth == 0:
                    break
                j += 1
            i = j
        i += 1
    return problems


def audit(root):
    problems, marked = [], 0
    for dirpath, _, files in os.walk(root):
        for f in files:
            if f.endswith((".cpp", ".h")):
                path = os.path.join(dirpath, f)
                text = open(path, encoding="utf-8").read()
                marked += text.count(MARKER)
                problems += audit_text(text, path)
    return problems, marked


def self_test():
    good = "void A::process (float* x, int n) // audio-thread\n{\n    for (int i = 0; i < n; ++i) x[i] = 0; // no new here\n}\n"
    bad = "void A::process (float* x, int n) // audio-thread\n{\n    v.push_back (1);\n}\nvoid A::prepare()\n{\n    v.push_back (2);\n}\n"
    assert audit_text(good, "good") == [], "comment mentioning new must not count"
    found = audit_text(bad, "bad")
    assert len(found) == 1 and "bad:3" in found[0], found
    print("lock_audit self-test passed")
    return 0


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    if sys.argv[1] == "--self-test":
        return self_test()
    problems, marked = audit(sys.argv[1])
    for p in problems:
        print("RT3:", p)
    print(f"lock_audit: {marked} audio-thread functions checked, {len(problems)} problems")
    return 1 if problems or marked == 0 else 0


if __name__ == "__main__":
    sys.exit(main())
