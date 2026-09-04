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

`check-surface-is-reachable.sh` compiles every declared entry point, so it
wants a compiler as its second argument; the loop below does not pass one and it
falls back to `c++`, which is fine for a release gate run on one machine. CI
passes the matrix compiler explicitly, because a header set can differ by
compiler.

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

Assertion counts were unusable as a baseline, and are usable again.

They were recorded here as one Debug sample and one Release sample per suite,
with a note that "a factor is worth explaining". Measured as repeat runs of one
binary in one configuration, the spread within that configuration was itself a
factor:

| Suite | Release, repeat runs of one binary | Spread |
|---|---|---|
| CoreTests, `UNITTEST_THREADS=2` | 10,188,745 / 12,417,707 / 48,889,143 / 93,027,810 | 9.1x |
| CoreTests, `UNITTEST_THREADS=1` | 99,442,447 / 99,508,221 / 99,655,290 | 0.2% |
| SyncTests | 39,039 to 125,116 over twelve runs | 3.2x |

Every run passed every test. The cause was a defect in the test framework, not
in any test: with more than one thread, one thread's accounting was discarded,
and it took that thread's failures with it, so the suite could exit 0 with a
failing test in it. With that fixed, the same binaries measure

| Suite | Release, repeat runs | Spread |
|---|---|---|
| CoreTests, 2 threads | 99,372,550 / 99,564,475 / 99,637,487 / 99,772,242 / 99,837,290 | 0.5% |
| SyncTests, 4 threads | 119,612 / 120,589 / 121,653 | 1.7% |

so a count is worth comparing again, within a tolerance rather than exactly.
Treat a drift of a percent or two in CoreTests as normal and more as worth
explaining. SyncTests moves further, for a known reason:
`Network_RepeatedCancelAndRestartRead` is about 70% of that suite's checks and
checks once per socket read completion while moving 64 MiB, so its count is
however many reads the kernel and the scheduler decide to complete.

What established the true figure was splitting the suite by concurrency: its
1626 concurrent and 33 nonconcurrent tests measure 98,356,931 and 1,148,179
checks when run separately, summing to 99,505,110 against single-threaded full
runs of 99,442,447 to 99,655,290. At two threads the same binary had reported as
little as 10,188,745, so up to 89 million checks were going uncounted. See
`docs/findings/0b-a-failure-that-was-not-counted.md`.

So the numbers previously tabulated --

| Suite | Debug | Release |
|---|---|---|
| CoreTests | 117,023,043 | 102,273,888 |
| SyncTests | 40,400 | 127,236 |
| ObjectStoreTests | 70,129 | 370,586 |

-- are single draws from distributions wider than the differences they were
being used to explain. CoreTests' Debug-to-Release gap is 1.14x, inside a 9.1x
noise band. SyncTests' Debug 40,400 and Release 127,236 both sit inside the
range Release alone produces, so they are not evidence of a Debug-to-Release
difference at all. The ratio nobody had explained may simply not exist.

For SyncTests the mechanism is known: `Network_RepeatedCancelAndRestartRead` is
about 70% of that suite's checks, and it checks once per socket read completion
while moving 64 MiB, so its count is however many reads the kernel and the
scheduler decide to complete. Pinning `UNITTEST_RANDOM_SEED` fixes the write
sizes and not the scheduling, which was measured and does not help. CoreTests
has not been bisected the same way.

ObjectStoreTests is the exception worth noting: 370,423 assertions observed
against 370,586 recorded. One sample, so not a claim.

**Test counts are stable and are the ones to check.** 1659 / 476 / 343 in
Release, every run, and they decompose into `#ifdef TESSERA_DEBUG` and
`SimulatedFailure::is_enabled()` as described below. A test that vanishes shows
up there. A test that stops asserting anything does not show up in either, which
is what `tools/analyse-zero-check-tests.sh` is for -- it counts per test, one
test at a time, because the framework tracks checks only in its summary.

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

The version bump and the changelog move go through a pull request, like every
other change. `main` is protected -- direct pushes are refused by the server and
by `tools/pre-push` -- so there is no `git push origin main` step.

```sh
# on a branch, with dependencies.yml and CHANGELOG.md updated:
gh pr create --title "release: v0.1.0"

# once its six required checks pass and it has merged:
git fetch origin
git tag -a v0.1.0 origin/main -m "Tessera v0.1.0

First release of the Tessera fork of realm-core (upstream f8752e180,
v14.14.0). An embedded local-first database with a documented public API,
no vendor dependencies, and a working open sync stack."

git push origin v0.1.0
```

The tag is created against `origin/main` rather than whatever the working tree
is on, so it names the commit that actually merged. Tags are not covered by the
branch protection rule, so pushing one needs no exception.

This section used to say `git push origin main`, which stopped working when
`main` became protected. Recorded here rather than left for whoever next tries to
cut a release to discover: making a rule stricter without checking what depended
on the old behaviour is how a documented procedure comes to describe something
that cannot be done.

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
