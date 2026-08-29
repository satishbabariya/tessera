# The test certificates expire, and nothing said so

`certificate-authority/` holds the TLS certificates the SSL tests hand to a real
server socket. Twenty-one tests in `test_util_network_ssl.cpp` and part of
`test_sync.cpp` complete an actual handshake against them.

Every server certificate expired **25 October 2026** -- 57 days after this was
found.

## Why it would have been hard to diagnose

Nothing in the failure would have mentioned a date. The tests are named for the
socket behaviour they cover -- `Util_Network_SSL_BrokenPipeOnHandshakeWrite`,
`Util_Network_SSL_ShutdownBeforeCloseNotifyReceived` -- so the report would show
a cluster of unrelated-looking socket tests failing on certificate verification,
on every platform at once, with no commit to blame. The repository would have
looked broken rather than stale.

## It had happened five times before

The signing CA keeps an issuance database, and it records the whole history:

    ...084546Z  0A   /DC=www.example.com/CN=www.example.com
    240722150953Z  14   /DC=www.example.com/CN=www.example.com
    261025154405Z  18   /DC=www.example.com/CN=www.example.com

Certificates were re-issued in 2018, 2020, 2021, 2022 and 2024. `default_days`
is 825, so upstream hit this roughly every two years and quietly re-issued each
time. `HOW_TO_UPDATE.md` documents how to regenerate, which is the artefact of a
problem solved repeatedly and never prevented.

## What was done

Re-issued the three server certificates for 3650 days from the existing keys and
the existing signing CA, which is valid until 2124. Subjects and subject
alternative names are unchanged; the SSL suite passes 21/21 and the full sync
suite 461/461.

The durable half is `tools/check-cert-expiry.sh`, which fails when any
certificate is within 180 days of expiry. A long-dated certificate still expires;
what changes is that the next lapse arrives as a sentence naming the file and the
date, rather than as a handshake error.

The check is canary-tested three ways: a certificate expiring in 30 days, one
already expired, and the certificates renamed away -- the last because a check
that finds nothing must fail rather than report success over an empty set.

## A note on reading tool output

The regeneration script initially carried a comment stating that the committed
CSRs did not contain the subject alternative names. That was wrong, and it came
from this:

    openssl req -in certs/localhost-server.csr.pem -noout -ext subjectAltName 2>/dev/null

`openssl req` has no `-ext` option. It exits 1 with
`Extra (unknown) options: "ext" "subjectAltName"`, and `2>/dev/null` turned that
into empty output, which read as "this CSR has no SANs." The correct invocation
is `openssl req -noout -text`. The CSRs carried the SANs all along.

This is the same shape as every other defect recorded in these findings -- the
command inspected something adjacent to the property being claimed -- with the
addition that discarding stderr is what made the two indistinguishable.

## A second one, in the cleanup path

The script deleted openssl's archived copies with `rm -f signing-ca/[0-9A-F]*.pem`.
Under macOS collation that range also matches lowercase, so it deleted
`signing-ca/crt.pem` -- the signing CA certificate itself. It was recovered from
git, and the script now deletes by comparing against a snapshot taken before
signing rather than by pattern.
