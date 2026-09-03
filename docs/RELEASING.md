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

PREFIX="$(mktemp -d)/tessera"
cmake --install build.release --prefix "$PREFIX" > /dev/null

for c in tools/check-*.sh tools/test-*.sh; do
  case "$c" in
    *copyright*) "$c" 560 ;;
    *)           "$c" "$PREFIX" ;;
  esac
done
tools/verify/authorization-end-to-end.sh build.release
tools/verify/survives-a-hard-kill.sh build.release
tools/verify/consumer-smoke-test.sh build.release
tools/verify/clean-clone-test.sh
```

The loop is deliberate. This list named five checks when there were five, and
went on naming five when there were twelve -- a gate that enumerates its own
members drifts behind them.

The glob drifted anyway, which is worth admitting here rather than quietly
fixing. It matched `check-*.sh` only, and the two suites added for
`pr-status.sh` and `pre-push` are named `test-*.sh`, so a loop written to stop
enumerating its members went back to missing two of them on a naming
convention. Both patterns are matched now. `tools/README.md` says what each one
does.

Every check is handed the install prefix, whether it wants one or not. That is
the same reason the loop is a glob: `check-install-surface.sh` needs an
installed tree, and a gate that names its exceptions acquires a new hole every
time a check is added. Passing the prefix to all of them means the next such
check works without this document changing. It was verified that the other
thirteen ignore an extra argument.

Before that, the glob ran `check-install-surface.sh` with no arguments, and it
exited 2 on its usage message without checking anything. The loop does not test
exit codes, so the release gate reported nothing wrong while silently not
running the check -- which is the failure mode this whole section exists to
prevent, reappearing one level down.

The two end-to-end verifications were missing from this list for the same
reason: CI runs `authorization-end-to-end.sh` and `survives-a-hard-kill.sh` on
every pull request, so nothing appeared to be wrong, and the gate that decides
whether to *tag* a build checked neither the authorization model nor crash
durability. Seven properties are verified through the shipped binaries and this
gate named five of them.

Both take a build directory, and both begin by checking that the load test in it
accepts the flags they pass. That check exists because running them here against
a stale `build.release` reported `FAIL: the deployed path does not hold` for a
server that was behaving correctly -- the load test predated `--converge` and
printed its usage instead, and the script blamed the property. A binary that
rejects its arguments makes everything downstream of it look broken.

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
| SyncTests | 477 | 476 |
| ObjectStoreTests | 343 | 343 |
| Tests disabled by `TEST_IF` | 32 | 36 |

Assertion counts, for the staleness check below. These are noisier than the test
counts -- several suites randomise -- so treat a small drift as normal and a
factor as worth explaining:

| Suite | Debug | Release |
|---|---|---|
| CoreTests | 117,023,043 | 102,273,888 |
| SyncTests | 40,400 | 127,236 |
| ObjectStoreTests | 70,129 | 370,586 |

The two suites where Release is several times Debug are not a mistake and not a
stale binary -- `build.release` was built from scratch for this measurement.
Nobody has yet explained the ratio, which is why it is written down: the next
person to see 370,586 should know it was 370,586 in 0.3.0 too.

Measured on macOS, 2026-09-02, at `29ec34cbe`. The SyncTests rows moved from
461/460 when the authentication work added sixteen tests; the one-test Debug to
Release difference is unchanged. Linux Debug agrees on 1664 and
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

## Run the suites one at a time

CoreTests in Release hung once during the 0.3.0 gate, with SyncTests running
single-threaded beside it and CoreTests using all eighteen cores. It stopped in
`DBTestPathGuard::~DBTestPathGuard()` -> `File::remove`, holding an open
descriptor on a `.realm.management` directory, with its CPU time frozen across
three samples a minute apart. Not slow -- stopped.

It did not reproduce. The same test passes alone, the whole suite passes alone
(1659 tests, 102,273,888 checks), and the CI matrix runs both Release
configurations on every pull request. So this is recorded as an observation
rather than a diagnosis: a contention-sensitive stall in test cleanup that
nobody has yet reproduced deliberately.

The practical consequence is to run the suites in sequence rather than in
parallel, which the commands above already do. If it happens again, sample the
process rather than assuming it is slow -- frozen CPU time is the difference,
and `ps -o time` shows it in one line.

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
