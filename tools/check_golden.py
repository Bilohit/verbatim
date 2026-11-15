#!/usr/bin/env python3
"""G4 (docs/06-test-plan.md): the golden hash file is write-protected by process.

Fails if any commit in the given range modifies or deletes tests/golden/corpus.hashes without the
tag [grammar-change-approved] in its commit message. Adding the file (first creation) is allowed.

usage: check_golden.py <git-rev-range>     e.g. origin/main..HEAD
       check_golden.py --self-test
"""
import os
import subprocess
import sys
import tempfile

TAG = "[grammar-change-approved]"
HASHES = "tests/golden/corpus.hashes"


def git(args, cwd):
    return subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True, text=True).stdout


def violations(rev_range, cwd="."):
    bad = []
    for commit in git(["rev-list", rev_range], cwd).split():
        status = git(["diff-tree", "--root", "--no-commit-id", "--name-status", "-r", commit, "--", HASHES], cwd).strip()
        if status[:1] in ("M", "D") and TAG not in git(["log", "-1", "--format=%B", commit], cwd):
            bad.append(commit)
    return bad


def self_test():
    with tempfile.TemporaryDirectory() as d:
        def commit(content, message):
            path = os.path.join(d, HASHES)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, "w") as f:
                f.write(content)
            git(["add", "-A"], d)
            git(["-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "-m", message], d)
            return git(["rev-parse", "HEAD"], d).strip()

        git(["init", "-q"], d)
        commit("a\n", "add hashes")
        base = git(["rev-parse", "HEAD"], d).strip()
        assert violations(base + "..HEAD", d) == [], "creation must pass"
        assert violations("HEAD", d) == [], "creation (root commit) must pass"
        bad = commit("b\n", "change hashes")
        assert violations(base + "..HEAD", d) == [bad], "untagged change must fail"
        commit("c\n", "change hashes " + TAG)
        assert violations(bad + "..HEAD", d) == [], "tagged change must pass"
    print("check_golden self-test passed")
    return 0


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    if sys.argv[1] == "--self-test":
        return self_test()
    bad = violations(sys.argv[1])
    for c in bad:
        print(f"G4: {c} changes {HASHES} without {TAG}")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
