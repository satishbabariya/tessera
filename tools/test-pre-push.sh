#!/usr/bin/env bash
# Exercises tools/pre-push, which git feeds on stdin as
#     <local ref> <local sha> <remote ref> <remote sha>
#
# The hook this replaces was never executed once in its life -- nothing
# installed it, nothing referenced it, and it had rotted into a state where it
# rejected this repository's own remote. This file is the difference between a
# hook that is believed to work and one that has been shown to.
set -euo pipefail
cd "$(dirname "$0")/.."

A=1111111111111111111111111111111111111111
B=2222222222222222222222222222222222222222
Z=0000000000000000000000000000000000000000

run() { echo "$1" | ./tools/pre-push origin https://github.com/satishbabariya/tessera >/dev/null 2>&1; }

check() {
    local what="$1" line="$2" want="$3" got=0
    run "$line" || got=$?
    if [[ "$got" != "$want" ]]; then
        echo "FAIL $what: expected exit $want, got $got" >&2
        return 1
    fi
    printf '  ok  %s\n' "$what"
}

failures=0
echo "pre-push:"
check "a feature branch pushes"          "refs/heads/f $A refs/heads/f $B"        0 || failures=1
check "main is refused"                  "refs/heads/main $A refs/heads/main $B"  1 || failures=1
check "deleting main is refused"         "refs/heads/main $Z refs/heads/main $B"  1 || failures=1
check "a tag pushes"                     "refs/tags/v1 $A refs/tags/v1 $Z"        0 || failures=1
check "a branch merely named main-ish"   "refs/heads/mainline $A refs/heads/mainline $B" 0 || failures=1

# Pushing several refs at once must still catch main among them.
multi=$'refs/heads/f '"$A"' refs/heads/f '"$B"$'\nrefs/heads/main '"$A"' refs/heads/main '"$B"
got=0; echo "$multi" | ./tools/pre-push origin url >/dev/null 2>&1 || got=$?
if [[ "$got" == 1 ]]; then echo "  ok  main is caught among several refs"
else echo "FAIL main not caught among several refs (exit $got)" >&2; failures=1; fi

got=0
echo "refs/heads/main $A refs/heads/main $B" \
  | TESSERA_ALLOW_MAIN_PUSH=1 ./tools/pre-push origin url >/dev/null 2>&1 || got=$?
if [[ "$got" == 0 ]]; then echo "  ok  the override works"
else echo "FAIL override did not permit the push (exit $got)" >&2; failures=1; fi

# The defect the old hook had: it rejected this repository's own remote, so a
# push to an ordinary branch failed. That is what the first assertion covers,
# and this states it in the terms of the bug.
if run "refs/heads/some-branch $A refs/heads/some-branch $B"; then
    echo "  ok  this repository's own remote is not rejected"
else
    echo "FAIL the hook rejects pushes to this repository, as the old one did" >&2
    failures=1
fi

exit "$failures"
