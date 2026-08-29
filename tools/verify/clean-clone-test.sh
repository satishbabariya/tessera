#!/usr/bin/env bash
# Verifies that a stranger can use this project: clone it and follow the README,
# with no local knowledge and no pre-existing working tree.
#
# This exists because every other gate in the project ran against a tree whose
# submodules were already checked out. The first genuine clean clone failed at
# configure, and nothing else could have caught it. The tests you write from
# inside a project cannot see the conditions of arriving at it from outside.
#
# Usage: tools/verify/clean-clone-test.sh [repo-url]
set -euo pipefail
REPO="${1:-https://github.com/satishbabariya/tessera}"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

# 1. The minimal path: a plain clone must produce a usable library.
git clone -q "$REPO" "$WORK/plain"
( cd "$WORK/plain" && cmake -B build -DCMAKE_BUILD_TYPE=Release > "$WORK/plain.log" 2>&1 ) \
  || { echo "FAIL: a plain clone does not configure"; tail -20 "$WORK/plain.log"; exit 1; }
grep -q "Tests are disabled" "$WORK/plain.log" \
  || echo "note: no submodule warning -- did the clone pick them up?"
( cd "$WORK/plain" && cmake --build build --target Storage -j"$(getconf _NPROCESSORS_ONLN)" > "$WORK/plain-build.log" 2>&1 ) \
  || { echo "FAIL: a plain clone does not build the library"; tail -20 "$WORK/plain-build.log"; exit 1; }
test -f "$WORK/plain/build/src/tessera/libtessera.a" \
  || { echo "FAIL: libtessera.a was not produced by a plain clone"; exit 1; }
echo "PASS: a plain clone builds the library"

# 2. The full path: --recursive must additionally enable the tests.
git clone -q --recursive "$REPO" "$WORK/full"
( cd "$WORK/full" && cmake -B build -DCMAKE_BUILD_TYPE=Release > "$WORK/full.log" 2>&1 ) \
  || { echo "FAIL: a recursive clone does not configure"; tail -20 "$WORK/full.log"; exit 1; }
grep -q "Tests are disabled" "$WORK/full.log" \
  && { echo "FAIL: tests disabled despite a recursive clone"; exit 1; }
echo "PASS: a recursive clone configures with tests enabled"
