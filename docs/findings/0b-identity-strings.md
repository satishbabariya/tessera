# The rename changed the code and not what the code says

Phase 0b renamed the namespace, the macro prefix, the library, the CMake package
and the file format. `tools/check-rename-residue.sh` reported clean afterwards
and has reported clean since.

It scanned for `namespace realm`, `realm::`, `REALM_` and `#include <realm/>` --
four patterns, all of them code identifiers. Every one of those is enforced by
the compiler. A survivor does not build.

The residue that does build was never looked at.

## What was still there

`src/tessera/version.hpp` defined the product name:

    #define TESSERA_PRODUCT_NAME "realm-core"
    #define TESSERA_VER_CHUNK "[" TESSERA_PRODUCT_NAME "-" TESSERA_VERSION_STRING "]"

`TESSERA_VER_CHUNK` is printed by `terminate.cpp` when the process aborts, and
logged by both the sync client and the sync server at startup. A crash in
Tessera reported itself as `[realm-core-...]`.

Three things went out over the network:

| | |
|---|---|
| `sync_client.hpp` | the `User-Agent` on every sync connection: `RealmSync/<version> ...` |
| `server.cpp` | the HTTP `Server` header the sync server returns to every client |
| `websocket.cpp` | the WebSocket subprotocol echoed to a client that offered none: `realm.io` |

The last is the second wire identifier this fork has had to correct. The first,
`com.mongodb.realm-sync#`, is recorded in [0b-rename-blind-spots.md](0b-rename-blind-spots.md).
Both were invisible to the test suite for the same reason: client and server are
built from one tree, so both sides agree on whatever the string happens to be.
Only a foreign peer disagrees, and there is no foreign peer in CI.

Five error categories named themselves `realm.*` -- `realm.basic_system`,
`realm.util.misc_ext`, `realm.simulated_failure`, `realm.sync.network.resolve`,
`realm.sync.network.ssl`. These reach a user through
`std::error_code::category().name()` in formatted messages.

Six command-line tools install as `tessera-trawler`, `tessera-enumerate`,
`tessera-encrypt`, `tessera-decrypt`, `tessera-dump` and `tessera2json`, and
each one's `--help` told the user to run `realm-trawler`, `realm-enumerate`, and
so on. The CMake targets producing them were still called `RealmTrawler` and
`Realm2JSON`; only the `OUTPUT_NAME` property had been renamed. `how-to-build.md`
correspondingly documented `cmake --build . --target RealmTrawler`.

Coordination resources created next to a database file, and in the Windows
`Local\` namespace, were named `realm_<hash>.cv`,
`Local\realm_named_intermutex_`, `Local\realm_cv_mutex_` and `realm_<hash>.note`.

And the symbol deliberately planted in crash backtraces to tell a user where to
report the crash was `please_report_this_issue_in_github_realm_realm_core_v14`.

## Why the check could not see any of it

The check looked for identifiers. All of this is string literals. The two
categories fail differently and only one of them fails at all: an identifier
that survives a rename stops the build, and a string that survives it is emitted
at runtime by a binary whose tests all pass.

`tools/check-rename-residue.sh` now runs a second scan over identity-bearing
literals -- the patterns by which the project names itself to something outside
itself. It found the `User-Agent` string that a manual search for
`user_agent` had already missed.

## What was deliberately left

The connection URL scheme is still `realm://` and `realms://`. Renaming it is a
change to public configuration, the obvious spelling (`tesseras://`) is poor, and
it belongs with the rest of the protocol work rather than with a string sweep.

`mongodb-realm/` and `realm-object-server` are directory layouts written by App
Services that Tessera reads and never creates. They name a foreign thing, so
they are correct as they stand.

Prose that merely contains the word -- "Cannot change primary key property when
realm is synchronized" -- is not matched by the scan. The public class is still
called `Realm`, and renaming it is roughly 2,600 sites and a separate decision.

Each exclusion is in the check's allowlist with its reason, so the next person
to see one of these strings learns why it is there instead of assuming it was
missed.
