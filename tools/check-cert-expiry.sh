#!/usr/bin/env bash
# Fails when a test certificate is close enough to expiry that it will lapse
# before anyone looks again.
#
# The SSL tests verify a real TLS handshake against certificates committed under
# certificate-authority/. When those expire the handshake fails, and the failure
# surfaces as a certificate-verification error inside a test named for the
# socket behaviour it was written to cover -- nothing points at the calendar.
#
# The CA database records this happening in 2018, 2020, 2021, 2022 and 2024:
# every couple of years the certificates lapsed and were quietly re-issued. This
# check is what makes the next lapse arrive as a sentence instead of a puzzle.
set -euo pipefail
cd "$(dirname "$0")/.."

# Long enough that the warning appears with room to act, short enough that a
# certificate at the maximum permitted lifetime does not sit permanently in
# warning.
WARN_DAYS=180
WARN_SECONDS=$((WARN_DAYS * 86400))

# Apple's Security framework rejects a TLS server certificate whose validity
# period exceeds 825 days. The handshake fails with errSSLXCertChainInvalid
# (-9807) and says nothing about duration; the certificate passes `openssl
# verify` and a real `openssl s_client` handshake, so every diagnostic available
# on Linux says it is fine.
#
# This is why the upstream conf carries default_days = 825. It is the limit, not
# a round number. Re-issuing these for ten years cost five failing tests across
# macOS and Linux, and the ceiling is checked here so the next person is told
# rather than left to find out.
MAX_DAYS=825

status=0
found=0

while IFS= read -r cert; do
    # Chains concatenate the leaf and the signing CA; openssl x509 reads the
    # first certificate in the file, which is the leaf, and that is the one
    # that expires first.
    enddate=$(openssl x509 -in "$cert" -noout -enddate 2>/dev/null | cut -d= -f2) || continue
    [ -n "$enddate" ] || continue
    found=$((found + 1))

    # Validity period, not just the end date. The ceiling applies to server
    # certificates only: the root and signing CAs here are valid until 2124 and
    # Apple accepts them, because the limit is a policy on the leaf presented in
    # a TLS handshake, not on the anchors above it.
    is_ca=$(openssl x509 -in "$cert" -noout -ext basicConstraints 2>/dev/null | grep -c 'CA:TRUE' || true)
    if [ "$is_ca" = "0" ]; then
    begin=$(openssl x509 -in "$cert" -noout -startdate 2>/dev/null | cut -d= -f2)
    b=$(date -j -f "%b %d %T %Y %Z" "$begin" +%s 2>/dev/null \
        || date -d "$begin" +%s 2>/dev/null) || b=""
    e=$(date -j -f "%b %d %T %Y %Z" "$enddate" +%s 2>/dev/null \
        || date -d "$enddate" +%s 2>/dev/null) || e=""
    if [ -n "$b" ] && [ -n "$e" ]; then
        span=$(( (e - b) / 86400 ))
        if [ "$span" -gt "$MAX_DAYS" ]; then
            echo "FAIL: ${cert#./} is valid for $span days, over the $MAX_DAYS-day maximum"
            echo "      Apple rejects these with errSSLXCertChainInvalid and no explanation."
            status=1
        fi
    fi
    fi

    if ! openssl x509 -in "$cert" -noout -checkend 0 > /dev/null 2>&1; then
        echo "FAIL: ${cert#./} EXPIRED on $enddate"
        status=1
    elif ! openssl x509 -in "$cert" -noout -checkend "$WARN_SECONDS" > /dev/null 2>&1; then
        echo "FAIL: ${cert#./} expires $enddate, within $WARN_DAYS days"
        status=1
    fi
done < <(find certificate-authority -name '*.pem' -not -name '*key.pem' -not -name '*csr.pem' | sort)

if [ "$found" -eq 0 ]; then
    # The certificates moving or being renamed must not silently turn this
    # check into a no-op that passes.
    echo "FAIL: no certificates found under certificate-authority/"
    exit 1
fi

if [ "$status" -ne 0 ]; then
    echo
    echo "Re-issue them with:"
    echo "  ./certificate-authority/regenerate-server-certs.sh"
    echo "then run the SSL tests before committing:"
    echo "  UNITTEST_FILTER='Util_Network_SSL_*' <build>/test/tessera-sync-tests"
    exit 1
fi

echo "PASS: $found certificates valid for 180+ days and within the ${MAX_DAYS}-day ceiling"
