# Changelog

## Unreleased

Nothing yet.

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
* Verified on macOS/arm64 only. Linux and Windows are expected to work but are
  not yet covered by CI.
