# The sync server is not in the package

The README's headline list said:

> **Convergent sync.** A production-tested operational-transform engine and a
> self-hostable sync server, both included. No cloud account, no vendor.

The first half is true. `Tessera::Merge` and `Tessera::Sync` install, export and
link. The second half is not, for anyone who installs the package.

    $ grep -oE 'Tessera::[A-Za-z0-9_]+' <prefix>/share/cmake/Tessera/*.cmake | sort -u
    Tessera::Merge
    Tessera::ObjectStore
    Tessera::QueryParser
    Tessera::Storage
    Tessera::Sync

There is no `Tessera::SyncServer`. No server header is installed. No server
executable exists anywhere in the project -- the only `main()` functions under
`sync/` belong to four inspection tools.

The server is real: `SyncServer` is a static library built from
`src/tessera/sync/noinst/server/`, and 461 sync tests run against it, including
in-process client-server round trips. It has simply never had an `install()`
rule and is not in the export set. It exists inside the build tree and nowhere
else.

## Why nothing caught it

`noinst` means "not installed", and the directory has been called that since
upstream. The name was accurate. What was inaccurate was the README, written
against the repository rather than against the artefact.

Every gate in the project agreed with the README, because every gate looks at
the tree:

* the test suites link `SyncServer` directly as an in-tree target;
* `check-header-tiers.sh` checks that private headers are *not* installed, so
  the server headers' absence is a pass, not a finding;
* `consumer-smoke-test.sh` installs the package and builds against it, but it
  only ever asked for `Tessera::Storage`. A package exporting one usable target
  would have passed it.

This is the same shape as everything else in these findings: the property being
checked sat next to the property being claimed. "The tests pass" and "the tree
builds" are both true and neither is "a stranger can run a sync server".

## What changed

The README now says what the package contains, and a `Self-hosting` section
states plainly that the server is not installable and why.

`consumer-smoke-test.sh` now asserts the exported target set against a literal
list, and the README names the same list. The list is written out rather than
derived: a derived list agrees with whatever the package happens to export,
which is the thing under test. Canary-tested by adding `SyncServer` to the
expected set and confirming the failure.

The smoke test also now compiles `<tessera/api.hpp>` and `<tessera/engine.hpp>`
in a translation unit of their own. The README calls these the public API --
"anything not reachable from these two headers carries no stability promise" --
but the existing consumer included `db.hpp`, `table.hpp` and `transaction.hpp`
directly, so nothing had ever verified that the two documented headers were
installed or self-contained. They are.

## What making it installable would take

Two things, both interface decisions rather than mechanical work.

The headers include each other as `<tessera/sync/noinst/server/...>`, so they
could be installed at that path unchanged -- but `noinst` in a public include
path is a contradiction, and renaming the directory fixes a public interface.

More substantially, the server still expects an App Services access token. It
requires a parseable signed JWT on every bind, and `AccessControl` will parse a
token without verifying its signature when no public key is configured, in a
branch the source comments describe as being for testing. A self-hostable server
needs an authentication model this project owns.

Both belong to the next milestone. They are the milestone.
