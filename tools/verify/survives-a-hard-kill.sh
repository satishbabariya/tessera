#!/usr/bin/env bash
# Checks that the server loses nothing when it is killed rather than stopped.
#
# The engine has crash-safety tests and CI asserts they ran. None of that covers
# the server: whether a SIGKILL mid-life leaves a directory it can reopen, and
# whether the rows a client was told were committed are still there afterwards.
#
# A clean shutdown proves much less. tessera-sync-server stops on SIGTERM by
# waiting in sigwait and calling Server::stop(), which flushes; SIGKILL gives it
# no such chance, and that is the case a machine losing power resembles.
#
# Usage: tools/verify/survives-a-hard-kill.sh [build-dir]
set -euo pipefail
BUILD="${1:-build.ossl}"
cd "$(dirname "$0")/../.."

pick() { [ -x "$1" ] && echo "$1" || echo "$2"; }
SRV=$(pick "$BUILD/src/tessera/sync/server/tessera-sync-server" \
           "$BUILD/src/tessera/sync/server/tessera-sync-server-dbg")
TK=$(pick "$BUILD/src/tessera/sync/server/tessera-token" \
          "$BUILD/src/tessera/sync/server/tessera-token-dbg")
LT=$(pick "$BUILD/test/benchmark-sync/tessera-load-test" \
          "$BUILD/test/benchmark-sync/tessera-load-test-dbg")

for b in "$SRV" "$TK" "$LT"; do
    [ -x "$b" ] || { echo "SKIP: $b not built"; exit 0; }
done
command -v openssl > /dev/null || { echo "SKIP: openssl not on PATH"; exit 0; }

# The load test must accept the flags this script passes it. When it does not,
# it prints its usage and exits non-zero, and the script that invoked it reports
# the property as broken -- which is a lie about the code under test. A
# build.release left over from before those flags existed produced exactly that:
# "FAIL: the deployed path does not hold", followed by a usage message, for a
# server that was behaving correctly.
#
# So the flags are checked first, against the binary's own usage text, and a
# mismatch says which flag and that the binary is stale rather than blaming a
# property that was never exercised.
require_flags() {
    local help
    help=$("$LT" --help 2>&1 || true)
    local f
    for f in "$@"; do
        case "$help" in
            *"$f"*) ;;
            *)  echo "FAIL: $LT does not accept $f"
                echo "  This script needs it. The binary predates the flag -- rebuild the"
                echo "  build directory rather than trusting what follows; a load test that"
                echo "  rejects its arguments makes every property below look broken."
                exit 1 ;;
        esac
    done
}
require_flags --converge --key-base

WORK=$(mktemp -d)
trap 'pkill -f "$(basename "$SRV")" 2> /dev/null || true; rm -rf "$WORK"' EXIT

mkdir -p "$WORK/srv"
openssl genrsa -out "$WORK/private.pem" 2048 2> /dev/null
openssl rsa -in "$WORK/private.pem" -pubout -out "$WORK/public.pem" 2> /dev/null
TOKEN=$("$TK" --key "$WORK/private.pem" --identity crash --expires-in 7200)

# Client invocations are bounded. A sync client that cannot get what it needs
# does not fail -- it retries, by design, because the server it wants may be
# coming back. Pointed at a server whose directory has been wiped, the client
# hangs rather than erroring, and an unbounded run turns a failed assertion into
# a job timeout, which names nothing.
run_bounded() {
    local secs="$1" log="$2"; shift 2
    "$@" > "$log" 2>&1 &
    local pid=$!
    ( sleep "$secs"; kill -9 "$pid" 2> /dev/null ) &
    local watchdog=$! status=0
    wait "$pid" || status=$?
    kill "$watchdog" 2> /dev/null || true
    wait "$watchdog" 2> /dev/null || true
    return "$status"
}

start_server() {
    "$SRV" --root "$WORK/srv" --public-key "$WORK/public.pem" --port 0 > "$1" 2>&1 &
    SERVER_PID=$!
    PORT=""
    for _ in $(seq 1 40); do
        PORT=$(grep -oE 'Listening on localhost:[0-9]+' "$1" 2> /dev/null \
               | head -1 | grep -oE '[0-9]+$' || true)
        [ -n "$PORT" ] && return 0
        sleep 0.5
    done
    echo "FAIL: the server never reported a port"; cat "$1"; exit 1
}

WROTE=200
start_server "$WORK/first.log"
dir=$(mktemp -d)
run_bounded 90 "$dir/out" \
    "$LT" --root "$dir" --port "$PORT" --clients 4 --transactions 50 --token "$TOKEN" || {
    echo "FAIL: could not write the first $WROTE rows"; tail -5 "$dir/out"; exit 1
}
rm -rf "$dir"

# Not SIGTERM. The point is the path that gets no chance to flush.
kill -9 "$SERVER_PID"
wait "$SERVER_PID" 2> /dev/null || true
sleep 1

start_server "$WORK/second.log"
if grep -qiE 'corrupt|cannot open|failed to open' "$WORK/second.log"; then
    echo "FAIL: the server would not reopen its directory after a hard kill"
    grep -iE 'corrupt|cannot open|failed to open' "$WORK/second.log" | head -3
    exit 1
fi

# A key base well clear of the first run, so this inserts rather than rewriting
# what is being checked for.
dir=$(mktemp -d)
run_bounded 90 "$dir/out" \
    "$LT" --root "$dir" --port "$PORT" --clients 1 --transactions 1 --token "$TOKEN" \
    --key-base 999000000 --converge || {
    echo "FAIL: could not reconnect after the restart"; tail -5 "$dir/out"; exit 1
}
SEEN=$(grep -oE 'every client sees [0-9]+ rows' "$dir/out" | grep -oE '[0-9]+' | head -1 || echo 0)
rm -rf "$dir"

EXPECTED=$((WROTE + 1))
if [ "${SEEN:-0}" -lt "$EXPECTED" ]; then
    echo "FAIL: $SEEN rows survived the kill, expected at least $EXPECTED"
    exit 1
fi

echo "PASS: $SEEN rows survived SIGKILL and the server reopened its directory"
