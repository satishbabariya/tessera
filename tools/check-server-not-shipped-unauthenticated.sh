#!/usr/bin/env bash
# Fails if the sync server becomes installable while it authenticates nobody.
#
# This was written when the second half was simply true: the server accepted a
# signed_user_token on every bind, logged it, and never referred to it again.
# verify_access_token and AccessControl::can were never called and the public key
# handed to the Server constructor was never read. See
# docs/findings/0b-server-has-no-auth.md.
#
# That is no longer the state of the code. The server verifies the token on the
# WebSocket handshake, answers 401 when it is missing, unverifiable or expired,
# requires Download at BIND and Upload at UPLOAD, and re-checks expiry at both.
# So this check now passes on its first condition and does not constrain
# installability.
#
# It is kept, rather than deleted, because what it guards is an ordering and not
# a one-time state: the mechanical half of shipping a server is the tempting
# half, and if the authentication were ever removed or refactored away, this is
# what would notice before the package shipped. A check that currently passes is
# not the same as a check that cannot fail. Canary-tested in three parts, and
# the first result corrected a wrong sentence that stood here: removing the two
# calls from server.cpp on its own does NOT fail, because the conjunction is
# still unsatisfied and an unreachable server that authenticates nobody is the
# safe pairing. It takes both halves:
#
#     A. auth calls removed only ................ PASS (safe pairing)
#     B. auth removed AND an install rule added . FAIL
#     C. install rule with auth restored ........ PASS (installability permitted)
#
# It is a conjunction, deliberately. Either half alone is fine: a server that
# authenticates nothing and cannot be reached is what existed before, and a
# server that authenticates properly may be installed freely. Only the
# combination is refused.
set -euo pipefail
cd "$(dirname "$0")/.."

SERVER=src/tessera/sync/noinst/server/server.cpp
[ -f "$SERVER" ] || { echo "FAIL: $SERVER not found; this check cannot see what it claims to"; exit 1; }

# Does the server consult a token for any decision? Comments and the accessor
# that nobody calls do not count, so look for a call: an identifier followed by
# an open parenthesis, outside a comment.
authenticates=0
if grep -nE '^[^/]*\b(verify_access_token|can)\s*\(' "$SERVER" > /dev/null 2>&1; then
    authenticates=1
fi

# Is the server reachable from outside a build tree? Two independent ways it
# could become so.
installable=0
reasons=""
if grep -rn "install(TARGETS[^)]*SyncServer" --include=CMakeLists.txt src > /dev/null 2>&1; then
    installable=1; reasons="$reasons\n    SyncServer has an install(TARGETS) rule"
fi
if grep -rn "add_executable" src/tessera/sync/noinst/server/CMakeLists.txt > /dev/null 2>&1; then
    installable=1; reasons="$reasons\n    the server directory declares an executable"
fi

if [ "$installable" -eq 1 ] && [ "$authenticates" -eq 0 ]; then
    echo "FAIL: the sync server is becoming reachable and still authenticates nobody"
    printf '%b\n' "$reasons"
    echo "    server.cpp calls neither verify_access_token nor AccessControl::can."
    echo "    Add authentication before making the server installable, not after."
    echo "    See docs/findings/0b-server-has-no-auth.md."
    exit 1
fi

if [ "$authenticates" -eq 1 ]; then
    echo "PASS: the sync server consults a token; this check no longer constrains installability"
else
    echo "PASS: the sync server authenticates nobody and is not installable, which is the safe pairing"
fi
