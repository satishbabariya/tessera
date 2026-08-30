#!/usr/bin/env bash
# Fails if the sync server becomes installable while it still authenticates
# nobody.
#
# The server accepts a signed_user_token on every bind, logs it, and never refers
# to it again: verify_access_token and AccessControl::can are never called, and
# the public key handed to the Server constructor is never read. See
# docs/findings/0b-server-has-no-auth.md.
#
# That is safe only because the server cannot be reached. It is not in the
# installed package -- no Tessera::SyncServer target, no installed header, no
# executable -- so it runs only inside a build tree, which is where the 461 sync
# tests run it.
#
# Making it installable is mechanical: 36 include sites and one header that is
# not yet installed. Adding authentication is not. This check exists so the two
# cannot be done in the wrong order by someone who has not read the finding,
# because the mechanical half is the tempting one and doing it first would ship a
# reachable server that authenticates nobody.
#
# It is a conjunction, deliberately. Either half alone is fine: a server that
# authenticates nothing and cannot be reached is what exists today, and a server
# that authenticates properly may be installed freely. Only the combination is
# refused.
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
