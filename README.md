# Tessera

An embedded, local-first database with built-in sync.

Tessera runs inside your process and stores data in a single file. No server, no
daemon, no connection string. It is a fork of
[realm-core](https://github.com/realm/realm-core), the storage engine behind the
Realm mobile databases, continued as an independent project after MongoDB wound
the originals down. See [FORK.md](FORK.md).

**Status: pre-release.** The API is settling and the file format is new. It is not
yet recommended for production data you cannot regenerate.

## Why you might want it

- **Zero-copy reads.** The file is memory-mapped and queries return lazy views
  over it. There is no deserialisation step and no buffer pool.
- **Crash safety by construction.** Commits write new nodes and swap a single
  root pointer. There is no write-ahead log to replay, because there is nothing
  to replay.
- **Reactive queries.** Results stay live and tell you which rows and fields
  changed, not merely that something did.
- **Convergent sync.** A production-tested operational-transform engine and a
  sync client, both installable today, with no cloud account and no vendor. A
  sync server is in the tree, authenticates and authorizes when given a public
  key, and is exercised by 477 tests, but it is not yet part of the installed
  package -- see [Self-hosting](#self-hosting).
- **Encryption at rest.** Optional AES-256, applied per page below the engine.

The trade-off worth knowing up front: **one write transaction at a time**, across
all threads and processes. Reads are unaffected and never block. This suits an
application writing its own data; it is not a high-write-concurrency server
database. [ARCHITECTURE.md](ARCHITECTURE.md) explains why.

## Building

Requires CMake 3.25+, a C++20 compiler (GCC 13+ or Clang 18+), OpenSSL and zlib
from your system package manager.

Continuous integration builds and tests on **macOS (Apple clang, arm64)** and
**Linux x86-64 (gcc-13 and clang-18)**, in Debug and Release. A nightly job
compiles for iOS, Android and WASM, and builds on Windows.

Two caveats worth stating rather than implying: the nightly jobs compile but do
not run tests, and the **Android build has sync and encryption disabled**,
because both need OpenSSL and the NDK ships none. Sync and encryption on Android
are unverified.

```sh
git clone --recursive https://github.com/satishbabariya/tessera
cd tessera
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(getconf _NPROCESSORS_ONLN)"
```

`--recursive` fetches Catch2, which the test suites need. Without it the library
still builds and the tests are skipped with a warning.

Nothing is downloaded at configure time. See [how-to-build.md](how-to-build.md)
for platform specifics and cross-compilation.

## Using it

```cmake
find_package(Tessera REQUIRED)
target_link_libraries(myapp PRIVATE Tessera::Storage)
```

```cpp
#include <tessera/engine.hpp>
#include <tessera/history.hpp>

auto db = tessera::DB::create(tessera::make_in_realm_history(), "app.tess");

{
    auto wt = db->start_write();
    if (!wt->has_table("Person")) {
        auto table = wt->add_table("Person");
        table->add_column(tessera::type_String, "name");
        table->add_column(tessera::type_Int, "age");
    }
    auto table = wt->get_table("Person");
    table->create_object()
        .set(table->get_column_key("name"), "Ada")
        .set(table->get_column_key("age"), 36);
    wt->commit();
}

auto rt = db->start_read();
auto table = rt->get_table("Person");
auto results = table->where().greater(table->get_column_key("age"), 30).find_all();
```

## Self-hosting

Sync needs a server, and Tessera's is the one Realm shipped: it lives in
`src/tessera/sync/server/`, is built as a static library, and runs inside
477 passing tests including an in-process client-server round trip.

Both of the things that stood in the way are now done.

**The server authenticates and authorizes, provided you give it a public key.**
It reads the credential the client carries on the WebSocket handshake --
`?baas_at=<token>`, appended by `ClientImpl::Connection::get_http_request_path`
-- verifies it against the key passed to the `Server` constructor, and answers
HTTP 401 before upgrading the connection if the token is missing, malformed,
unverifiable or expired. At BIND it requires `Privilege::Download` for the path
being asked for; at UPLOAD it requires `Privilege::Upload`. Both re-check
expiry, because the handshake can only answer for the moment it ran and sessions
are multiplexed over a connection that may outlive its token.

**A server given no public key authenticates nobody and authorizes nothing.** It
can verify no signature, so it demands no token and stores none, and the checks
above are skipped in their entirety. That is the documented test mode -- see
`Sync_RunServerWithoutPublicKey` -- and it is not a configuration to run
anything real on.

It is installable. `find_package(Tessera)` exports `Tessera::SyncServer`
alongside `Tessera::Storage`, `Tessera::Sync`, `Tessera::Merge`,
`Tessera::ObjectStore` and `Tessera::QueryParser`, and three headers come with
it:

```cpp
#include <tessera/sync/server/server.hpp>   // Server, its Config, start/stop
#include <tessera/sync/server/clock.hpp>    // Config::token_expiration_clock
#include <tessera/sync/server/crypto_server.hpp>  // PKey, for the constructor
```

Three rather than the ten the directory holds. The other seven are the history
format, the file-access cache, the on-disk directory layout and the
access-control internals -- nothing a caller names, and the things most likely
to change. Publishing a header is a promise about it.

`tools/verify/consumer-smoke-test.sh` builds a program outside the tree that
does exactly this: `find_package`, include, construct a `Server`, link. It runs
on every pull request, so the claim on this line is checked rather than
asserted.

And there is a server you can run. `tessera-sync-server` is installed to `bin`
alongside the inspector tools:

```console
$ tessera-sync-server --root /srv/tessera --public-key /etc/tessera/pub.pem
Tessera.Sync.Server - database sync server started ([tessera-0.2.0])
... twenty lines of configuration ...
Tessera.Sync.Server - Listening on ::1:7800 (max backlog is 128, non-TLS)
Tessera - Listening on localhost:7800
```

`SIGINT` or `SIGTERM` stops it cleanly -- the signal is taken by a thread
waiting in `sigwait` rather than by a handler, because `Server::stop()` takes
locks.

It refuses to start without a key unless you say so by name:

```console
$ tessera-sync-server --root /srv/tessera
tessera-sync-server: no --public-key given.

Without one this server cannot verify a signature, so it would accept
every connection, require no token from anyone, and apply no
permissions to any database it serves.

Pass --public-key PATH to authenticate clients, or
--authenticate-nobody if that is genuinely what you want.
```

A keyless server is a real mode and a useful one for tests, but a binary that
entered it silently, on a port, would put back at the command line exactly what
the sections above took out of the server. The smoke test asserts the refusal.

It refuses a second thing for the same reason. A client sends its access token
in the WebSocket URL -- `?baas_at=<token>`, which is how the server
authenticates it at all -- so a connection without TLS carries the credential
across the network in the clear:

```console
$ tessera-sync-server --root /srv/tessera --public-key pub.pem --listen 0.0.0.0
tessera-sync-server: refusing to bind 0.0.0.0 without TLS.

Clients send their access token in the WebSocket URL, so a connection
without TLS puts credentials on the wire in the clear. On loopback that
is a process talking to itself; on 0.0.0.0 it is not.

Pass --tls-cert PATH --tls-key PATH to serve over TLS, or
--allow-cleartext if that is genuinely what you want.
```

`--tls-cert` and `--tls-key` take a PEM certificate chain and its private key,
and are what you want for anything reachable from another machine.

### Issuing a token

The server verifies tokens; `tessera-token` mints them. Generate a key pair,
give the public half to the server and keep the private half wherever you issue
credentials from:

```console
$ openssl genrsa -out private.pem 2048
$ openssl rsa -in private.pem -pubout -out public.pem

$ tessera-token --key private.pem --identity alice \
      --access download,upload --expires-in 86400 --verify public.pem
verified: identity=alice path=<any> expires=1788327484
eyJpZGVudGl0eSI6ImFsaWNlIiwiYWNjZXNzIjpbImRvd25sb2FkIiwidXBsb2FkIl0s...
```

`--path /some/db` restricts a token to one database; without it the token is
valid for every path the server serves. `--verify` runs the result back through
the server's own `AccessControl`, so the tool can say the token was *accepted*
rather than merely printed.

Signing needs a build with the OpenSSL backend. On macOS the default backend is
Apple's Security framework, which does not implement signing and says so;
configure with `-DTESSERA_FORCE_OPENSSL=ON` if you want to mint tokens there.

See [docs/findings/0b-server-has-no-auth.md](docs/findings/0b-server-has-no-auth.md)
for what was missing and
[docs/findings/0b-keyless-still-demanded-a-token.md](docs/findings/0b-keyless-still-demanded-a-token.md)
for why the keyless case is a branch of its own.

So "self-hostable" now means what it says: install the package, run
`tessera-sync-server`, and you have your own sync server, with no cloud account
and no vendor. Link `Tessera::SyncServer` instead if you would rather embed it.

## The two API tiers

```cpp
#include <tessera/api.hpp>      // schemas, typed objects, live results, notifications
#include <tessera/engine.hpp>   // DB, Transaction, Table, Query, Obj
```

Tier 1 (`api.hpp`) is the high-level API most applications want: declare a schema,
work with typed objects, subscribe to changes. Tier 2 (`engine.hpp`) is direct
access to the storage engine for binding authors and anyone with unusual needs.

Both are supported and stable from v1.0, and both can be used in the same program.
Anything not reachable from these two headers carries no stability promise.

## Documentation

| | |
|---|---|
| [ARCHITECTURE.md](ARCHITECTURE.md) | How the engine works: storage layout, MVCC, sync |
| [FORK.md](FORK.md) | Lineage, licensing, and what changed from Realm |
| [how-to-build.md](how-to-build.md) | Platform-specific build instructions |
| [docs/protocol.md](docs/protocol.md) | The sync wire protocol, message by message |
| [docs/algebra_of_changesets.md](docs/algebra_of_changesets.md) | Why concurrent edits converge |
| [CONTRIBUTING.md](CONTRIBUTING.md) | How to contribute |

## Licence

Apache 2.0. See [LICENSE](LICENSE) and [THIRD-PARTY-NOTICES](THIRD-PARTY-NOTICES).

Copyright notices from Realm Inc. are retained throughout, as Apache 2.0 §4(b)
requires. Do not remove them.
