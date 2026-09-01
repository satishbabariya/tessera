# Changelog

## Unreleased

### Added

* **The sync server authenticates connections.** It reads the credential the
  client already carries on the WebSocket handshake -- `?baas_at=<token>`,
  appended by `ClientImpl::Connection::get_http_request_path` -- verifies it
  against the public key given to the `Server` constructor, and answers HTTP 401
  before upgrading the connection if the token is missing, malformed, unverifiable
  or expired.

  A server given no public key can verify nothing, so it authenticates nobody and
  does not demand a token either -- including from a client that sends
  `?baas_at=` with an empty value, which is what a client does while its access
  token is being refreshed.

  Until now it read nothing: `verify_access_token` and `AccessControl::can` were
  never called and the public key was never used, so anyone able to reach the port
  could bind to any database path. See `docs/findings/0b-server-has-no-auth.md`.

  A server constructed with no public key still accepts unsigned tokens, which is
  the documented keyless mode `Sync_RunServerWithoutPublicKey` covers. It now says
  so in the log once per connection rather than silently.

  Checked at the handshake rather than at BIND: that is where the token is, it
  costs one check per connection instead of one per session, it can refuse with an
  HTTP status before any protocol state exists, and BIND's token field was emptied
  upstream on purpose. See `docs/findings/0b-auth-belongs-at-the-handshake.md`.
  **Authorisation is separate and follows below.** `AccessControl::can` is still
  never called at that point: the handshake settles *who* is connected, and the
  path being asked for is not known until BIND.

* **The sync server authorises access to a path.** The token verified at the
  handshake is carried on the connection, and `AccessControl::can` is consulted
  at BIND with the requested path -- the first call that function has had since
  upstream removed its caller in realm-core #5151. A token scoped to one path is
  refused for any other, with `permission_denied`.

  `Privilege::Download` is the floor for binding: a session that cannot download
  cannot usefully bind, since even an upload-only client receives the server's
  history.

* **A read-only token cannot write.** `Privilege::Upload` is checked where an
  UPLOAD message arrives rather than at bind, because refusing it at bind would
  lock out read-only sessions -- which have every right to connect and receive
  history, and may only not send changesets.
  `g_signed_test_user_token_readonly` grants `["download"]`, and until now a
  session presenting it could write freely.

* `Sync_Auth_HandshakeRejectsABadSignature`, `...RejectsAnExpiredToken` and
  `...AcceptsAValidToken`. The third is not redundant: without it the first two
  would pass against a server that refused every connection.
* `Sync_Auth_APathScopedTokenIsRefusedElsewhere` and `...WorksOnItsOwnPath`.
  `g_signed_test_user_token_for_path` carries `"path": "/valid"` and had sat in
  `sync_fixtures.hpp` with no test to belong to.
* `Sync_Auth_AReadOnlyTokenCannotUpload` and `...MayStillBindAndDownload`. The
  second is what makes the first mean something: a server that refused read-only
  sessions outright would pass the first and fail this one.

### Fixed

* Three defects in the sync install rules. `set(SYNC_HEADERS
  ${IMPL_INSTALL_HEADESR} ...)` was a typo, so the `Sync` target's source list
  silently omitted the impl headers; an `install(FILES ${SYNC_INSTALL_HEADERS}
  DESTINATION include/tessera/sync)` rule appeared twice verbatim; and
  `install(FILES ${UTIL_INSTALL_HEADERS} ...)` referenced a variable defined
  nowhere in the project, installing an empty list. All three are the same
  failure mode -- CMake expands an undefined variable to nothing without
  complaint, so a misspelling is a silent omission rather than an error.
  Verified with `cmake --install` to a clean prefix before and after: the same
  260 files, so the removed rules were shipping nothing.

* A hanging test now reports as a failure rather than a cancellation. GitHub
  reports a job killed by `timeout-minutes` as `cancelled` -- the same word it
  uses when `concurrency.cancel-in-progress` supersedes a run -- so an
  `ObjectStoreTests` deadlock read as "still running" for about two hours. A
  *step*-level timeout is reported as `failure` instead, which was measured with
  a throwaway workflow rather than assumed, so every long step in `build.yml`
  now carries its own cap several times its observed duration. The job-level 60
  stays as a backstop.

