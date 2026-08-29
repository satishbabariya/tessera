#!/usr/bin/env bash
# Guards the Apache 2.0 section 4(b) obligation.
#
# Derivative works must retain the original copyright, patent, trademark and
# attribution notices. Tessera inherits ~560 files carrying "Copyright ... Realm
# Inc." and they must survive every mechanical transformation applied to this
# tree -- a rename, a sed, a formatting pass.
#
# Usage:
#   tools/check-copyright-notices.sh            # print the current count
#   tools/check-copyright-notices.sh <expected> # fail unless the count matches
#
# Run it before and after any tree-wide edit with the before-count as argument.
set -euo pipefail
COUNT=$(grep -rl "Copyright.*Realm Inc" src test 2>/dev/null | wc -l | tr -d ' ')

if [ $# -eq 0 ]; then
    echo "$COUNT files retain a Realm Inc. copyright notice"
    exit 0
fi

if [ "$COUNT" != "$1" ]; then
    echo "FAIL: copyright notice count changed: expected $1, found $COUNT"
    echo "  Apache 2.0 section 4(b) requires retaining these notices."
    echo "  A transformation that removes them is a licence violation, not a bug"
    echo "  to fix forward: revert it."
    exit 1
fi
echo "PASS: $COUNT copyright notices intact"
