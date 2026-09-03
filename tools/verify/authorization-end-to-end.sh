#!/usr/bin/env bash
# Checks the authorization model through the shipped binaries, over a socket.
#
# The test suite covers all of this in-process, through fixtures. None of it had
# been exercised end to end -- a real server, a real token minted by
# tessera-token, a real client over a real socket -- until this script. That gap
# is the same one that hid every other finding in docs/findings: a thing verified
# in one configuration and never in the one people actually use.
#
# Three properties, each with its complement, because a server that refuses
# everything would satisfy the refusals alone:
#
#   a token scoped to /allowed          works on /allowed
#   the same token                      is refused on /denied
#   a download-only token               is refused when it uploads
#
# and then the same thing again over TLS, because --tls-cert was verified with an
# openssl s_client handshake, which proves the server speaks TLS and not that a
# sync client can complete a session over it.
#
# Usage: tools/verify/authorization-end-to-end.sh [build-dir]
set -euo pipefail
BUILD="${1:-build.ossl}"
cd "$(dirname "$0")/../.."

SRV="$BUILD/src/tessera/sync/server/tessera-sync-server"
[ -x "$SRV" ] || SRV="$BUILD/src/tessera/sync/server/tessera-sync-server-dbg"
TK="$BUILD/src/tessera/sync/server/tessera-token"
[ -x "$TK" ] || TK="$BUILD/src/tessera/sync/server/tessera-token-dbg"
LT="$BUILD/test/benchmark-sync/tessera-load-test"
[ -x "$LT" ] || LT="$BUILD/test/benchmark-sync/tessera-load-test-dbg"

for b in "$SRV" "$TK" "$LT"; do
    [ -x "$b" ] || { echo "SKIP: $b not built"; exit 0; }
done
command -v openssl > /dev/null || { echo "SKIP: openssl not on PATH"; exit 0; }

WORK=$(mktemp -d)
SERVER_PID=""
cleanup() {
    [ -n "$SERVER_PID" ] && kill "$SERVER_PID" 2> /dev/null || true
    rm -rf "$WORK"
}
trap cleanup EXIT

mkdir -p "$WORK/srv"
openssl genrsa -out "$WORK/private.pem" 2048 2> /dev/null
openssl rsa -in "$WORK/private.pem" -pubout -out "$WORK/public.pem" 2> /dev/null

SCOPED=$("$TK" --key "$WORK/private.pem" --identity scoped --path /allowed --expires-in 3600)
READONLY=$("$TK" --key "$WORK/private.pem" --identity readonly --access download --expires-in 3600)

# Port 0 lets the OS choose, so this cannot collide with anything else on the
# machine. The server prints what it got.
"$SRV" --root "$WORK/srv" --public-key "$WORK/public.pem" --port 0 --log-level debug \
    > "$WORK/srv.log" 2>&1 &
SERVER_PID=$!

for _ in $(seq 1 40); do
    PORT=$(grep -oE 'Listening on localhost:[0-9]+' "$WORK/srv.log" 2> /dev/null \
           | head -1 | grep -oE '[0-9]+$' || true)
    [ -n "${PORT:-}" ] && break
    sleep 0.5
done
[ -n "${PORT:-}" ] || { echo "FAIL: the server never reported a port"; cat "$WORK/srv.log"; exit 1; }

# The positive case runs to completion and is judged by its exit status: a
# permitted client finishes quickly and exits 0. Nothing is timed.
permitted() {
    local path="$1" token="$2" dir status=0
    dir=$(mktemp -d)
    "$LT" --root "$dir" --port "$PORT" --path "$path" --clients 1 --transactions 2 \
          --token "$token" > "$dir/out" 2>&1 || status=$?
    rm -rf "$dir"
    return "$status"
}

# A refused client retries forever, so it cannot be waited on. It is started, the
# log is polled for the rejection, and it is stopped as soon as that appears.
#
# Polling rather than sleeping is deliberate. A fixed sleep that is long enough
# on this laptop is not necessarily long enough on a loaded CI runner, and a gate
# that fails for reasons unrelated to the change is one everybody learns to
# ignore -- which is why SyncTests is already excluded from the merge gate over a
# DNS-dependent test.
refused_with() {
    local path="$1" token="$2" pattern="$3" dir pid found=1
    dir=$(mktemp -d)
    "$LT" --root "$dir" --port "$PORT" --path "$path" --clients 1 --transactions 2 \
          --token "$token" > /dev/null 2>&1 &
    pid=$!
    for _ in $(seq 1 60); do
        if grep -q "$pattern" "$WORK/srv.log"; then found=0; break; fi
        sleep 0.5
    done
    kill -9 "$pid" 2> /dev/null || true
    wait "$pid" 2> /dev/null || true
    rm -rf "$dir"
    return "$found"
}

