# Finding: the build depended on infrastructure the project does not control

Date: 2026-08-28
Task: Phase 0a Task 2 (sever the vendor supply chain)

## What was found

`realm-core` could not be built without fetching artifacts from
`static.realm.io`, MongoDB-controlled infrastructure with no remaining reason to
exist. The coupling was **four times deeper than the initial survey suggested.**

| Site | Purpose |
|---|---|
| `tools/cmake/AcquireRealmDependency.cmake:32` | Prebuilt OpenSSL and zlib tarballs |
| `tools/cmake/linux.toolchain.base.cmake:5` | Linux cross-compile toolchains (`.tar.zst`) |
| `CMakeLists.txt:388`, `:403` | The fetch module was `configure_file`d **and installed** to `share/cmake/Realm` |
| `tools/cmake/RealmConfig.cmake.in:2,10,28` | The installed package config re-invoked the fetcher, so **every downstream consumer inherited the dependency** |
| `evergreen/` (~22 references) | Node, jq, Go, clang, CMake, grcov, `stitch-support` all fetched from Realm's S3 |

The fourth row is the one that matters most: it was not merely *our* build that
depended on `static.realm.io`, but the build of anyone who consumed an installed
copy of realm-core.

Verified live on 2026-08-28: the OpenSSL tarball still served (HTTP 200, 14MB,
last modified August 2024). Working, but on borrowed time.

## What was done

- Deleted `AcquireRealmDependency.cmake` and `linux.toolchain.base.cmake`.
- Deleted `evergreen/` — MongoDB's internal CI plus the BAAS provisioning
  harness. Unusable without MongoDB infrastructure, and the largest single
  source of vendor fetches.
- Rewrote `RealmConfig.cmake.in` to resolve dependencies from the consuming
  environment only.
- OpenSSL and zlib are now always resolved via `find_package`.
- `dependencies.yml`: `OPENSSL_VERSION` (a pinned prebuilt tarball version)
  replaced by `OPENSSL_MIN_VERSION: 3.0.0`. `ZLIB_VERSION` removed (unpinned;
  `find_package` decides). `BAAS_VERSION`/`BAAS_VERSION_TYPE` removed — read
  only by the deleted `evergreen/install_baas.sh`.
- Added `tools/check-no-vendor-hosts.sh`, wired into CI in Task 4, so the
  invariant is enforced rather than merely intended. The guard requires a URL
  scheme to match, so prose describing this history does not trip it, and it was
  canary-tested to confirm it still catches a real fetch.

## Correction to the plan

The plan proposed building OpenSSL from pinned source via
`FetchContent_MakeAvailable` for Android and Windows. **That cannot work:
OpenSSL ships no `CMakeLists.txt` and configures through a Perl script**, so
`FetchContent` has nothing to add. `ExternalProject_Add` driving OpenSSL's
`Configure` would be required, and cross-compiling it for four Android ABIs is a
project in its own right.

Resolution: OpenSSL is supplied by the environment on every platform. Integrators
cross-compiling for Android or Windows point CMake at their own build via
`OPENSSL_ROOT_DIR`. This fully satisfies the Phase 0a invariant — no dependency
on hosts we do not control — and is what most C++ projects do.

**Known consequence, carried into Task 5:** Android CI cannot build sync or
encryption until an OpenSSL is provided for it, because `REALM_NEEDS_OPENSSL`
includes Android and the NDK ships none. Options at that point: build OpenSSL in
the CI job, or restrict the Android compile-only job to a configuration that
needs no TLS. Decision deferred to Task 5 rather than guessed at here.

## Digests

Where versions are pinned for reproducibility, digests were measured, not copied:

| Artifact | SHA-256 | Authenticity |
|---|---|---|
| `openssl-3.5.8.tar.gz` | `a8f84a39918ec6415ce765d9b429d313ba97b8143169c172e734b9514464f5b2` | **Matches OpenSSL's published `.sha256`** |
| `zlib-1.3.2.tar.gz` | `bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16` | TLS-fetched from the project's own release assets. zlib publishes GPG `.asc` rather than `.sha256`; signature verification is a follow-up |

Note the distinction: hashing a file you just downloaded pins it against future
tampering but does not by itself prove authenticity. Only the OpenSSL digest was
independently corroborated against a publisher-provided digest.

## Dangling references left for later tasks

Deleting `evergreen/` left references behind. Deliberately not fixed here, to
keep this commit to one concern:

| Reference | Disposition | Task |
|---|---|---|
| `doc/development/how-to-use-remote-baas-host.md` | Delete — documents a service that no longer exists | 7 |
| `doc/development/how-to-release.md` | Rewrite — describes the Evergreen release flow | 7 (stub) / 0b (full) |
| `tools/release-init.sh` | Rewrite or delete with the release workflows | 7 |
| `test/tessera-fuzzer/README.md` | Minor mention; correct in passing | 7 |
| `CHANGELOG.md` | **Leave.** Historical entries are a record, not a reference | — |
| `test/object-store/CMakeLists.txt` (`REALM_MONGODB_ENDPOINT`) | Investigation I3 — determines which sync tests need a live BAAS | 11 |

The last row is the substantive one: it is the concrete entry point for I3, which
decides whether BAAS-dependent tests get retargeted at the bundled server or
deleted.
