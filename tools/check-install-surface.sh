#!/usr/bin/env bash
# Fails when the set of installed headers differs from tools/install-surface.txt,
# and names every header that appeared or disappeared.
#
# Every installed header is a promise. The package shipped 243 of them, of which
# 165 are reachable from the documented entry points of the five exported
# targets; thirteen of the rest were implementation headers under a directory
# called impl that nothing installed included, and they are gone. Sixty-five
# remain unadjudicated -- util, bson, sync configuration, merge internals -- and
# this check makes no claim about them.
#
# What it does is make the set deliberate. The thirteen arrived because one
# CMake list served both add_library and the install rule, so a header added for
# the build became a header shipped to consumers, silently. A manifest that has
# to be edited by hand turns that into a question somebody answers, and the diff
# of that manifest is the answer, reviewable.
#
# This replaced a pair of expected counts. The counts caught the right changes
# and said nothing useful about them: a failure reading "expected 231, found
# 230" gives no way to learn which header, so diagnosis needs a push, a CI run,
# and a guess -- five of those, in the event. A manifest costs one line per
# header and names it on the spot.
#
# Usage: tools/check-install-surface.sh <install-prefix>
#
# It needs an installed tree, so it is not one of the invariant checks that run
# before configuring; the consumer smoke test already builds one, and this runs
# beside it.
set -euo pipefail

# Sort and compare under one collation. The manifest is sorted with LC_ALL=C,
# and `comm` validates its input's order using the ambient locale: under
# en_US.UTF-8 punctuation collates differently, so "version.hpp" and
# "version_id.hpp" swap places, comm decides the input is unsorted, and it
# reports headers as both present and absent. A canary caught that -- it removed
# one header and the check blamed two others.
export LC_ALL=C

MANIFEST="$(dirname "$0")/install-surface.txt"

PREFIX="${1:-}"
[ -n "$PREFIX" ] || { echo "usage: $0 <install-prefix>" >&2; exit 2; }
[ -d "$PREFIX/include" ] || { echo "SKIP: $PREFIX/include does not exist"; exit 0; }
[ -f "$MANIFEST" ] || { echo "FAIL: no manifest at $MANIFEST" >&2; exit 2; }

# The manifest marks platform-specific headers with an "apple:" prefix rather
# than keeping two lists, so a header added to the common set cannot be added to
# only one platform's expectations -- which is how the first version of this
# check failed, its numbers recorded on macOS and asserted on Linux.
#
# Nothing carries that prefix today. Two headers appended under `if(APPLE)` did:
# they were reachable only from sync/impl/sync_client.hpp, an internal header
# that no longer ships, so the macOS and Linux packages now offer the same API.
# The branch stays because the next divergence should land in the manifest and
# not in a second list.
if [ "$(uname -s)" = "Darwin" ]; then
    expected=$(grep -v '^#' "$MANIFEST" | sed 's|^apple:||' | sort)
else
    expected=$(grep -v '^#' "$MANIFEST" | grep -v '^apple:' | sort)
fi

actual=$(cd "$PREFIX/include" && find . \( -name '*.hpp' -o -name '*.h' \) \
    | sed 's|^\./||' | sort)

unexpected=$(comm -13 <(printf '%s\n' "$expected") <(printf '%s\n' "$actual"))
missing=$(comm -23 <(printf '%s\n' "$expected") <(printf '%s\n' "$actual"))

if [ -n "$unexpected" ] || [ -n "$missing" ]; then
    echo "FAIL: the installed header surface differs from the manifest"
    if [ -n "$unexpected" ]; then
        echo
        echo "    These install but are not in the manifest:"
        printf '%s\n' "$unexpected" | sed 's|^|      + |'
    fi
    if [ -n "$missing" ]; then
        echo
        echo "    These are in the manifest but did not install:"
        printf '%s\n' "$missing" | sed 's|^|      - |'
    fi
    cat <<'HINT'

    If a header should now ship, add it to tools/install-surface.txt (with an
    "apple:" prefix if it only installs under `if(APPLE)`) and add a CHANGELOG
    entry saying what moved and why. If it should not ship, it is probably in a
    CMake list that feeds both add_library and an install rule, which is how
    thirteen implementation headers came to be published.
HINT
    exit 1
fi

count=$(printf '%s\n' "$expected" | grep -c .)
echo "PASS: the $count installed headers on $(uname -s) are exactly the manifest"