* `tools/pr-status.sh` no longer announces `NO BUILD MATRIX` for a pull request
  whose build has not registered yet. Right after a push the changelog check can
  be green while the matrix is still being created, and nothing is pending, so a
  verdict drawn from the check list alone called the build absent. It now asks
  whether a build run exists for the head commit and reports the two cases
  differently. See `docs/findings/0b-a-hang-should-look-like-a-failure.md`.

* The `pre-push` hook no longer rejects this repository. Inherited from
  realm-sync, it compared the push destination against four permitted
  `realm-sync` remotes and aborted otherwise, so it refused every push to
  `satishbabariya/tessera` -- not just pushes to `main`. Nothing installs the
  hooks and `tools/README.md` did not list them, so the file had never been
  executed once. It now enforces the rule the project actually has: changes
  reach `main` through a pull request, with `TESSERA_ALLOW_MAIN_PUSH=1` for the
  deliberate exception. `tools/install-hooks.sh` installs the hooks and
  `tools/test-pre-push.sh` covers them in CI. See
  `docs/findings/0b-hooks-nobody-runs.md`.

* `tools/pr-status.sh` reports pull-request checks by outcome instead of counting
  failures. The poll it replaces printed `5 of 7` for a stack whose runs had been
  *cancelled* -- a terminal state -- which is the same string it prints for a
  stack still building, so two pull requests waited two hours on runs that would
  never finish. It also covers the case #32 fixed: checks containing no build job
  at all now say so rather than reading as green. See
  `docs/findings/0b-green-by-absence.md`.
* Removed `AccessControl::is_admin`, which was inverted. It treated the absence
  of a path scope as evidence of administrator status, so a token whose entire
  grant was `["download"]` -- one that `can()` correctly refuses permission to
  upload anything anywhere -- came back as an administrator, while a token
  scoped to a path and holding upload rights within it did not. Seven of the
  suite's eight token fixtures are unscoped and so were all administrators. The
  function was called by nothing, which is why it survived; its own comment read
  "It is not safe since it might be too liberal". Tessera implements no
  administrator concept, so nothing is preserved by keeping a broken
  implementation of one. `AccessToken::admin` and `admin_field` stay -- they are
  parsed token data. See `docs/findings/0b-is-admin-was-inverted.md`.

* CI runs on every pull request, not only those targeting `main`. A stacked pull
  request -- one whose base is another feature branch -- got no build matrix at
  all, so `gh pr checks` reported two green checks and nothing else. Two passing
  checks and seven passing checks look identical if you count failures rather
  than reading the list.

* A server could terminate any client connected to it. `sync/protocol.cpp` mapped
  `ProtocolError` values onto `ErrorCodes`, and twelve of them fell through to
  `TESSERA_UNREACHABLE()`, which aborts the process. Two are what a server sends
  when it refuses a bind -- `token_expired` and `bad_authentication` -- so a
  client could not survive being told its authentication had failed. They now map
  to `AuthError`, `SyncPermissionDenied`, `SyncServerPermissionsChanged` and
  `SyncProtocolInvariantFailed`. Nothing sent them, because nothing
  authenticates, which is why this survived. See
  `docs/findings/0b-client-terminates-on-errors.md`.

### Changed

* ~~The sync client sends the token it was given.~~ Reverted before release. The
  token was never absent: the client carries it on the WebSocket handshake as
  `?baas_at=`, and upstream emptied the BIND field deliberately in 2022. Sending
  it on BIND as well put a second copy of the credential on the wire once per
  session. See `docs/findings/0b-auth-belongs-at-the-handshake.md`.

  The original entry follows, struck through rather than deleted because it
  describes a change that briefly existed on `main`.

* The sync client sends the token it was given. `Session::Config::signed_user_token`
  reached the connection and was used for the WebSocket handshake request, but
  `send_bind_message` sent a local variable named `empty_access_token` --
  "discarded since it's ignored by the server". The server does ignore it, partly
  because nothing sent one. This changes nothing on its own: the server still does
  not read it. It is the half of authentication that can land without the other
  half, and it must land first, or adding server-side verification fails every
  bind in the suite. See `docs/findings/0b-both-ends-of-the-token.md`.

