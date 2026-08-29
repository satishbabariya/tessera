# Changelog

## Unreleased

### Added

* `test/test_util_enum.cpp` is now in the build. It was in no `CMakeLists.txt`,
  compiled into no target and had never run, though it covers
  `tessera/util/enum.hpp`, which is installed as public API. It compiled and
  passed unchanged.
* `tools/check-test-sources-listed.sh`, run in CI, fails if any test source is
  not named by the `CMakeLists.txt` that should compile it. It establishes that a
  file is visible to the build, not that any particular configuration compiles it
  -- 26 test files here are correctly conditional on `TESSERA_ENABLE_SYNC`. A test file left out
  of the build does not fail, does not appear as skipped and does not break
  anything, and the suite total is no help because nobody knows what it should
  be.
* `tools/analyse-zero-check-tests.sh` reports tests that run and execute no
  checks. Analysis rather than a gate: of 106 such tests, 105 are regression
  tests that assert by not crashing.

* `test/test_file_format.cpp`. README.md and ARCHITECTURE.md both state that
  Tessera rejects any file whose format is not its own, which is the fork's
  central promise, and nothing tested it. Five tests write a real database, patch
  its 24-byte header on disk and reopen: a `T-DB` mnemonic is refused, so is any
  other, format versions 2, 10, 24 and 255 are refused, and the error names the
  version it rejected. Each was confirmed to fail against a deliberately broken
  engine.
* Four more in the same file for README's other claim about the bytes on disk,
  "Encryption at rest. Optional AES-256, applied per page below the engine".
  `test_encrypted_file_mapping.cpp` covers the cryptor, page IVs and concurrent
  mappings in thirteen tests, none of which answers whether the data is on the
  disk in the clear. These write a distinctive string, confirm it is findable in
  an unencrypted file and absent from an encrypted one -- along with the table
  name, since "below the engine" means the engine's own structures too -- and
  check that an encrypted file is refused without the key and with a key that is
  one byte wrong.

### Fixed

* `DB::upgrade_history_schema` calls `start_write()`, which is annotated
  `REQUIRES(!m_mutex)`, and carried no annotation of its own. Clang's
  thread-safety analysis could not prove the caller does not hold the lock and
  warned at the call site. It is annotated now, so the contract is documented and
  the compiler enforces it -- adding the annotation produced no new warning at
  the caller, which is the evidence that the lock is genuinely not held there.
* The `SynchronousTestTransport` barrier in `sync_test_utils.hpp` acquires a lock
  in `block()` and releases it in `unblock()`, which clang's analysis cannot
  follow across functions. It produced two warnings per translation unit, 22 of
  the 23 `-Wthread-safety-analysis` warnings in a full build, and that noise is
  why the one real warning was not visible. Both are annotated
  `NO_THREAD_SAFETY_ANALYSIS` with the reason.

### Fixed

* `Shared_RobustAgainstDeathDuringWrite`, the only test of the crash-safety
  claim, had never executed its body. Three guards made that impossible:
  `!TESSERA_ENABLE_ENCRYPTION` excluded every configuration built or tested here,
  `TESSERA_PLATFORM_APPLE` admitted Apple only, and the runtime check requires
  robust POSIX mutexes, which Apple does not provide. Compiled in with encryption
  off it reported "All 1 tests passed (0 checks)" in 20 microseconds. It now
  compiles wherever `fork()` exists and is gated on the runtime capability check
  alone.
* That test now uses `TEST_IF` rather than returning early, so the framework
  reports it as excluded where it does not apply instead of as a pass over zero
  checks, and is `NONCONCURRENT`, because it calls `fork()` from a runner full of
  worker threads and then does real work in the child.
* The CI build job had no `timeout-minutes`. A dead-locking test would have held
  a runner for GitHub's six-hour default.

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
* The TLS certificates the SSL tests use expired on 25 October 2026. They are
  re-issued for 825 days from the existing keys and signing CA. Since the tests
  complete a real handshake against them, the whole suite would have begun
  failing on every platform at once, reported as certificate-verification errors
  inside tests named for the socket behaviour they cover.
* `Sync_SSL_Certificate_Verify_Callback_2` and `_3` asserted the contents of the
  test certificates by pinning `pem_size` and individual base64 characters
  (`pem_data[1667] == 'J'`). Re-issuing a certificate changes every base64 byte
  while every length stays the same, so the tests broke on one character and
  never said which certificate had arrived. They now compare against the
  certificate the server was configured with.
* Three test executables share one `resources/` directory and each ran its own
  POST_BUILD `copy_if_different` into it, which raced under `--parallel` and
  failed the build. It was unreachable until a test resource actually changed,
  because the copy writes nothing when nothing differs. One target now copies the
  union and the executables depend on it.

### Added

* `consumer-smoke-test.sh` asserts the package's exported target set against a
  literal list, which the README names as well, and compiles `<tessera/api.hpp>`
  and `<tessera/engine.hpp>` on their own. The README calls those two the public
  API, but the smoke test had only ever included `db.hpp` and friends, so nothing
  verified they were installed or self-contained.
* `tools/check-cert-expiry.sh`, run in CI, fails when any test certificate is
  within 180 days of expiry, and when any leaf certificate is issued for more
  than 825 days. The signing CA's issuance database shows the certificates lapsed
  in 2018, 2020, 2021, 2022 and 2024; the next lapse now arrives as a sentence
  naming the file and the date. The 825-day ceiling is Apple's: its Security
  framework rejects longer-lived server certificates with
  `errSSLXCertChainInvalid` and no explanation, while OpenSSL accepts them.
* `certificate-authority/regenerate-server-certs.sh` replaces six blocks of
  manual `openssl` invocation in a markdown file.

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
