#!/usr/bin/env python3
"""Fail when recorded verification evidence no longer matches the sources it describes.

docs/evidence/source-sha256.json pins every authored source, test and build file. Rerun the
commands in docs/verification.md and refresh docs/evidence/*.txt before using --write.
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs/evidence/source-sha256.json"
PATTERNS = ("Ina229/**/*", "tests/**/*", "examples/**/*", "library.cmake", ".clang-tidy",
            ".github/workflows/ci.yml")


def sources():
    files = set()
    for pattern in PATTERNS:
        for path in ROOT.glob(pattern):
            parts = path.relative_to(ROOT).parts
            if (path.is_file() and path.suffix != ".pyc" and path.name != ".DS_Store"
                    and "__pycache__" not in parts and not any(p.startswith("build") for p in parts)):
                files.add(path.relative_to(ROOT).as_posix())
    return sorted(files)


def digest(relative):
    return hashlib.sha256((ROOT / relative).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write", action="store_true", help="re-pin the manifest to the working tree")
    args = parser.parse_args()
    current = {relative: digest(relative) for relative in sources()}
    if args.write:
        MANIFEST.write_text(json.dumps(current, indent=2) + "\n", encoding="utf-8")
        print(f"pinned {len(current)} files in {MANIFEST.relative_to(ROOT)}")
        return 0
    recorded = json.loads(MANIFEST.read_text(encoding="utf-8"))
    problems = []
    for relative in sorted(set(recorded) | set(current)):
        if relative not in current:
            problems.append(f"missing: {relative}")
        elif relative not in recorded:
            problems.append(f"not pinned: {relative}")
        elif recorded[relative] != current[relative]:
            problems.append(f"changed since evidence was recorded: {relative}")
    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        print("Rerun the verification commands, refresh docs/evidence, then use --write.", file=sys.stderr)
        return 1
    print(f"evidence manifest matches {len(current)} files")
    return 0


if __name__ == "__main__":
    sys.exit(main())
