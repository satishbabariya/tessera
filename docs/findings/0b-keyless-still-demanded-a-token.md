# The server that authenticates nobody refused every connection

`ObjectStoreTests` hung. Not failed -- hung, on all five platforms, with no
output at all, until GitHub's `timeout-minutes: 60` killed the job:

```
18:53:58Z  ##[group]Run "$(tools/verify/test-binary.sh build tessera-object-store-tests)"
19:42:40Z  ##[error]The operation was canceled.
19:42:40Z  Terminate orphan process: pid (10340) (tessera-object-store-tests)
```

The same step on `main` takes **19 seconds**.

## The defect

Authenticating at the handshake has to allow for the keyless server: one
constructed with no public key, which can verify nothing and therefore
authenticates nobody. That mode is deliberate and documented, and
`Sync_RunServerWithoutPublicKey` covers it.

The first implementation detected it like this:

```cpp
util::Optional<AccessToken> token =
    access_control.verify_access_token(handshake_token, &parse_error);

if (!token) {
    handle_401_unauthorized(...);          // <- empty token lands here
    return;
}
if (parse_error == AccessToken::ParseError::invalid_signature) {
    // keyless: verify_access_token reports this when it holds no key
}
```

`verify_access_token` signals keyless-ness by reporting `invalid_signature` on a
token it *parsed*. So the keyless branch is reachable only for tokens that
parse, and the `!token` rejection sits in front of it.

A client with no token yet sends the parameter with nothing after it:

```
PROBE handshake path=[/tessera-sync?baas_at=] token_len=0 got=0 err=2
PROBE -> 401 (no token)
```

An empty string does not parse. So a server holding no public key -- one that
had already decided to authenticate nobody -- answered 401 to a connection it
was never meant to examine. The client retried. Forever.

## Why a client sends an empty token

Because it is waiting for one. The object-store test at `db.cpp:1120`, "can
async open while waiting for a token refresh", is built precisely on that state:
it sets an expired token, lets the client discover it needs a refresh, and holds
the refresh completion open while an async open proceeds. During that window the
session has no token, and it connects anyway.

That is not a test artefact. It is the ordinary shape of a client whose
credential has lapsed and is being renewed.

## Why the existing keyless test did not catch it

`Sync_RunServerWithoutPublicKey` presents `g_unsigned_test_user_token` -- a
token with no signature. It *parses*. So it reached the keyless branch and
passed, throughout.

The gap was never "keyless mode is untested". It was that keyless mode was
tested with a token, and the case that mattered was no token. With the defect
reinstated, the new test fails alone and the old one still passes:

```
ERROR in Sync_Auth_AKeylessServerAcceptsAClientWithNoToken: CHECK_NOT(did_fail) failed
FAILURE: 1 out of 9 tests failed
=== keyless test ===
Success: All 1 tests passed (3 checks)
```

## What was done

`AccessControl::has_public_key()` answers the question before parsing, so the
decision no longer depends on a token being well-formed:

```cpp
if (!access_control.has_public_key()) {
    logger.warn("... this server was given no public key and authenticates nobody");
}
else {
    // verify; 401 on a missing, unverifiable or expired token
}
```

Two tests, as a pair: a keyless server must accept a client with no token, and a
verifying server must refuse one. The second is what stops the first from
passing against a server that simply stopped authenticating.

`test/sync_fixtures.hpp` gained `default_the_user_token`, because the fixture
could not express "no token" at all -- it substituted the default for any empty
string. The comment there claimed a test wanting no token "still sets one
explicitly and this leaves it alone", which was not true of the code beneath it.

## What this cost, and what caught it

Four pull requests were stacked on the defect and all four were one merge away
from `main`. What caught it was `timeout-minutes: 60`, added earlier for exactly
this -- a test that deadlocks would otherwise hold a runner for GitHub's
six-hour default.

But the timeout reports the job as **`cancelled`**, which is the same word
GitHub uses for a concurrency cancellation, and for a long time the polling in
use rendered it as `5 of 7` -- indistinguishable from five checks still running.
See [0b-green-by-absence.md](0b-green-by-absence.md). The hang was visible for
about two hours before anything said so out loud.
