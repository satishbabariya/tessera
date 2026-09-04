#!/usr/bin/env bash
# Fails when an installed header is neither a declared entry point nor reachable
# from one, and when a declared entry point is not self-contained.
#
# tools/install-surface.txt records what ships. This records why. A manifest
# stops the surface changing by accident; it says nothing about whether the
# surface makes sense, and 243 headers shipped for years of which a consumer
# could reach 160.
#
# The other eighty-three were not reachable from anything a consumer was told to
# include. Some were internal by directory name -- impl/, noinst/ -- and went
# earlier. The rest were utilities: base64, uri, compression, sha_crypto,
# hex_dump, a priority queue. Each was a promise, and each could only have been
# discovered by listing the include directory, because no public header
# mentioned it.
#
# Compiling each entry point alone also proves it is self-contained, which the
# consumer smoke test previously checked for two of them.
#
# Usage: tools/check-surface-is-reachable.sh <install-prefix> [compiler]
set -euo pipefail
export LC_ALL=C

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENTRIES="$ROOT/tools/api-entry-points.txt"

PREFIX="${1:-}"
CXX_BIN="${2:-${CXX:-c++}}"
[ -n "$PREFIX" ] || { echo "usage: $0 <install-prefix> [compiler]" >&2; exit 2; }
[ -d "$PREFIX/include" ] || { echo "SKIP: $PREFIX/include does not exist"; exit 0; }
[ -f "$ENTRIES" ] || { echo "FAIL: no entry point list at $ENTRIES" >&2; exit 2; }

INC="$PREFIX/include"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

declared=$(grep -v '^#' "$ENTRIES" | grep -v '^[[:space:]]*$' | sort -u)

# Each entry point is compiled on its own. -H prints the include tree to stderr,
# one line per header with a depth prefix; the paths under $INC are what this
# entry point reaches.
: > "$WORK/reached"
notselfcontained=""
missing=""
while IFS= read -r h; do
    [ -n "$h" ] || continue
    if [ ! -f "$INC/$h" ]; then
        missing="$missing $h"
        continue
    fi
    printf '#include <%s>\nint main() { return 0; }\n' "$h" > "$WORK/one.cpp"
    if "$CXX_BIN" -std=c++20 -fsyntax-only -H -I"$INC" "$WORK/one.cpp" \
         > /dev/null 2> "$WORK/one.err"; then
        grep -oE "$INC/[^ ]+" "$WORK/one.err" | sed "s|$INC/||" >> "$WORK/reached"
    else
        notselfcontained="$notselfcontained $h"
        # Keep going: one broken entry point should not hide the rest.
        grep -oE "$INC/[^ ]+" "$WORK/one.err" | sed "s|$INC/||" >> "$WORK/reached" || true
    fi
done <<< "$declared"

if [ -n "$missing" ]; then
    echo "FAIL: declared entry points that do not install:"
    for h in $missing; do printf '      %s\n' "$h"; done
    echo
    echo "    Either the header should ship -- add it to tools/install-surface.txt and"
    echo "    the CMake list that installs its directory -- or it is not an entry point"
    echo "    and belongs out of tools/api-entry-points.txt."
    exit 1
fi

if [ -n "$notselfcontained" ]; then
    echo "FAIL: declared entry points that do not compile on their own:"
    for h in $notselfcontained; do printf '      %s\n' "$h"; done
    echo
    echo "    A consumer includes an entry point first, or alone. One that needs another"
    echo "    header included before it is not an entry point yet -- add the include it"
    echo "    depends on to the header itself."
    exit 1
fi

printf '%s\n' "$declared" >> "$WORK/reached"
sort -u "$WORK/reached" -o "$WORK/reached"
(cd "$INC" && find . \( -name '*.hpp' -o -name '*.h' \) | sed 's|^\./||' | sort) > "$WORK/installed"

orphans=$(comm -23 "$WORK/installed" "$WORK/reached")
if [ -n "$orphans" ]; then
    echo "FAIL: installed headers that no consumer can reach:"
    printf '%s\n' "$orphans" | sed 's|^|      |'
    echo
    echo "    Each of these ships as public API, and no declared entry point includes"
    echo "    it, directly or transitively. A consumer could only find it by listing"
    echo "    the include directory."
    echo
    echo "    Either it is an entry point -- add it to tools/api-entry-points.txt, with"
    echo "    a CHANGELOG entry, because that widens the API -- or it should not"
    echo "    install: move it to the internal header list in its CMakeLists.txt."
    exit 1
fi

n_declared=$(printf '%s\n' "$declared" | grep -c .)
n_installed=$(grep -c . "$WORK/installed")
echo "PASS: all $n_installed installed headers are reachable from the $n_declared declared"
echo "      entry points, and every entry point compiles on its own"
