# Changelog

## Unreleased

### Fixed

* The library called itself `realm-core`. `TESSERA_PRODUCT_NAME` still held the
  old name, so crash reports and the sync client and server startup logs all
  identified the process as `[realm-core-<version>]`.
* Three identifiers went out over the network unchanged: the `User-Agent` on
  every sync connection and the HTTP `Server` header both read `RealmSync/`, and
  the sync server echoed `realm.io` as the negotiated WebSocket subprotocol to a
  client that offered none. Client and server are built from one tree, so no test
  could observe any of them.
* Five error categories reported themselves as `realm.*` through
  `std::error_code::category().name()`.
* The six command-line tools install as `tessera-*` but their `--help` told the
  user to run `realm-trawler`, `realm2json` and so on. Their CMake targets were
  also still named `RealmTrawler`, `Realm2JSON` and so on.
* Cross-process coordination files and Windows named objects used a `realm_`
  prefix.
* The symbol planted in crash backtraces to direct users where to report pointed
  at `github/realm/realm-core`.
* The README claimed "a self-hostable sync server, included". The installed
  package exports no `Tessera::SyncServer` target, installs no server header, and
  the project contains no server executable. The server is real and 461 tests run
  against it, but it exists only inside the build tree. The README now says so,
  and a new `Self-hosting` section explains what making it installable requires.

### Added

* `consumer-smoke-test.sh` asserts the package's exported target set against a
  literal list, which the README names as well, and compiles `<tessera/api.hpp>`
  and `<tessera/engine.hpp>` on their own. The README calls those two the public
  API, but the smoke test had only ever included `db.hpp` and friends, so nothing
  verified they were installed or self-contained.

## 0.1.1 — 2026-08-29

### Added

* Build-and-test CI on Linux (gcc-13, clang-18) and macOS, in Debug and
  Release. Until now only a changelog bot had ever run, so every claim the
  project made about itself was macOS/arm64 only.

### Fixed

* The installed CMake package no longer exports `-fuse-ld=lld` (or `gold`).
  Tessera selects a faster linker for its own build, and that flag was applied
  as a plain `PUBLIC` link option, so it was baked into the exported target: a
  consumer inherited whichever linker happened to exist on the machine that
  built Tessera. Building Tessera with clang and consuming it with gcc failed
  with `collect2: fatal error: cannot find 'ld'`. Affects v0.1.0 when built on
  a system where lld or gold is available, which does not include macOS.

## 0.1.0 — 2026-08-29

### The fork

First release of Tessera, forked from
[realm-core](https://github.com/realm/realm-core) v14.14.0 (`f8752e180`).
See [FORK.md](FORK.md) for lineage and licensing.

**This release is about removal and clarification, not new features.** The
storage engine is Realm's, lightly modernised. What changed is everything around
it.

#### New identity and file format

* The project, namespace (`tessera::`), macro prefix (`TESSERA_`), library names
  and CMake package are renamed. Apache 2.0 grants no trademark rights, so this
  was a licence requirement rather than a preference.
* New file format: magic mnemonic `TESS`, format version 1. **Tessera cannot open
  Realm files** and rejects any format that is not its own rather than upgrading
  it. Export with Realm and import with Tessera if you need to migrate.
* All 560 upstream copyright notices are retained, as Apache 2.0 §4(b) requires.

#### A documented, stable public API

* Two supported surfaces: `<tessera/api.hpp>` for the high-level API and
  `<tessera/engine.hpp>` for direct engine access. Both stable from v1.0.
* The README no longer disclaims having a stable public API.
* The installed CMake package is verified consumable end to end: `find_package`,
  link, include, run.

#### The merge engine is a standalone library

* `tessera-merge` — changesets, instructions and operational transform — is
  carved out of the sync monolith and depends only on the storage engine. It was
  previously impossible to use without a websocket stack.

#### The bundled sync server is a first-class target

* The self-hostable sync server at `sync/noinst/server/` was marked
  `EXCLUDE_FROM_ALL` upstream and never built by default. It builds and ships
  now. 461 sync tests pass against it, including randomised concurrent-edit
  convergence.

#### No vendor dependencies

* The build no longer downloads prebuilt OpenSSL or cross-compile toolchains
  from `static.realm.io`, and neither do downstream consumers of the installed
  package. Dependencies come from the environment.
* Removed: Atlas App Services (App, MongoDB remote access, push notifications,
  JWT auth), the Atlas-coupled audit backend, the BAAS test infrastructure, the
  generated C API and its binding generator, MongoDB's CI configuration, and the
  file-format upgrade machinery for versions 10 through 23.

#### Toolchain

* C++20 (from C++17), CMake 3.25+ (from 3.22), GCC 13+ / Clang 18+.
* The upstream tree did not compile on a 2026 toolchain; three fixes were needed.

#### Known limitations

* **One write transaction at a time**, across all threads and processes. Reads
  are unaffected. See [ARCHITECTURE.md](ARCHITECTURE.md).
* Flexible sync (FLX) remains in the tree but is unused and unsupported, pending
  removal.
* The `Realm` class name is unchanged inside `namespace tessera`; renaming it is
  an API decision deferred to a later release.
* Verified by CI on macOS (Apple clang, arm64) and Linux x86-64 (gcc-13 and
  clang-18), in Debug and Release. Windows, Linux on ARM, iOS, Android and WASM
  are not verified.
