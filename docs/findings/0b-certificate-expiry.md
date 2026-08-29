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

## 825 is not a round number

The first attempt re-issued the certificates for 3650 days. CI failed on macOS
Debug, macOS Release and Linux gcc Debug -- five sync tests, all of them
certificate tests, with `securetransport:-9807`.

**Apple's Security framework rejects a TLS server certificate whose validity
period exceeds 825 days.** The handshake returns `errSSLXCertChainInvalid` and
says nothing about duration. Every diagnostic available elsewhere reports the
certificate as sound: `openssl verify -CAfile root-ca/crt.pem -untrusted
certs/dns-chain.crt.pem` returns OK, and a real `openssl s_client` handshake
against `openssl s_server` reports `Verify return code: 0 (ok)`.

`default_days = 825` in `signing-ca.conf` was upstream encoding that limit. It
looked like an arbitrary two-year-ish figure and it is the exact ceiling.

The certificates are now issued for 825 days, expiring December 2028, and
`check-cert-expiry.sh` enforces the ceiling as well as the floor: it fails on any
leaf valid for more than 825 days. The ceiling applies to leaves only -- the root
and signing CAs here are valid until 2124 and Apple accepts them, because the
limit is a policy on the certificate presented in the handshake, not on the
anchors above it. Both directions are canary-tested.

## The verification that was not one

Before opening the pull request this finding claimed the SSL suite passed 21/21
and the sync suite 461/461 against the new certificates. Both numbers were real
and neither meant anything.

`test/CMakeLists.txt` globs the certificate files into the test target's
resources, and they are copied into the app bundle **at build time**. Between
regenerating the certificates and running the tests I had not rebuilt. The suite
ran against the certificates already sitting in the bundle -- the old ones, which
work. The new certificates had never been executed at all.

The tests were green, the certificates were broken, and both were true at once.
This is the third time in this project a gate has passed against a stale
artefact, and the second time it was a stale test binary or bundle specifically.
The general form: a build system that copies inputs at build time makes "run the
tests" and "test the current inputs" two different actions.

## A test that pinned the certificate's bytes

The Linux job failed differently, and for a second independent reason:

    ERROR in Sync_SSL_Certificate_Verify_Callback_3:
      CHECK_EQUAL(pem_data[1667], 'J') failed with (S, J)

`Sync_SSL_Certificate_Verify_Callback_3` verified that the SSL verify callback
receives the right certificates by asserting `pem_size == 1700` and four
individual characters of the base64: `pem_data[1667] == 'J'`,
`pem_data[93] == 'G'`, and the two closing bytes. `Sync_SSL_Certificate_Verify_Callback_2`
transcribed the first two lines of the signing CA's base64 as a string literal.

Both hold only for one particular issuance. Re-signing a certificate changes
every base64 byte while leaving every length identical -- which is why the test
failed on a single character and not on `pem_size`, and why it says nothing
about *which* certificate arrived.

They now compare against the certificate blocks in the file the server was
actually configured with. That is the property the test names claim, and it
survives re-issuance.

These tests are `#if TESSERA_HAVE_OPENSSL`, so they never run on macOS, where
the build uses SecureTransport. Verifying the fix meant configuring a second
build with `-DTESSERA_FORCE_OPENSSL=ON`, which is how the count of 462 tests on
Linux against 461 on macOS was reconciled. That configuration also has two
pre-existing failures of its own, `Util_Network_SSL_BrokenPipeOnWrite` and
`...OnShutdown`, which reproduce on an unmodified tree: nothing builds
OpenSSL-on-macOS, so nothing had noticed.

## What the check does

`tools/check-cert-expiry.sh` fails when any certificate is within 180 days of
expiry, and when any leaf is issued for more than 825 days. A certificate at the
maximum lifetime still expires; what changes is that the next lapse arrives as a
sentence naming the file and the date, rather than as a handshake error.

The check is canary-tested five ways: a certificate expiring in 30 days, one
already expired, a leaf over the 825-day ceiling, a CA over the ceiling (which
must pass), and the certificates renamed away -- the last because a check that
finds nothing must fail rather than report success over an empty set.

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
