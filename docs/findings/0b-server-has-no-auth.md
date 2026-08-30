# The sync server authenticates nothing

The bundled sync server performs no authentication and no authorization. It
accepts a `signed_user_token` on every BIND message, logs it, and never refers to
it again.

Every piece of the machinery that would use it is unreachable:

| | |
|---|---|
| `AccessToken::verify_access_token` | never called |
| `AccessControl::can` | never called |
| `AccessToken` | never constructed in `server.cpp` |
| `ServerImpl::get_access_control()` | defined at `server.cpp:796`, called by nobody, including itself |
| the `PKey` passed to `Server(root_dir, pkey, config)` | stored in `m_access_control`, never read |
| `Config::authorization_header_name` | used once in `server.cpp:3875`, to log its own value at startup |

`access_token.cpp`, `access_control.cpp`, `permissions.cpp` and the three
`crypto_server` implementations are 761 lines that nothing calls.

The consequence is direct: anyone who can reach the server's port can bind to any
database path and read and write it.

## What is not affected

Nothing is exposed by this today. The server is not in the installed package --
no `Tessera::SyncServer` target, no installed header, no executable -- so it
cannot be reached from outside a build tree. The 461 sync tests run it in-process.

## The claim this corrects

`README.md` and [0b-self-hostable.md](0b-self-hostable.md) both said the opposite:

> the server still expects the client to present an App Services access token: it
> requires a parseable signed JWT on every bind, and when no public key is
> configured it parses the token without verifying the signature -- a mode the
> source describes as being for testing

Every clause of that is wrong. It came from reading `AccessControl::verify_access_token`,
finding the comment that says the missing-key path exists "for the purpose of
testing", and concluding that the function was therefore on the request path. It
is not called at all.

That is the failure this directory documents more than any other: a function was
read, and its being called was assumed. The check that would have caught it is
the one applied to everything else here -- follow the call graph rather than the
name -- and it was not applied, in a paragraph about a security boundary.

## What it changes about the next milestone

It makes the work simpler and the ordering stricter.

Simpler, because there is no App Services entanglement to unpick. The
authorization model that exists is six lines and is not App-Services-shaped at
all: `can()` checks that the token's path matches the file and that its `access`
bitfield contains the requested privilege. That is a reasonable model. The
App-Services-specific parts are the token *encoding* -- a JWT carrying `app_id`,
`sync_label` and `identity` -- and the claims `app_id`, `sync_label` and `admin`,
none of which `server.cpp` reads.

That ordering is enforced rather than described.
`tools/check-server-not-shipped-unauthenticated.sh` fails if the server acquires
an `install(TARGETS)` rule or an executable while `server.cpp` still calls
neither `verify_access_token` nor `can()`. It is a conjunction on purpose: a
server that authenticates nothing and cannot be reached is what exists today, and
a server that authenticates properly may be installed freely. Only the pairing is
refused, and the check says so when it passes rather than claiming a safety it is
not measuring.

Stricter, because the sequence now matters. Making the server installable is
mechanical: 36 include sites and one header. Adding authentication is not. Doing
the mechanical part first would ship a reachable server that authenticates
nobody, which is worse than shipping nothing.
