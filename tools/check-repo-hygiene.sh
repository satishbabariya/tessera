#!/usr/bin/env bash
# Checks the repository's own surface, which no other check in this project
# looks at. Every other check inspects code -- layering, includes, exported
# symbols, installed headers. None of them notices a stray private key in the
# root, two documentation folders one letter apart, or a source file importing
# a module that was deleted.
#
# All of those were present after Phase 0b and none was detected by anything.
set -euo pipefail
FAIL=0

# 1. Runtime artifacts must not be committed. The engine writes coordination
#    files next to a database, and tests run from the repository root leave
#    them behind: .note is the notification FIFO, .cv files hold
#    condition-variable state.
STRAY=$(git ls-files | grep -E '(^|/)\.(note|new_commit\.cv|pick_writer\.cv)$|\.tess(\.lock|\.management)?$' || true)
if [ -n "$STRAY" ]; then
    echo "FAIL: runtime artifacts are committed:"; echo "$STRAY"; FAIL=1
fi

# 2. One documentation directory. "doc" and "docs" side by side is how a
#    project looks unmaintained, and a reader cannot guess which is current.
if [ -d doc ] && [ -d docs ]; then
    echo "FAIL: both doc/ and docs/ exist -- consolidate them"; FAIL=1
fi

# 3. Nothing may reference a target or module that no longer exists. src/swift
#    imported RealmFFI for an entire phase after that module was deleted,
#    because nothing built it and so nothing failed.
for dead in RealmFFI RealmFFIStatic; do
    HIT=$(git ls-files | xargs grep -l "$dead" 2>/dev/null | grep -v '^docs/' || true)
    if [ -n "$HIT" ]; then
        echo "FAIL: '$dead' was deleted but is still referenced by:"; echo "$HIT"; FAIL=1
    fi
done

# 4. Private keys do not belong in the repository root. The ones inherited here
#    were unused test keys, but a .pem in the root reads as a leak.
ROOTPEM=$(git ls-files --full-name | grep -E '^[^/]+\.pem$' || true)
if [ -n "$ROOTPEM" ]; then
    echo "FAIL: certificate or key files in the repository root:"; echo "$ROOTPEM"
    echo "  test fixtures belong beside the tests that use them"; FAIL=1
fi

[ "$FAIL" -eq 0 ] && echo "PASS: repository surface is clean"
exit "$FAIL"
