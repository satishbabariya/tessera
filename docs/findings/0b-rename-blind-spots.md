# Finding: what a code-identifier rename cannot see

Date: 2026-08-29
Task: Phase 0b, post-rename sweep

The Phase 0b rename matched namespaces, macros, include paths and the file
extension: roughly 12,000 occurrences across 721 files, verified by a residue
check and a full test gate. It was correct for what it matched, and it matched
only *code identifiers*.

Six categories fell outside that, found only by looking for them deliberately.
Each is listed with how it would have reached a user.

## 1. User-facing prose in string literals — 80 strings

```
"Can't compact a read-only Realm"
"A user must be provided to open a synchronized Realm."
"Realm file at path '%1' has history type '%2'"
```

Error messages, exception text and CLI help. No `.realm`, no `realm::`, no
`REALM_` — nothing for the rename to match. Every one is what a Tessera user
would read when something went wrong.

Rewritten to name the thing rather than a brand: *"Can't compact a read-only
database"*. That is better copy independent of the fork.

## 2. A wire-protocol identifier — silently corrupted

The `.realm` → `.tess` substitution changed the WebSocket subprotocol from
`com.mongodb.realm-sync#` to `com.mongodb.tess-sync#`.

**Every test passed throughout.** Client and server are built from the same tree,
so both sides of the handshake changed together. A test suite where both peers
come from one source cannot detect a consistently wrong wire identifier; only an
external peer would fail, and nothing simulates one.

Now deliberately `io.tessera.sync#` and `io.tessera.query-sync#`, with the
server's URL routes matched. A clean-break fork with no deployed peers is the one
moment a protocol break is free, and a project that is not MongoDB's should not
advertise `com.mongodb` on the wire.

## 3. A macOS Keychain item name

`CFSTR("%@ - Realm Sync Metadata Key")` — the name a user sees in Keychain
Access. Untouched by the rename; now "Tessera Sync Metadata Key".

## 4. The root log category — API, not text

`LogCategory::realm("Realm", nullptr)` roots every category path, so users
filtering logs write `Realm.Storage.Transaction`. Renaming it to `Tessera` is a
deliberate API change.

It also produced the sharpest failure of the sweep. `get_category` uses
`map::at()`, which **throws on a missing key**, so a stale `"Realm"` lookup did
not fail a test — it aborted the entire suite before any test ran:

```
libc++abi: terminating due to uncaught exception of type
std::out_of_range: map::at: key not found
```

The first fix matched `"Realm.` *with a trailing dot* and caught the child paths.
It missed the bare root `"Realm"` in `test_table.cpp`. One character of pattern,
one crashed suite.

## 5. Comments describing history — the rename made documentation lie

Two comments were rewritten into false claims about the past:

- `protocol.hpp` said protocol version 1 matched `io.realm.sync-30`. The rename
  changed it to `io.tessera.sync-30`, an identifier that never existed.
- `keychain_helper.cpp` described where Realm *used to* store metadata keys. The
  rename reassigned that history to Tessera.

Both restored. **A mass rename applied to prose makes documentation assert things
that never happened**, and unlike a broken identifier nothing will ever fail
because of it.

## 6. Bare identifiers in build files

`CPACK_PACKAGE_NAME "realm-${BUILD_TYPE}"`, target names `realm-benchmark-*`,
`add_library(realm-parser ALIAS ...)`, `install(EXPORT realm ...)`,
`PACKAGE_NAME: realm-core`. Neither strings nor paths nor namespaces. The export
set surfaced only when a *new* target had to join it — adding a component during
a rename tests the rename, because new code written in the target vocabulary
collides with whatever was missed.

## The distinction that mattered throughout

Strings that look identical needed opposite treatment, and only the call site
distinguished them:

| String | What it is | Treatment |
|---|---|---|
| `"Can't compact a read-only Realm"` | prose | rewrite |
| `"Local in-Realm"` | display value for a history type | rewrite, to "Local in-file" |
| `"Realm.Storage"` | **API** — a log category path | rewrite, and update every caller |
| `"Realm"` in `test_sync_subscriptions.cpp` | **test data** — a row value | leave |

The tests were what told them apart. Values nothing references can change
freely; values a test asserts are a contract.

## The transferable rule

A rename is not finished when the code compiles and the tests pass. Those verify
identifiers. The things that reach a user — messages, wire formats, stored
identifiers, log categories, documentation — are mostly invisible to both, and
the wire identifier in particular passed every gate this project has while being
wrong.

After the mechanical rename, grep for the old name in string literals, in build
files, and in comments, and classify each hit by what it *is* rather than what it
looks like.
