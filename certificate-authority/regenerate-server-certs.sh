#!/usr/bin/env bash
# Re-issues the three server certificates the SSL tests use, from the existing
# private keys and the existing signing CA.
#
# Run this when tools/check-cert-expiry.sh starts warning. It does NOT touch the
# root or signing CA -- those are valid until 2124 and regenerating them is a
# separate, larger job documented in HOW_TO_UPDATE.md.
#
# The subject alternative names originate in the per-server .conf files, travel
# into the CSR via req_extensions, and reach the certificate because
# signing-ca.conf sets copy_extensions = copy. The CSR is rebuilt here rather
# than re-signed so that editing a .conf is enough to change a SAN. Rebuilding
# reuses the existing key, and an RSA PKCS#1 signature over identical input is
# deterministic, so an unmodified .conf leaves the .csr.pem byte-identical and
# out of the diff.
#
# To inspect what a CSR carries, use `openssl req -noout -text`. There is no
# `openssl req -ext` -- it exits 1 with "Extra (unknown) options", which reads
# as an empty SAN list if stderr is discarded.
set -euo pipefail
cd "$(dirname "$0")"

# 825 days, and not one more. Apple's Security framework rejects a TLS server
# certificate whose validity period exceeds 825 days, returning
# errSSLXCertChainInvalid (-9807) from the handshake -- it does not say why, and
# the certificate verifies perfectly under `openssl verify` and against
# `openssl s_client`. The upstream conf has carried default_days = 825 all
# along, which is exactly this limit rather than a round number.
#
# Issuing for ten years instead cost five failing tests across macOS and Linux.
DAYS=825

# Snapshot new_certs_dir so the archive copies openssl is about to write can be
# identified by difference rather than by pattern.
BEFORE=$(mktemp)
ls signing-ca | sort > "$BEFORE"

issue() {
    local name=$1 subj=$2
    echo "  $name"
    # Rebuild the CSR from the .conf so req_extensions -> subjectAltName is
    # applied, reusing the existing key so the diff stays to the certificate.
    openssl req -new -config "$name.conf" -key "certs/$name.key.pem" \
        -out "certs/$name.csr.pem" -subj "$subj" 2>/dev/null
    openssl ca -config signing-ca.conf -in "certs/$name.csr.pem" \
        -out "certs/$name.crt.pem" -extensions server_ext -days "$DAYS" \
        -batch 2>/dev/null
}

echo "Issuing server certificates for $DAYS days:"
issue dns-checked-server "/DC=www.example.com/CN=www.example.com"
issue ip-server          "/DC=www.example.com/CN=www.example.com"
issue localhost-server   "/DC=localhost/CN=localhost"

# Each leaf is served with the signing CA appended, so a client that trusts only
# the root can build the path.
echo "Building chains"
cat certs/dns-checked-server.crt.pem signing-ca/crt.pem > certs/dns-chain.crt.pem
cat certs/ip-server.crt.pem          signing-ca/crt.pem > certs/ip-chain.crt.pem
cat certs/localhost-server.crt.pem   signing-ca/crt.pem > certs/localhost-chain.crt.pem
openssl x509 -in certs/localhost-chain.crt.pem -outform der -out certs/localhost-chain.crt.cer

# openssl ca archives a copy of every certificate it issues under new_certs_dir,
# named for its serial. They duplicate what is now in certs/ and must not be
# committed. Delete by comparing against the snapshot taken before signing --
# a [0-9A-F]*.pem glob looks equivalent and is not: under macOS collation the
# range also matches lowercase, so it deletes the signing CA's own crt.pem.
comm -13 "$BEFORE" <(ls signing-ca | sort) | while read -r stray; do
    rm -f "signing-ca/$stray"
done
rm -f "$BEFORE" signing-ca/db/*.old

echo "Verifying"
for leaf in dns-checked-server ip-server localhost-server; do
    openssl verify -CAfile root-ca/crt.pem -untrusted signing-ca/crt.pem \
        "certs/$leaf.crt.pem" > /dev/null
    san=$(openssl x509 -in "certs/$leaf.crt.pem" -noout -ext subjectAltName \
        | tail -n +2 | tr -s ' ' | sed 's/^ //')
    printf '  %-20s %s\n' "$leaf" "$san"
done
echo "Done. Run the SSL tests before committing:"
echo "  UNITTEST_FILTER='Util_Network_SSL_*' <build>/test/tessera-sync-tests"