### Fixed

* `util::base64_decode("")` terminated the process. Its guard against
  overlapping input and output buffers called `Span::back()`, which asserts on an
  empty span, and an empty input produces a zero-length output buffer.
  `AccessToken::parse("")` inherited it. Unreachable today because nothing on the
  server parses a token, and a remote abort the moment anything does -- an empty
  token is what a client with no credentials sends.

### Added

* `Sync_Auth_MalformedTokensAreRejectedNotFatal` covers six malformed tokens.
  `Sync_Auth_TheSuitesOwnTokenVerifiesAgainstTheSuitesOwnKey` and
  `Sync_Auth_AWrongKeyRejectsTheToken` establish that the suite's own token
  verifies against the suite's own key, and that a different key rejects it --
  neither of which anything had checked, because the server never verifies.

### Fixed

* The `Self-hosting` section of `README.md`, shipped in 0.2.0, described an
  authentication model the sync server does not have. It said the server
  "requires a parseable signed JWT on every bind" and parses without verifying
  when no public key is configured. In fact the server accepts a token on bind,
  logs it, and never refers to it again: `verify_access_token` and
  `AccessControl::can` are never called, the public key passed to the `Server`
  constructor is never read, and `Config::authorization_header_name` is used only
  to log its own value at startup. **The server performs no authentication and no
  authorization.**

  Nothing is exposed by this. The server is not in the installed package -- there
  is no `Tessera::SyncServer` target, no installed header and no executable -- so
  it cannot be reached from outside a build tree. It does mean that making the
  server installable must come after adding authentication rather than before.
  See `docs/findings/0b-server-has-no-auth.md`.

### Added

* `tools/check-server-not-shipped-unauthenticated.sh`, run in CI, fails if the
  sync server acquires an `install(TARGETS)` rule or an executable while
  `server.cpp` still consults no token. A conjunction on purpose: either half
  alone is fine, and only the pairing of reachable and unauthenticated is
  refused.

## 0.2.0 — 2026-08-29

### Breaking

Tessera is 0.x and promises no compatibility between versions. These are the
changes that actually break something, stated plainly rather than left for a
reader to infer from the entries below.

* **Two versions must not open the same database file.** On Windows the
  interprocess write mutex is a named object derived from the file path, and its
  prefix changed from `realm_named_intermutex_` to `tessera_named_intermutex_`.
  A 0.1.x process and a 0.2.0 process would take different mutexes for the same
  file and both believe they held the single writer lock. On POSIX the write lock
  is file-based and unaffected, but the change-notification FIFO was renamed, so
  the two would not see each other's commits.
* **Two versions must not sync with each other.** The WebSocket subprotocol a
  server offers changed from `realm.io` to `io.tessera`, and the `User-Agent` and
  HTTP `Server` headers from `RealmSync/` to `TesseraSync/`.
* **Error category names changed** from `realm.*` to `tessera.*`:
  `basic_system`, `util.misc_ext`, `simulated_failure`, `sync.network.resolve`,
  `sync.network.ssl`. Code matching on `std::error_code::category().name()`
  breaks.
* **`TESSERA_PRODUCT_NAME` is now `tessera`**, so `TESSERA_VER_CHUNK` -- printed
  on abort and logged at sync startup -- reads `[tessera-0.2.0]` rather than
  `[realm-core-...]`.
* `TESSERA_VERSION` in the generated `config.h` was `""`. `config.h.in`
  interpolated `@VERSION@`, a CMake variable that does not exist; the one that
  does is `TESSERA_VERSION`, holding the `git describe` output. It now reads
  e.g. `v0.1.1-29-g47c8d3fdf`. No C++ code used it, so nothing behaved
  differently -- a generated header simply stated a version it did not have.
