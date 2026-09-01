# The server was private by directory name

Tessera's sync server was never unfinished. It ran, it passed 477 tests, and
after #29-#35 it authenticated and authorized. What kept it out of the installed
package was a directory called `noinst`.

That name is not decoration. `tools/check-header-tiers.sh` enforces it: no
public header may include anything under a path containing `noinst/`. So the
server could not be published without moving, and moving it means choosing what
its public surface is -- which is the part that deserved the delay.

## Three headers, not ten

The directory holds ten headers. Installing all ten would have been the easy
reading of "make the server installable", and would have published the history
format, the file-access cache, the on-disk directory layout and the
access-control internals -- four things no caller names and all four things most
likely to change.

The public closure of `server.hpp` is exactly two other headers:

| header | why it is public |
|---|---|
| `server.hpp` | the API: `Server`, its `Config`, `start`/`stop` |
| `clock.hpp` | `Config::token_expiration_clock` is a `Clock*` |
| `crypto_server.hpp` | the constructor takes `util::Optional<PKey>` |

Everything `server.hpp` needs from outside the directory -- `util/logger.hpp`,
`util/optional.hpp`, `sync/client.hpp`, `sync/network/network.hpp`,
`binary_data.hpp`, `util/buffer.hpp` -- was already installed, checked against a
real `cmake --install` rather than assumed.

## The header that did not need promoting

`server.hpp` included `<tessera/util/time.hpp>`, which sits in
`TESSERA_NOINST_HEADERS` and is deliberately not installed. That looked like a
forced choice: either promote a general-purpose time utility into the public API
to satisfy one include, or leave the server unshippable.

It was neither. The include is unused. `milliseconds_type` comes from
`sync/protocol.hpp`; `Clock` comes from `clock.hpp`; nothing in `server.hpp`
refers to `localtime`, `gmtime`, `put_time` or `format_local_time`. Deleting it
and rebuilding `SyncServer`, `SyncTests` and `ObjectStoreTests` settled it in
about a minute.

The README had already been written claiming the promotion would be necessary.
It was corrected before it merged. An unused include in a header that nobody
installs costs nothing and is invisible; the moment the header becomes public it
turns into an argument for widening the public API, and the argument is wrong.

## What proves it works

`tools/verify/consumer-smoke-test.sh` builds a program outside the tree that
calls `find_package(Tessera)`, includes `<tessera/sync/server/server.hpp>`,
constructs a `Server` and links `Tessera::SyncServer`. It constructs but does not
start: starting binds a port, and a merge gate that fails when a port is busy
teaches everyone to ignore it.

The check's target list is hardcoded, deliberately, so that exporting a new
target fails until someone updates both the list and the README section that
names it. It did:

```
FAIL: exported targets changed
  expected: Merge ObjectStore QueryParser Storage Sync SyncServer
  actual:   Merge ObjectStore QueryParser Storage Sync
```

## A canary that did not fire, and why

The first attempt to prove that check works disabled the install rule by
renaming the target to `SyncServer_disabled`. CMake refused to configure --
`install TARGETS given target "SyncServer_disabled" which does not exist` -- so
the build directory kept its previously generated install scripts, the smoke
test installed the *correct* package, and passed.

For a moment that read as "the check is weak". It was the canary that never
applied. The configure error was also invisible, because the command piped
`cmake` into `tail` and reported `$?` from `tail`.

Commenting out both install rules configures cleanly and fires it properly. The
lesson is the one this project keeps relearning in new costumes: a check that
passes tells you nothing until you have watched it fail, and a canary is itself
a thing that can silently not run.
