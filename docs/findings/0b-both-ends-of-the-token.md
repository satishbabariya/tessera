# Each end of the authentication is justified by the other

[0b-server-has-no-auth.md](0b-server-has-no-auth.md) records that the sync server
never looks at the token a client presents on BIND. That is half of it.

The client never sends one:

    // Discard the token since it's ignored by the server.
    std::string empty_access_token;
    ...
    protocol.make_pbs_bind_message(protocol_version, out, session_ident,
                                   server_path, empty_access_token,
                                   need_client_file_ident, is_subserver);

`client_impl_base.cpp:1898`. The variable is declared empty, never assigned, and
passed to both the partition-based and flexible-sync bind messages. Its name says
what it is and the comment above it says why.

`Session::Config::signed_user_token` is accepted from the caller and stored on the
connection as `m_signed_access_token`, where it is used for `update_connect_info`
-- the HTTP request the WebSocket handshake carries. It never reaches BIND.

## The shape of it

The client's comment is *correct*: the server does ignore the token. The server's
behaviour is *reasonable*: nothing sends one. Each end is individually defensible
and justified by the other, and the pair of them means the protocol carries a
credential field that neither side populates or reads.

This is not the same as either half alone. A server that ignores a token clients
send is a server missing a check. A client that omits a token the server requires
fails loudly on the first connection. Both together fail silently, forever,
because there is nothing left to notice the absence.

## How it was found, which is the useful part

Not by reading either file. By attempting to make the server verify, and watching
the suite crash in `base64_decode` -- which only happens on empty input, which
meant the server was receiving an empty token, which meant the client was not
sending one.

Reading `receive_bind_message` gives "the server ignores the token". Reading
`send_bind_message` gives "the client discards it because the server ignores it",
which reads as a description of the server rather than a fact about the client.
Neither reading produces the conjunction. Changing one end and observing the
other did, in about a minute, and it also surfaced
[0b-base64-empty-input.md](0b-base64-empty-input.md) on the way.

## What it means for the work

Adding authentication is a two-sided change, not a one-sided one. The estimate in
[0b-server-installability-cost.md](0b-server-installability-cost.md) counted the
server side and stopped there.

The order is now clearer than it was:

1. The client must send `m_signed_access_token` on BIND rather than an empty
   string, and every test fixture must supply a token that verifies -- which
   `Sync_Auth_TheSuitesOwnTokenVerifiesAgainstTheSuitesOwnKey` confirms the
   existing one does.
2. The server must verify it, and reject on failure with `bad_authentication`,
   `token_expired` or `permission_denied` -- all three of which the client's
   error mapping already handles.
3. Only then may the server become installable, which
   `tools/check-server-not-shipped-unauthenticated.sh` enforces.

Step 1 before step 2, or the suite fails on every bind. That ordering is the
thing this finding buys, and it cost one reverted change to learn.
