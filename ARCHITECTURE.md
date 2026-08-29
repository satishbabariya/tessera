# Tessera architecture

## What it is

An embedded database: a library that runs inside your process and stores data in
a single file. There is no server, no daemon and no socket. Opening a database is
opening a file.

Tessera is a fork of Realm Core. The storage engine described below is Realm's
work; see [FORK.md](FORK.md) for lineage and licensing.

## Libraries

Six static libraries, strictly layered. Nothing includes upward.

```
    tessera-objectstore     schemas, typed objects, live results, notifications
            |
    tessera-sync            client sessions, wire protocol, transport
    tessera-server          the bundled sync server
            |
    tessera-merge           changesets, instructions, operational transform
            |
    tessera-storage         allocator, cluster tree, tables, queries, indexes
            |
    tessera-parser          the query language
```

| Library | Source LOC | Contents |
|---|---|---|
| `tessera-storage` | ~80,700 | The engine: allocator, B+-trees, cluster tree, arrays, tables, queries, indexes, transactions |
| `tessera-util` | ~22,200 | Files, mmap, encryption, logging, futures, networking primitives |
| `tessera-objectstore` | ~13,700 | Schema management, typed object accessors, results, change notifications |
| `tessera-merge` | ~11,100 | Convergent merge: changesets, instructions, operational transform |
| `tessera-sync` | ~6,100 | Sync sessions, protocol, client history |
| `tessera-parser` | ~3,500 | Query language parser (Bison/Flex) |

`tessera-merge` depends on `tessera-storage` and nothing else, enforced by
`tools/check-merge-deps.sh`. It can be used on its own by anything that needs
convergent merge over structured data.

## The storage engine

### Copy-on-write, memory-mapped, no write-ahead log

The database file is memory-mapped. Readers walk it directly -- there is no
deserialisation step, no row materialisation, no buffer pool. A query returns a
lazy view over mapped pages.

Writes never modify existing data. A commit allocates new nodes, writes them, and
finally swaps a single **top ref** -- the pointer to the new root -- in the file
header. That swap is the commit.

The consequence is that crash safety is structural rather than bolted on. If the
process dies mid-write, the top ref still points at the last complete state, and
the partially written nodes are unreferenced garbage. There is no WAL to replay
and no recovery pass on open.

### MVCC without locking readers

Each committed state is a complete, immutable tree. A reader pins a version and
sees a consistent snapshot for as long as it holds it, unaffected by concurrent
writers.

Version bookkeeping lives in shared memory alongside the file, so readers in
different *processes* see the same version list. Reader registration is
lock-free.

### One writer at a time

Writes are serialised by a single interprocess mutex (`db.hpp`). One write
transaction proceeds at a time, across all threads and all processes touching
the file.

This is a real limit and worth stating plainly: write throughput does not scale
with cores. It suits the workload the engine was built for -- an application
writing its own data locally -- and it is what makes the copy-on-write design
tractable, since there is only ever one tree being built. Lifting it means
replacing the allocator and commit protocol, which is a much larger project than
it sounds.

Reads do not contend with writes or with each other.

### Storage layout

Objects live in **clusters** -- row groups in a B+-tree keyed by object key.
Within a cluster, each column is a separate array. So the layout is row-grouped
but column-wise inside the group, which gives locality for whole-object access
and scannability for single-column queries.

Integer arrays are bit-packed to the narrowest width that fits their contents, so
a column of small numbers costs a few bits per value. Decompression is not a
separate step: the width is a property of the array and reads shift directly.

### Encryption

Optional AES-256 encryption is applied per page in the memory-mapping layer, so
the engine above it is unaware of it. Pages are decrypted on fault and re-encrypted
on write.

## The file format

| | |
|---|---|
| Magic mnemonic | `TESS` at offset 16 |
| Format version | 1 |
| Accepts | version 1 only |

Tessera rejects any file whose format is not its own. It does not upgrade older
formats, including Realm's. See [FORK.md](FORK.md).

