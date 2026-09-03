#!/usr/bin/env bash
# Fails when the installed header count changes without this number changing
# with it.
#
# Every installed header is a promise. The package shipped 243 of them, of which
# 165 are reachable from the documented entry points of the five exported
# targets; thirteen of the rest were implementation headers under a directory
# called impl that nothing installed included, and they are gone. Sixty-five
# remain unadjudicated -- util, bson, sync configuration, merge internals -- and
# this check makes no claim about them.
#
# What it does is make the number deliberate. The thirteen arrived because one
# CMake list served both add_library and the install rule, so a header added for
# the build became a header shipped to consumers, silently. A count that has to
# be edited by hand turns that into a question somebody answers.
#
# Usage: tools/check-install-surface.sh <install-prefix>
#
# It needs an installed tree, so it is not one of the invariant checks that run
# before configuring; the consumer smoke test already builds one, and this runs
# beside it.
set -euo pipefail

# Update these together with a note in CHANGELOG.md saying what moved and why.
#
# The count is platform-dependent, which this check discovered on its first CI
# run by failing on Linux and passing on macOS. Two headers are appended under
# `if(APPLE)` in src/tessera/object-store/CMakeLists.txt --
# sync/impl/apple/network_reachability_observer.hpp and
# sync/impl/apple/system_configuration.hpp -- and both are included by an
# installed header, so they are not removable: the Apple package genuinely has a
# surface the Linux one does not.
#
# That is worth knowing on its own. Code that compiles against the macOS package
# may not compile against the Linux one.
case "$(uname -s)" in
    Darwin) EXPECTED_HPP=231 ;;
    *)      EXPECTED_HPP=229 ;;
esac
EXPECTED_H=4

PREFIX="${1:-}"
[ -n "$PREFIX" ] || { echo "usage: $0 <install-prefix>" >&2; exit 2; }
[ -d "$PREFIX/include" ] || { echo "SKIP: $PREFIX/include does not exist"; exit 0; }

hpp=$(find "$PREFIX/include" -name '*.hpp' | wc -l | tr -d ' ')
h=$(find "$PREFIX/include" -name '*.h' | wc -l | tr -d ' ')

if [ "$hpp" != "$EXPECTED_HPP" ] || [ "$h" != "$EXPECTED_H" ]; then
    echo "FAIL: the installed header surface changed"
    printf '    .hpp  expected %s, found %s\n' "$EXPECTED_HPP" "$hpp"
    printf '    .h    expected %s, found %s\n' "$EXPECTED_H" "$h"
    echo
    echo "    If a header should now ship, say so: update EXPECTED_HPP/EXPECTED_H in"
    echo "    $0 and add a CHANGELOG entry. If it should not, it is probably in a"
    echo "    CMake list that feeds both add_library and an install rule, which is"
    echo "    how thirteen implementation headers came to be published."
    exit 1
fi

echo "PASS: $hpp .hpp and $h .h headers install on $(uname -s), as recorded"
