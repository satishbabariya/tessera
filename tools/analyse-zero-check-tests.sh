#!/usr/bin/env bash
# Lists tests that run and execute no checks.
#
# This is analysis, not a gate, and is deliberately not wired into CI. Most
# zero-check tests are legitimate: a large family of regression tests here assert
# by not crashing -- Query_DeepCopyLeak1, Table_DeleteCrash, Shared_StringIndexBug2
# and about a hundred others reproduce a sequence that once corrupted memory, and
# their assertion is the absence of a crash. There is nothing to count.
#
# What the list is for is the other kind: a test that returns early because the
# platform lacks something it needs, and reports success over nothing. The
# framework has TEST_IF for exactly that case, which reports the test as
# excluded; an early return reports it as passed.
#
# Usage: tools/analyse-zero-check-tests.sh [test-binary]
#
# It runs each test in its own process, so it takes minutes rather than seconds.
# That is the price of a per-test check count: the summary line is per-run, and
# a run of the whole suite reports one total.
set -uo pipefail
cd "$(dirname "$0")/.."

BIN="${1:-}"
if [ -z "$BIN" ]; then
    BIN=$(find build.debug/test -name 'tessera-tests' -type f 2>/dev/null | head -1)
fi
[ -x "$BIN" ] || { echo "usage: $0 <path-to-tessera-tests>"; exit 1; }

NAMES=$(grep -hoE '^[[:space:]]*(NONCONCURRENT_)?TEST(_IF)?\([[:space:]]*[A-Za-z_][A-Za-z0-9_]*' \
        test/*.cpp | sed -E 's/.*\([[:space:]]*//' | sort -u)

run_one() {
    local name="$1" tmp out tests checks
    tmp=$(mktemp -d)
    out=$(TMPDIR="$tmp/" UNITTEST_FILTER="$name" UNITTEST_THREADS=1 "$BIN" 2>&1 \
          | grep -E "tests passed|tests failed")
    rm -rf "$tmp"
    tests=$(echo "$out" | sed -nE 's/.*All ([0-9]+) tests passed.*/\1/p')
    checks=$(echo "$out" | sed -nE 's/.*passed \(([0-9]+) checks?\).*/\1/p')
    # A test absent from this binary reports 0 tests; that is not a finding, it
    # lives in the sync or object-store suite.
    [ -n "$tests" ] && [ "$tests" != "0" ] && [ "$checks" = "0" ] && echo "$name"
    return 0
}
export -f run_one
export BIN

echo "Running each test alone. This takes a few minutes."
echo "$NAMES" | xargs -P 8 -I{} bash -c 'run_one "$@"' _ {} | sort