The `.tess` extension is a convention, not a requirement; the magic bytes are
what identify a file.

## Sync

Two halves, deliberately separable.

**The merge engine** (`tessera-merge`) is where convergence happens. Concurrent
edits become changesets of per-field instructions over stable object identifiers,
and operational transform rewrites concurrent changesets against each other so
that applying them in any order reaches the same state. The formal argument is in
[docs/algebra_of_changesets.md](docs/algebra_of_changesets.md).

**The protocol and server** (`tessera-sync`, `tessera-server`) carry changesets
between peers. The wire protocol is specified in
[docs/protocol.md](docs/protocol.md) -- 1,126 lines, message by message.

The server has no cloud dependency, and 461 sync tests run against it, including
randomised concurrent-edit convergence (`Transform_Randomized`) and multi-client
convergence (`ClientReset_ThreeClients`).

It is not in the installed package. `SyncServer` is built as a static library
from `src/tessera/sync/noinst/server/`, has no `install()` rule and is not in the
export set, so `find_package(Tessera)` offers no `Tessera::SyncServer` and there
is no server executable anywhere in the project. Running one means building from
source and linking the in-tree target. See the Self-hosting section of
[README.md](README.md) for what changing that requires.

## The public API

Two supported surfaces, both stable from v1.0:

```cpp
#include <tessera/api.hpp>      // tier 1: schemas, objects, results, notifications
#include <tessera/engine.hpp>   // tier 2: DB, Transaction, Table, Query, Obj
```

Tier 1 is what most applications want. Tier 2 is direct engine access for binding
authors and unusual requirements. They compose; use both in one program.

Anything not reachable from these two headers carries no stability promise.
`tools/check-header-tiers.sh` enforces the boundary. Six `impl/` headers are
reachable today because core engine headers require them -- documented in
`docs/findings/0b-header-tiers.md`, capped so the exposure cannot grow.

## Enforced invariants

Structural decisions are executable checks rather than prose, each canary-tested
to confirm it can actually fail:

| Check | Invariant |
|---|---|
| `tools/check-copyright-notices.sh` | Apache 2.0 §4(b) attribution survives every tree-wide edit |
| `tools/check-no-vendor-hosts.sh` | the build fetches nothing from hosts we do not control |
| `tools/check-layering.sh` | no upward includes between layers |
| `tools/check-merge-deps.sh` | `tessera-merge` depends only on storage |
| `tools/check-header-tiers.sh` | the public API does not leak private headers |
| `tools/check-repo-hygiene.sh` | no runtime artefacts, stray keys or dead references in the tree |
| `tools/check-rename-residue.sh` | no pre-rename identifiers, and nothing still names the project `realm` to the outside |
| `tools/check-tests-compiled.sh` | every test source is in a `CMakeLists`, so no test file is silently absent from the build |
| `tools/check-cert-expiry.sh` | the test certificates are neither near expiry nor over Apple's 825-day ceiling |
| `tools/verify/consumer-smoke-test.sh` | the installed package is consumable and exports exactly the documented target set |
| `tools/verify/clean-clone-test.sh` | a fresh clone configures and builds |

Two claims this file makes are checked by tests rather than by scripts. That
Tessera opens no other file format, and that an encrypted file does not contain
its plaintext, are both in `test/test_file_format.cpp` -- each one canary-tested
against a deliberately broken engine, because a test that cannot fail is worth as
little as a check that cannot fail.

## Further reading

- [docs/protocol.md](docs/protocol.md) -- the sync wire protocol
- [docs/algebra_of_changesets.md](docs/algebra_of_changesets.md) -- merge correctness
- [docs/changeset.md](docs/changeset.md) -- changeset wire format
- [docs/primer/primer_architecture.md](docs/primer/primer_architecture.md) -- Realm-era architecture primer
- `docs/findings/` -- decisions made during the fork, with evidence
