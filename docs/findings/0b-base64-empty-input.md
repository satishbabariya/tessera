# base64_decode terminates on empty input

`util::base64_decode("")` aborts the process:

    src/tessera/util/span.hpp:408: Assertion failed: m_size

The overlap guard read:

    TESSERA_ASSERT(input.data() + input.size() <= out_buffer.data()
                   || input.data() > &out_buffer.back());

`Span::back()` asserts on an empty span, and an empty input produces a
zero-length output buffer, because `base64_decoded_size(0)` is 0. When the first
disjunct is false the second is evaluated, and the guard against overlapping
buffers becomes the thing that crashes.

`AccessToken::parse("")` inherits it directly: the parser splits on `:`, decodes
each half, and terminates on the first.

## Why it had never been reached

Nothing calls `AccessToken::parse` on the server -- see
[0b-server-has-no-auth.md](0b-server-has-no-auth.md) -- and the one existing
test, `Sync_Auth_JWTAccessToken`, passes a valid 900-character JWT. The parser
had never been given anything malformed, and the only caller that would give it
malformed input is the one that does not exist yet.

That is the shape of it: **a server that authenticates is a server that parses
hostile input, and this parser had never seen any.** An empty token is what a
client sends when it has not been given one, and the first thing anyone probing
an exposed port sends. Adding authentication without fixing this would have
turned an authentication gap into a remote abort.

## The fix

The guard computes the end pointer instead of dereferencing the last element,
and short-circuits on either span being empty:

    TESSERA_ASSERT(input.empty() || out_buffer.empty() ||
                   input.data() + input.size() <= out_buffer.data() ||
                   input.data() >= out_buffer.data() + out_buffer.size());

`Sync_Auth_MalformedTokensAreRejectedNotFatal` covers six inputs: empty, a bare
separator, an empty payload, an empty signature, an invalid alphabet, and a
single part with no signature. Each must return false with a parse error rather
than terminate.

## Two things found alongside it

`sync/protocol.cpp:250` maps `ProtocolError` values to `ErrorCodes` in a switch
whose last group falls through to `TESSERA_UNREACHABLE()`. A client that receives
`bad_server_file_ident`, `partial_sync_disabled`, `too_many_sessions`,
`server_file_deleted`, `client_file_blacklisted`, `user_blacklisted` or
`transact_before_upload` terminates. A server can abort any client connected to
it by sending one of seven error codes. The three a bind rejection would use --
`bad_authentication`, `permission_denied`, `token_expired` -- are handled, so
this is not on the authentication path, but it is the same class of gap: a code
path unreachable today, load-bearing the moment something reaches it.

And an attempt to wire verification into `receive_bind_message` rejected the test
suite's own token with `invalid_signature`, while a unit test verified that same
token against that same key successfully. That is now understood, and it is why
this fix exists at all: the server was receiving an empty token, so `parse` was
being handed `""` and terminating in `base64_decode`. See
[0b-both-ends-of-the-token.md](0b-both-ends-of-the-token.md). The server change
was reverted, because closing one end of an authentication that has neither end
is not an improvement.
