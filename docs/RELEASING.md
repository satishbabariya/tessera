# Releasing

## Prerequisite: a remote

Until a Tessera repository exists under the project owner's control, nothing can
be pushed and no release can be tagged. `origin` currently points at
`https://github.com/realm/realm-core`, which is the upstream we forked from and
must not be pushed to.

```sh
git remote rename origin upstream      # keep upstream history fetchable
git remote add origin <tessera-repo-url>
git remote -v                          # confirm before pushing
```

## Before tagging

Run the whole gate. Not a subset — the gate has been too narrow three times in
this project's history, and each time something shipped broken:

```sh
rm -rf build.release
cmake -B build.release -DCMAKE_BUILD_TYPE=Release
cmake --build build.release                    # no target argument: build all

CLEAN=$(mktemp -d)
TMPDIR="$CLEAN/" ./build.release/test/tessera-tests.app/Contents/MacOS/tessera-tests
TMPDIR="$CLEAN/" UNITTEST_THREADS=1 ./build.release/test/tessera-sync-tests.app/Contents/MacOS/tessera-sync-tests
TMPDIR="$CLEAN/" ./build.release/test/object-store/tessera-object-store-tests.app/Contents/MacOS/tessera-object-store-tests

tools/check-copyright-notices.sh 560
tools/check-no-vendor-hosts.sh
tools/check-layering.sh
tools/check-merge-deps.sh
tools/check-header-tiers.sh
tools/verify/consumer-smoke-test.sh build.release
```

Expected: CoreTests 1647 (Release), SyncTests 461, ObjectStoreTests 343. Debug
reports 1652 — five tests are compiled out of Release builds by `#ifdef
TESSERA_DEBUG` and `TEST_IF(..., SimulatedFailure::is_enabled())`. Compare like
with like.

## Both configurations, not one

Debug and Release compile different code. `#ifdef TESSERA_DEBUG`,
`TEST_IF(..., SimulatedFailure::is_enabled())` and `#ifdef TEST_LOGGING_LEVEL`
all change what is built and what the tests request. Running one configuration
is running half the gate.

Phase 0b found this the hard way: a stale log-category name passed every Debug
run and aborted the entire Release suite before a single test executed, because
the entry sat behind an `#ifdef`.

Expected counts, which differ legitimately between configurations:

| Suite | Debug | Release |
|---|---|---|
| CoreTests | 1652 | 1647 |
| SyncTests | 461 | 460 |
| ObjectStoreTests | 343 | 343 |

**Check the assertion counts too, not just the test counts.** A suite reporting
the same number of tests with a wildly different number of assertions is usually
a stale binary rather than a passing run -- during Phase 0b an ObjectStoreTests
run reported 343 passing with 53,540 assertions against a baseline of ~70,000,
because the binary predated the changes being verified.

## The clean-clone test

The only honest test of "can someone use this" is doing what they would do, with
no local knowledge:

```sh
cd $(mktemp -d)
git clone <tessera-repo-url> tessera && cd tessera
# now follow README.md exactly, typing nothing it does not tell you to
```

If any step requires knowledge that is not in the README, fix the README rather
than working around it. This is spec criterion 10 and it is the one that decides
whether the release is real.

## Version

`dependencies.yml` carries `VERSION`. Tessera's line starts at 0.1.0 rather than
continuing Realm's 14.x, because the file format is incompatible and inheriting
the number would imply an upgrade path that does not exist.

Update `CHANGELOG.md`: move the `Unreleased` section under the new version
heading, and start a fresh `Unreleased`.

## Tag

```sh
git tag -a v0.1.0 -m "Tessera v0.1.0

First release of the Tessera fork of realm-core (upstream f8752e180,
v14.14.0). An embedded local-first database with a documented public API,
no vendor dependencies, and a working open sync stack."

git push origin main
git push origin v0.1.0
```

## What 0.1.0 does not promise

State this plainly in the release notes rather than letting people discover it:

- Verified on macOS/arm64 only. Linux and Windows are expected to work and are
  unmeasured.
- The API is stable in shape but not yet frozen; breaking changes are allowed in
  0.x minors.
- The file format is new. There is no migration from Realm files, and none is
  planned.
- One write transaction at a time. This is a design property, not a bug.
