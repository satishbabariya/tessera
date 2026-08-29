# A test file that was never in the build

`test/test_util_enum.cpp` -- sixty-five lines, one test -- was not referenced by
`test/CMakeLists.txt`. It compiled into no target and had never run.

It covers `tessera/util/enum.hpp`, which this project installs as public API and
whose implementation is compiled into the library. Adding the file to the build
required no other change: it compiled and passed on the first attempt, with two
checks. It was not broken, and it was not obsolete. It had simply never been
listed.

## Why nothing noticed

A test file left out of the build is invisible in every direction at once.

It does not fail, because it does not run. It does not appear as skipped, because
the framework never learns it exists -- `TEST()` registers a test by running a
static initialiser in the translation unit, and there is no translation unit. It
does not break the build, because nothing references it. `git` shows it as a
tracked file in the test directory, which is exactly where it belongs.

And the suite's own total offers nothing. It reports 1,662 tests, and 1,662 is
correct for what was compiled. Nobody knows which number to expect, so no number
looks wrong.

## How it was found

Not by looking for it. The crash-safety test in
[0b-format-rejection-untested.md](0b-format-rejection-untested.md) prompted a
scan of every test name in `test/*.cpp`, run one process at a time, to see which
ones executed zero checks. That scan reported 512 names as absent from the
`CoreTests` binary, which was expected: most belong to the sync and object-store
suites.

Checking that assumption -- running the 512 against the sync binary as well --
left 74 in neither. Most of those are `TEST_IF` with a false condition, which is
the visible and correct way to disable a test, or are guarded by `#if` for a
platform or a feature. Sorting the remainder by preprocessor depth left a small
group at depth zero: no guard above them at all, and still in no binary.

Three of those turned out to be an artefact of scanning a binary built from a
different branch. One did not.

## The check

`tools/check-test-sources-listed.sh` fails if any `test/*.cpp` is not named by
the `CMakeLists.txt` that should compile it. It covers 133 files today.

It is named for what it inspects rather than for what one would like to conclude.
The check establishes that a file is visible to the build; it does not establish
that any given configuration compiles it. Twenty-six test files here are listed
inside `if(TESSERA_ENABLE_SYNC)` and are correctly absent when sync is off. The
first version of this check was called `check-tests-compiled.sh` and printed
"all 133 test sources are referenced", which claimed more than it had looked at
-- the exact failure this directory documents a dozen times over, reproduced in
the check written to prevent it.

Canary-tested twice: an unreferenced file fails it, and an empty tree fails it
rather than passing over nothing -- the same guard every check in this project
carries, for the same reason.

## The same thing, one directory up

`test/benchmark-util-network/` held a 7,700-line-byte `main.cpp` with its own
`.gitignore` and no `CMakeLists.txt`, and `test/CMakeLists.txt` did not
`add_subdirectory` it. Immediately below the four benchmarks it does add is the
comment `# FIXME: Add other benchmarks`, so the omission was known and left.

It did not compile. `network::end_of_input` had moved to
`util::MiscExtErrors::end_of_input` in an upstream namespace refactor
(`a396142c3 Updated namespaces for sync/network files`), which touched every file
the build knew about and not this one. `BenchmarkResults` had since gained a
required `suite_name` and a third argument to `finish()`, and this file had
followed neither.

Three API changes, none of which broke anything, because nothing compiled the
file that used them. It is now wired up and builds and runs:

    Post:  min 217.53ms  max 247.52ms  median 220.35ms  avg 220.71ms  stddev 3.16ms

Restored rather than deleted, on the grounds that the next such refactor should
break the build instead of the file. A file that cannot rot loudly will rot
quietly.

`tools/check-test-sources-listed.sh` does not cover this case: it walks `test/`
and `test/object-store/`, and would not have noticed a whole directory missing
from the build graph. That gap is stated here rather than papered over -- an
`add_subdirectory` audit is a different check from a source-listing one, and
writing one check and claiming it covers both is the mistake this directory
exists to record.

## The shape of it

Every other finding here is about a check that inspected the wrong thing, or a
claim nobody tested. This one is about a file that was never in any of those
conversations, because the build never mentioned it and the test report never
counted it.

The general form: **a total is not a coverage measure unless something
independently determines what the total should be.** 1,662 passing tests says
nothing about the 1,663rd that was never compiled.