* **The command-line tools' CMake target names changed** to match the binaries
  they have always produced: `RealmTrawler` is `TesseraTrawler`, `Realm2JSON` is
  `Tessera2JSON`, and so on. The installed binary names are unchanged.

The file format is unchanged at version 1. A database written by 0.1.x opens in
0.2.0.

### Fixed

* `test/test_util_enum.cpp` is now in the build. It was in no `CMakeLists.txt`,
  compiled into no target and had never run, though it covers
  `tessera/util/enum.hpp`, which is installed as public API. It compiled and
  passed unchanged.
* `test/test_file_format.cpp`. README.md and ARCHITECTURE.md both state that
  Tessera rejects any file whose format is not its own, which is the fork's
  central promise, and nothing tested it. Five tests write a real database, patch
  its 24-byte header on disk and reopen: a `T-DB` mnemonic is refused, so is any
  other, format versions 2, 10, 24 and 255 are refused, and the error names the
  version it rejected. Each was confirmed to fail against a deliberately broken
  engine.
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
* `RobustMutex::is_robust_on_this_platform` was `false` in every translation
  unit, on every platform, including `thread.cpp`'s own. The platform detection
  was in `thread.cpp`, which includes `thread.hpp` twelve lines *before* defining
  any of it, so the constant was evaluated with the macros undefined -- while the
  implementation further down the same file compiled full robust-mutex support,
  because its `#ifdef` appears after the defines. A class whose implementation
  supported robust mutexes and whose public constant denied it. The detection now
  lives in the header that declares the constant. Five tests gate on it.
* The test framework computed `num_disabled_tests` and never printed it, so a
  test switched off by its `TEST_IF` condition was invisible: absent from the
  pass count and absent from every other number reported. There are 32 of them on
  macOS.
* The CI build job had no `timeout-minutes`. A dead-locking test would have held
  a runner for GitHub's six-hour default.
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
* `consumer-smoke-test.sh` asserts the package's exported target set against a
  literal list, which the README names as well, and compiles `<tessera/api.hpp>`
  and `<tessera/engine.hpp>` on their own. The README calls those two the public
  API, but the smoke test had only ever included `db.hpp` and friends, so nothing
  verified they were installed or self-contained.

### Changed

* The test binaries reject an argument beginning with `-` instead of treating it
  as a path prefix, and print what they do accept. `tessera-tests --help` used to
  set the prefix to `--help` and write its temporary databases into the working
  directory.

### Added

* `check-repo-hygiene.sh` rejects any tracked path that is not a legal filename
  on Windows, and its runtime-artifact rule now covers `.realm` and `.mx` as well
  as `.tess` -- the test suite names its files `.realm`, so the rule had been
  looking for artifacts the engine produces while the tests produce different
  ones.
* `.gitignore` covers the databases the test suite leaves behind.

* `tools/check-test-sources-listed.sh`, run in CI, fails if any test source is
  not named by the `CMakeLists.txt` that should compile it. It establishes that a
  file is visible to the build, not that any particular configuration compiles it
  -- 26 test files here are correctly conditional on `TESSERA_ENABLE_SYNC`.
  A test file left out of the build does not fail, does not appear as skipped and
  does not break anything, and the suite total is no help because nobody knows
  what it should be.
* `tools/analyse-zero-check-tests.sh` reports tests that run and execute no
  checks. Analysis rather than a gate: of 106 such tests, 105 are regression
  tests that assert by not crashing.
* Four more in the same file for README's other claim about the bytes on disk,
  "Encryption at rest. Optional AES-256, applied per page below the engine".
  `test_encrypted_file_mapping.cpp` covers the cryptor, page IVs and concurrent
  mappings in thirteen tests, none of which answers whether the data is on the
  disk in the clear. These write a distinctive string, confirm it is findable in
  an unencrypted file and absent from an encrypted one -- along with the table
  name, since "below the engine" means the engine's own structures too -- and
  check that an encrypted file is refused without the key and with a key that is
  one byte wrong.
* `tools/check-header-macros.sh`, run in CI, fails when a header decides
  something on a `TESSERA_` macro that only a `.cpp` defines. It checks 287
  macros and found exactly one instance, the robust-mutex constant above.
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
