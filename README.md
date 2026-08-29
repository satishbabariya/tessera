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
  self-hostable sync server, both included. No cloud account, no vendor.
- **Encryption at rest.** Optional AES-256, applied per page below the engine.

The trade-off worth knowing up front: **one write transaction at a time**, across
all threads and processes. Reads are unaffected and never block. This suits an
application writing its own data; it is not a high-write-concurrency server
database. [ARCHITECTURE.md](ARCHITECTURE.md) explains why.

## Building

Requires CMake 3.25+, a C++20 compiler (GCC 13+ or Clang 18+), OpenSSL and zlib
from your system package manager.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

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
    auto table = wt->add_table("Person");
    auto name = table->add_column(tessera::type_String, "name");
    auto age  = table->add_column(tessera::type_Int, "age");
    table->create_object().set(name, "Ada").set(age, 36);
    wt->commit();
}

auto rt = db->start_read();
auto table = rt->get_table("Person");
auto results = table->where().greater(table->get_column_key("age"), 30).find_all();
```

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
| [doc/protocol.md](doc/protocol.md) | The sync wire protocol, message by message |
| [doc/algebra_of_changesets.md](doc/algebra_of_changesets.md) | Why concurrent edits converge |
| [CONTRIBUTING.md](CONTRIBUTING.md) | How to contribute |

## Licence

Apache 2.0. See [LICENSE](LICENSE) and [THIRD-PARTY-NOTICES](THIRD-PARTY-NOTICES).

Copyright notices from Realm Inc. are retained throughout, as Apache 2.0 §4(b)
requires. Do not remove them.
