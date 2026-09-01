# A token expired, and the session kept every privilege it had

The server checked `AccessToken::expired` exactly once, in
`handle_request_for_sync`, while deciding whether to upgrade the WebSocket. That
answers one question -- *is this token valid right now* -- and the answer was
never revisited. A connection accepted at T with a token expiring at T+10
carried full privileges at T+10000, for as long as the socket stayed open.

`AccessControl::can` does not help. It compares the token's path against the
requested one and masks the access bits; it takes no clock and never had one:

```cpp
bool AccessControl::can(const AccessToken& token, Privilege permission,
                        const RealmFileIdent& realm_file) const noexcept
{
    if (token.path && *token.path != realm_file)
        return false;
    unsigned int p = static_cast<unsigned int>(permission);
    return (token.access & p) == p;
}
```

Nor did anything else. `ProtocolError::token_expired` existed and the server
never emitted it, anywhere. There was no expiry timer, no session sweep, and no
`REFRESH` message to re-present a credential on: upstream removed REFRESH in
realm-core #5151 and moved the credential onto the WebSocket URL, so
`Session::refresh()` works by *reconnecting*. Every path that could have
re-examined the token had been closed, and the one remaining check ran before
the session existed.

## Why this was reachable, not theoretical

Sessions are multiplexed over one connection. A connection open long enough for
its token to lapse can still be asked to bind a *new* session, and the handshake
has no say in it -- it ran once, earlier, when the token was good. From the
server log of the test that now covers this:

```
Client: Connection[1:6a947e1f24...] Session[2]: Sending: BIND(session_ident=2, server_path=/b)
Server: Sync Connection[1]: Session[2]: Received: BIND(server_path=/b, signed_user_token='')
Server: Sync Connection[1]: Session[2]: Rejected: BIND(path='/b') -- the access token has expired
Server: Sync Connection[1]: Session[2]: Protocol error: Access token expired (error_code=202)
Client: Connection[1:6a947e1f24...] Session[2]: Received: ERROR "Access token expired" (error_code=202)
```

Note `Connection[1]` throughout: one connection, two sessions, the second bound
long after the first was authenticated.

## What was done

`SyncConnection::access_token_expired()` reads the same clock the handshake
read, and BIND and UPLOAD consult it where they consult privileges. Deliberately
not a timer: an idle connection is not the hazard, a connection still being used
is, and every use passes through one of those two messages.

Four tests, in canary pairs -- a rejection and its complement, so that neither
could pass against a server that simply refuses everything:

| test | canary: with the check removed |
|---|---|
| `ATokenThatExpiresMidSessionCannotUpload` | fails (1 of 15) |
| `ATokenThatHasNotExpiredMayStillUpload` | passes |
| `ASecondSessionOnALiveConnectionSeesTheExpiry` | fails (1 of 15) |
| `ASecondSessionBindsWhileTheTokenIsStillGood` | passes |

The BIND pair asserts `ErrorCodes::AuthError` rather than merely asserting
failure. An expired token given to a *fresh* connection is refused by the
handshake with HTTP 401, which also fails the test; only the error code
distinguishes a rejection that came from BIND on a live connection from one that
never got past the upgrade. A test that checked only "did it fail" would pass
with the BIND check deleted, because the handshake would catch the easy case.

## What made it safe to fix

Emitting `token_expired` would have been worse than not enforcing expiry until
very recently. It was one of the twelve `ProtocolError` values that fell through
to `TESSERA_UNREACHABLE()` in `protocol.cpp` -- see
[0b-client-terminates-on-errors.md](0b-client-terminates-on-errors.md). A server
enforcing expiry would have aborted its clients' processes rather than
disconnecting their sessions. The mapping to `AuthError` had to land first, and
the log above -- where the client receives the error, ends that session and
keeps running -- is what that fix bought.

The order was accidental. The mapping was fixed because twelve unreachable cases
in a switch are obviously wrong, not because anything was known to send them.
