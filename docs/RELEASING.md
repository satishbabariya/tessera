# Releasing

## The remote

`origin` is `https://github.com/satishbabariya/tessera`. There is deliberately no
`upstream` remote: this repository is not a fork that tracks realm-core, and
`tools/check-no-vendor-hosts.sh` fails if one is added, because a remote that can
be pushed to by accident is worse than no remote at all.

Confirm before pushing anything:

```sh
git remote -v
```

## Before tagging

Run the whole gate. Not a subset — the gate has been too narrow three times in
this project's history, and each time something shipped broken:

```sh
rm -rf build.release
cmake -B build.release -DCMAKE_BUILD_TYPE=Release
cmake --build build.release -j"$(getconf _NPROCESSORS_ONLN)"   # no target: build all

CLEAN=$(mktemp -d)
TMPDIR="$CLEAN/" ./build.release/test/tessera-tests.app/Contents/MacOS/tessera-tests
TMPDIR="$CLEAN/" UNITTEST_THREADS=1 ./build.release/test/tessera-sync-tests.app/Contents/MacOS/tessera-sync-tests
TMPDIR="$CLEAN/" ./build.release/test/object-store/tessera-object-store-tests.app/Contents/MacOS/tessera-object-store-tests

for c in tools/check-*.sh; do
  case "$c" in
    *copyright*) "$c" 560 ;;
    *)           "$c"     ;;
  esac
done
tools/verify/consumer-smoke-test.sh build.release
tools/verify/clean-clone-test.sh
```

The loop is deliberate. This list named five checks when there were five, and
went on naming five when there were twelve -- a gate that enumerates its own
members drifts behind them. `tools/README.md` says what each one does.

Expected counts move whenever tests are added, so treat the table below as the
last measured values rather than as constants, and check it against a recent
`main` run before trusting it. What matters is that Debug and Release differ
legitimately -- some tests are compiled out of Release by `#ifdef TESSERA_DEBUG`
and `TEST_IF(..., SimulatedFailure::is_enabled())` -- so compare like with like.

## Read the run's conclusion, not the count of green jobs

A CI matrix can show four of five jobs green while the run itself is
`cancelled`. `concurrency: cancel-in-progress` stops superseded runs when a new
commit is pushed, which is the behaviour you want -- but the jobs that had
already finished keep their `success` status, so counting them reports a pass
that never happened.

```sh
gh api repos/<owner>/<repo>/actions/runs/<id> --jq .conclusion
```

That is the authoritative answer. It is the same mistake as counting passing
tests without checking the binary was rebuilt: the individual signals look
right, and the aggregate says otherwise.

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
| CoreTests | 1664 | 1659 |
| SyncTests | 461 | 460 |
| ObjectStoreTests | 343 | 343 |
| Tests disabled by `TEST_IF` | 32 | 36 |

Measured on macOS, 2026-08-29, at `feea4f54b`. Linux Debug agrees on 1664 and
reports 31 disabled rather than 32, because `Shared_RobustAgainstDeathDuringWrite`
runs there and not on Apple platforms.

The five-test difference between Debug and Release is `#ifdef TESSERA_DEBUG`; the
four-test difference in the disabled count is `SimulatedFailure::is_enabled()`,
which is false in Release. A discrepancy that does not decompose into those two
is worth investigating rather than accepting.

The disabled row is new and matters more than it looks. Until #6 the framework
computed that number and printed nothing, so a test switched off by its condition
was invisible: absent from the total and from every other figure. If it moves
without a test being added or removed, something changed platform or
configuration underneath you.

**Check the assertion counts too, not just the test counts.** A suite reporting
the same number of tests with a wildly different number of assertions is usually
a stale binary rather than a passing run -- during Phase 0b an ObjectStoreTests
run reported 343 passing with 53,540 assertions against a baseline of ~70,000,
because the binary predated the changes being verified.

## The clean-clone test

`tools/verify/clean-clone-test.sh` automates this, and since #13 the nightly
runs it against the published repository. Run it by hand before a release
anyway: the nightly tests whatever `main` was at 03:17, and a release is a
different commit.

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

- Verified by CI on macOS (Apple clang, arm64) and Linux x86-64 (gcc-13 and
  clang-18), in Debug and Release. Windows, Linux on ARM, iOS, Android and WASM
  are not verified.
- The API is stable in shape but not yet frozen; breaking changes are allowed in
  0.x minors.
- The file format is new. There is no migration from Realm files, and none is
  planned.
- One write transaction at a time. This is a design property, not a bug.
