# The token was never missing; it is in the handshake URL

[0b-both-ends-of-the-token.md](0b-both-ends-of-the-token.md) concluded that
neither end of the authentication was populated: the server ignores the BIND
token, and the client sends an empty string for it. Both halves are true. The
conclusion drawn from them was wrong.

The client does send the token. Not on BIND -- in the WebSocket handshake:

    std::string ClientImpl::Connection::get_http_request_path() const
    {
        const auto param = m_http_request_path_prefix.find('?') == npos
                               ? "?baas_at="sv : "&baas_at="sv;
        path += m_http_request_path_prefix;
        path += param;
        path += m_signed_access_token;
        return path;
    }

`client.cpp:1788`. `baas_at` is the App Services access token, carried as a query
parameter on the HTTP request that opens the socket. Every connection has
presented a credential the whole time.

## Why BIND is empty, on purpose

Upstream removed it. `af504f10d`, January 2022, realm-core PR #5151,
*"Discard token from bind and refresh messages"*:

    src/realm/sync/noinst/server/server.cpp    | 240 +---------------
    test/test_sync.cpp                         | 228 +---------------
    doc/protocol/protocol_1.md                 |  35 +---
    7 files changed, 19 insertions(+), 606 deletions(-)

Its changelog entry: *"Send empty access token in bind messages."* Among the
deletions:

    -    m_connection.get_server().get_access_control().verify_access_token(signed_user_token, &error);
    -    if (REALM_UNLIKELY(!access_control.can(*m_access_token, Privilege::Download, path))) {

That second line is, character for character, the check this project was about to
add back. Authentication moved from the sync protocol to the transport, because
in Realm's deployment App Services terminated the connection and authenticated
there. The bundled server was left with the machinery and no caller, which is
what [0b-server-has-no-auth.md](0b-server-has-no-auth.md) found.

So the accurate statement is not "neither end is populated". It is: **the client
authenticates at the transport, and the server does not check.** One end, not
neither.

## What this changes

The server already sees the token. `handle_request_for_sync(const HTTPRequest&)`
at `server.cpp:1688` receives the full request, and `handle_http_request` reads
`request.path` at 1667. Nothing parses the query string.

Authentication belongs there, not at BIND:

* it happens once per connection rather than once per session
* it can reject with an HTTP status before any sync protocol state exists
* it is where the client already puts the token
* it does not reinstate a mechanism upstream removed deliberately

## The correction this owes

Pull request #23 changed `send_bind_message` to send the connection's token
instead of an empty string. That was based on this incomplete picture. It is not
harmful -- the field is ignored at both ends -- but it puts a second copy of the
credential on the wire per session, and re-adds a field that upstream emptied on
purpose. It should be reverted.

The reasoning that produced it was sound and the evidence was partial. Reading
`send_bind_message` shows a client discarding a token. Reading
`get_http_request_path` shows the same client sending one. Only the second
question was not asked, because the first answer was sufficient to explain the
crash being investigated.

That is the risk in an investigation that starts from a symptom: it stops when
the symptom is explained. The symptom -- `base64_decode` aborting on an empty
token -- was fully explained by the empty BIND field, and the explanation was
correct, and the conclusion drawn from it about the system was not.