failures=0
say() { printf '  %-52s %s\n' "$1" "$2"; }

if permitted /allowed "$SCOPED"; then
    say "scoped token on its own path" "accepted"
else
    say "scoped token on its own path" "FAIL: refused"; failures=1
fi

if refused_with /denied "$SCOPED" "Rejected: BIND(path='/denied')"; then
    say "scoped token on another path" "refused"
else
    say "scoped token on another path" "FAIL: accepted"; failures=1
fi

if refused_with /rw "$READONLY" "Rejected: UPLOAD"; then
    say "download-only token uploading" "refused"
else
    say "download-only token uploading" "FAIL: accepted"; failures=1
fi

if [ "$failures" -ne 0 ]; then
    echo "FAIL: the authorization model does not hold through the binaries"
    exit 1
fi

# TLS. The certificates come from certificate-authority/, which already holds a
# chain issued for localhost -- test_sync.cpp uses the same ones. A self-signed
# CN=Test certificate is refused by the client, correctly, and using one here
# tests the client's certificate validation rather than the server's TLS.
CA_DIR="certificate-authority"
TLS_CERT="$CA_DIR/certs/localhost-chain.crt.pem"
TLS_KEY="$CA_DIR/certs/localhost-server.key.pem"
# The root CA, not the signing CA. The test resources copy root-ca/crt.pem, and
# handing the client the signing CA instead makes it hang retrying a handshake
# it can never complete rather than failing outright.
TLS_TRUST="$CA_DIR/root-ca/crt.pem"

if [ -r "$TLS_CERT" ] && [ -r "$TLS_KEY" ] && [ -r "$TLS_TRUST" ]; then
    kill "$SERVER_PID" 2> /dev/null || true
    wait "$SERVER_PID" 2> /dev/null || true
    mkdir -p "$WORK/tlssrv"
    "$SRV" --root "$WORK/tlssrv" --public-key "$WORK/public.pem" --port 0 \
           --tls-cert "$TLS_CERT" --tls-key "$TLS_KEY" > "$WORK/tls.log" 2>&1 &
    SERVER_PID=$!

    TLS_PORT=""
    for _ in $(seq 1 40); do
        TLS_PORT=$(grep -oE 'Listening on localhost:[0-9]+' "$WORK/tls.log" 2> /dev/null \
                   | head -1 | grep -oE '[0-9]+$' || true)
        [ -n "$TLS_PORT" ] && break
        sleep 0.5
    done
    [ -n "$TLS_PORT" ] || { echo "FAIL: the TLS server never reported a port"; cat "$WORK/tls.log"; exit 1; }

    grep -q '(TLS)' "$WORK/tls.log" || { echo "FAIL: the server did not report TLS"; exit 1; }

    ANY=$("$TK" --key "$WORK/private.pem" --identity tls --expires-in 3600)
    # Bounded, because a client that cannot verify the chain does not fail --
    # it retries the handshake forever. Handing it the signing CA instead of the
    # root CA did exactly that, and an unbounded run would spend the job's whole
    # step timeout on it and then report a timeout rather than a bad trust
    # anchor. `timeout` is not on macOS by default, so this is done by hand.
    tlsdir=$(mktemp -d)
    "$LT" --root "$tlsdir" --port "$TLS_PORT" --clients 2 --transactions 5 \
          --token "$ANY" --tls --tls-trust "$TLS_TRUST" --converge > "$tlsdir/out" 2>&1 &
    tlspid=$!
    tlsdone=1
    for _ in $(seq 1 120); do
        kill -0 "$tlspid" 2> /dev/null || { tlsdone=0; break; }
        sleep 0.5
    done
    if [ "$tlsdone" -ne 0 ]; then
        # Job-control notices ("Killed: 9 ...") would otherwise land in the
        # middle of the failure output, which is the moment it most needs to be
        # readable.
        kill -9 "$tlspid" 2> /dev/null || true
        wait "$tlspid" 2> /dev/null || true
        say "two clients converging over TLS" "FAIL: still running after 60s"
        grep -iE 'tls|certificate' "$tlsdir/out" | head -2 || true
        failures=1
    elif wait "$tlspid"; then
        say "two clients converging over TLS" "$(grep -o 'every client sees [0-9]* rows' "$tlsdir/out" || echo ok)"
    else
        say "two clients converging over TLS" "FAIL"
        cat "$tlsdir/out"
        failures=1
    fi
    rm -rf "$tlsdir"
else
    say "two clients converging over TLS" "skipped: certificate-authority/ not readable"
fi

if [ "$failures" -ne 0 ]; then
    echo "FAIL: the deployed path does not hold"
    exit 1
fi

echo "PASS: path scoping, upload privilege and TLS sync hold end to end"
