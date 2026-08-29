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

## Addendum: error messages are a contract the tests assert

Rewriting the 80 user-facing messages in `src/` broke **178 assertions** across
the object-store suite, because the tests assert exact message text:

```
REQUIRE_EXCEPTION(dict.verify_attached(), InvalidatedObject,
    "Dictionary is no longer valid. Either the parent object was deleted "
    "or the containing Realm has been invalidated or closed.");
```

Production now says "containing database"; the test still expected "containing
Realm". One side of an assertion pair had changed.

87 test expectation strings needed the identical substitution list. The sweep
should have covered `src/` and `test/` in a single pass from the start -- an
error message is an interface, and the test is its consumer.

## Addendum: a stale binary reported a passing run

While chasing the above, an ObjectStoreTests run reported "343 test cases
passed" with **53,540 assertions**, against a baseline of roughly 70,000. Same
test count, 30% fewer assertions.

The binary predated the changes being verified, because the preceding build had
used `--target CoreTests` rather than building everything. The test count looked
right, so the run looked green.

**Compare assertion counts, not only test counts.** A suite whose test count is
unchanged but whose assertion count has moved substantially is usually not
running the code you think it is.

This is the fourth distinct way verification proved too narrow in this project,
and they rhyme:

| Narrow gate | Missed |
|---|---|
| one suite | broken tests in the other two |
| one target | the command-line tools |
| one configuration | a Release-only crash behind an `#ifdef` |
| a stale binary | the changes under test |

The defence is the same each time: `cmake --build` with no target argument, then
every suite, in both configurations, and confirm the numbers moved the way the
change predicts.

## Addendum: some strings are length-sensitive, and no pattern can tell

The message sweep broke `HTTPParser_ChunkedEncoding`, which tests HTTP chunked
transfer encoding. Its fixture embeds each chunk's length in hex:

```
"7\r\nMongoDB\r\n8\r\n Realm i\r\nB\r\n..."
       └ 7 bytes    └ 8 bytes
```

The substitution rewrote ` Realm i` (8 bytes) as ` database i` (11 bytes) while
leaving the `8` that declares its length. The string reads as prose about
MongoDB Realm. It is protocol test data with embedded byte counts.

Two more fixtures had the same shape: `test_index_string.cpp` and
`test_query2.cpp` contain a string that self-describes as "around 90 bytes long,
which falls in the long-string type", used to exercise behaviour at a size
threshold. Substitution lengthened it by three bytes -- harmless here, but the
same class of risk.

All three were reverted rather than adjusted. De-branding invisible test fixture
data gains nothing, and the strings' *content* is arbitrary while their *length*
is not.

**No refinement of the pattern would have caught this.** The only thing that
could was running the tests, which is what makes a mass substitution safe to
attempt at all.

## The complete taxonomy

Six characters, five correct treatments, distinguishable only by what the string
*is* and never by what it looks like:

| Occurrence | What it is | Treatment |
|---|---|---|
| `"read-only Realm"` | prose in an error message | rewrite -- and update the tests asserting it |
| `"Local in-Realm"` | a display value | rewrite |
| `"Realm.Storage"` | log-category **API** | rewrite, and update every caller |
| `"Realm"` in subscription tests | row **data** | leave |
| `"8\r\n Realm i"` | **length-prefixed protocol data** | leave -- rewriting corrupts the encoding |
