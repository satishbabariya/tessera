#!/usr/bin/env bash
# Fails if a header decides something on a TESSERA_ macro that only a .cpp
# defines.
#
# A macro defined in a .cpp and tested in a .hpp does not mean what it looks
# like it means. The header is parsed by every translation unit that includes it,
# and almost none of them have seen the definition, so the #ifdef is false for
# them. It is false even in the defining .cpp when the include comes first, which
# is the ordering that produced this check.
#
# RobustMutex::is_robust_on_this_platform was declared in thread.hpp under
# `#ifdef TESSERA_HAVE_ROBUST_PTHREAD_MUTEX`, and that macro was defined in
# thread.cpp fourteen lines after thread.cpp includes thread.hpp. The constant
# was therefore false in every translation unit in the project, including
# thread.cpp's own, while the implementation further down that same file
# compiled full robust-mutex support because its #ifdef came after the defines.
#
# A public constant that contradicted its own implementation, on every platform,
# with five tests gating on it and none of them running. See
# docs/findings/0b-header-macro-visibility.md.
set -euo pipefail
cd "$(dirname "$0")/.."

python3 - <<'PY'
import re, pathlib, collections, sys

files = [p for p in pathlib.Path('src/tessera').rglob('*')
         if p.suffix in ('.cpp', '.hpp', '.h', '.mm') and 'external' not in p.parts]

defined_in = collections.defaultdict(set)
tested_in = collections.defaultdict(set)
for p in files:
    t = p.read_text(errors='ignore')
    for m in re.finditer(r'^\s*#\s*define\s+(TESSERA_[A-Z0-9_]+)', t, re.M):
        defined_in[m.group(1)].add(p)
    for m in re.finditer(r'^\s*#\s*(?:ifdef|ifndef|if\s+defined\s*\(?)\s*(TESSERA_[A-Z0-9_]+)', t, re.M):
        tested_in[m.group(1)].add(p)

if not files:
    print("FAIL: no sources found; this check would pass over an empty set")
    sys.exit(1)

bad = []
for macro, testers in sorted(tested_in.items()):
    definers = defined_in.get(macro)
    if not definers:
        continue  # supplied by the build or by config.h, which is installed
    headers = sorted(p for p in testers if p.suffix in ('.hpp', '.h'))
    if headers and all(p.suffix in ('.cpp', '.mm') for p in definers):
        bad.append((macro, sorted(definers), headers))

for macro, definers, headers in bad:
    print(f"FAIL: {macro} is tested in a header but defined only in a source file")
    print(f"      defined in: {', '.join(str(p) for p in definers)}")
    print(f"      tested in : {', '.join(str(p) for p in headers)}")
    print("      Every translation unit that includes the header without the")
    print("      definition takes the other branch, silently.")

if bad:
    sys.exit(1)
print(f"PASS: no header decides on a macro that only a source file defines "
      f"({len(tested_in)} macros checked)")
PY
