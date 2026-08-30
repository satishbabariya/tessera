# Fourteen tokens, three tests, nothing checked

`test/sync_fixtures.hpp` defines fourteen access tokens:

| | |
|---|---|
| `g_signed_test_user_token` | download and upload, any path, no expiry |
| `g_unsigned_test_user_token` | no signature |
| `g_user_0_token`, `g_user_1_token`, `g_user_2_token` | three distinct identities |
| `g_user_0_path_test_token`, `g_user_1_path_test_token` | identities scoped to a path |
| `g_signed_test_user_token_readonly` | download only |
| `g_signed_test_user_token_for_path` | scoped to one path |
| `g_signed_test_user_token_expiration_unspecified` / `_null` / `_specified` | three expiry shapes |
| `g_signed_test_user_token_sync_label_default` / `_custom` | two sync labels |

That is the fixture set for an authorization system: distinct users, read-only
access, path scoping, expiry, labels. Someone built the data to test all of it.

Six of the fourteen are referenced by no test at all -- `g_user_2_token`, both
`_path_test_` tokens, `_expiration_specified`, and both `_sync_label_` tokens.

Three tests mention tokens in their names, and all three assert that nothing goes
wrong:

    Sync_TokenWithoutExpirationAllowed      CHECK_NOT(did_fail)
    Sync_TokenWithNullExpirationAllowed     CHECK_NOT(did_fail)
    Sync_RefreshSignedUserToken             waits, asserts nothing

The comment on a neighbouring test says it plainly: *"The check of the test is
just the absence of an error."*

## Why they cannot fail

The server does not read the token. See
[0b-server-has-no-auth.md](0b-server-has-no-auth.md). Every one of these tests
passes for the same reason an empty test passes.

`g_signed_test_user_token_readonly` grants download and not upload. A test using
it can upload freely. `g_signed_test_user_token_for_path` is scoped to one path.
A session presenting it can bind to any path. Neither test asserts otherwise,
which is the only reason they are green rather than wrong.

## What this is an instance of

The other findings here concern a check that inspected the wrong thing, or a test
that never ran. This is neither. These tests run, and check exactly what they say
they check -- that a valid token is not rejected. The gap is that nothing checks
the complement: that an invalid token *is* rejected.

A test suite that only asserts acceptance cannot distinguish a system that
accepts the right things from one that accepts everything. That is the same
shape as the canary discipline applied to `tools/check-*.sh` -- a check is not a
check until it has been shown to fail -- arriving at the test suite from the
other direction.

## What to do with them

Nothing yet, and deliberately. These fixtures become useful the moment the server
verifies tokens, and not before: a test asserting that a read-only token cannot
upload would fail today for the right reason and could not be made to pass
without the server change it is waiting on.

When that lands, the tests it needs are already implied by the data:

* a token with a bad signature is refused
* an expired token is refused
* a token scoped to `/a` cannot bind `/b`
* a read-only token cannot upload
* an unsigned token is accepted only by a server configured with no key,
  which `Sync_RunServerWithoutPublicKey` already covers from the other side

Six unused fixtures and five missing tests is not a coincidence. The data was
built for the tests; the tests were never written.
