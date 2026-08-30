# A server can terminate any client connected to it

`sync/protocol.cpp` maps `ProtocolError` values, which arrive from the server in
an ERROR message, onto `ErrorCodes` the client reports to its embedder. Twelve of
them fell through to:

    TESSERA_UNREACHABLE();

which aborts the process. A server -- or anything able to send a frame on the
WebSocket -- could terminate any connected client by sending one of:

    limits_exceeded          bad_server_file_ident      server_file_deleted
    token_expired            partial_sync_disabled      client_file_blacklisted
    bad_authentication       unsupported_session_feature  user_blacklisted
    no_such_realm            too_many_sessions          transact_before_upload

Two of those are the ones a server sends when it refuses a bind: `token_expired`
and `bad_authentication`. **A client could not survive being told its
authentication had failed.**

## Why it had never fired

Nothing sends them. The server does not authenticate, so it never rejects a bind
for a bad token -- see [0b-server-has-no-auth.md](0b-server-has-no-auth.md) --
and the remaining ten belong to features this fork has not reached.

`TESSERA_UNREACHABLE()` was not wrong when written. It became wrong when the
protocol grew codes the mapping did not, and nothing noticed because no message
carried one.

## How it was found, and a mistake made on the way

By adding server-side verification and watching the suite abort in the client's
event loop rather than fail a test.

Before that, this document's predecessor recorded the opposite. Asked whether the
three bind-rejection codes were handled, the check was a grep for each name and a
comparison of line numbers against the `TESSERA_UNREACHABLE` line -- which
reported all three as "handled" because they appear *before* it in the file. They
appear before it because they fall through *to* it. `permission_denied` really is
handled, at line 182; `token_expired` and `bad_authentication` are at 229 and 231,
inside the group.

Reading the switch takes ten seconds. Counting line numbers took less and was
wrong, in a paragraph about whether a security-relevant path was safe. That is
the second pattern in [README.md](README.md), committed while investigating an
instance of it.

## The mapping now

| | |
|---|---|
| `token_expired`, `bad_authentication` | `AuthError` |
| `user_blacklisted`, `client_file_blacklisted` | `SyncPermissionDenied` |
| `no_such_realm`, `server_file_deleted` | `SyncServerPermissionsChanged` |
| the remaining six | `SyncProtocolInvariantFailed` |

A client that receives one of these now reports an error instead of aborting.
SyncTests 464, unchanged, because nothing in the suite sends one -- which is
exactly the property that let this survive.
