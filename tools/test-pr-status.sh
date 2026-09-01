#!/usr/bin/env bash
# Exercises tools/pr-status.sh against recorded check output.
#
# The two states this tool exists to name -- a cancelled run and a missing build
# matrix -- cannot be produced on demand against a live repository, so without
# recorded fixtures the tool could only ever be tested in the one state that
# happens to be current. A check that has never been shown to fire is not a
# check, so the fixtures are the check.
#
# testdata/pr-status/cancelled.json and running.json are the pair that matters:
# they are the two states that the previous ad-hoc poll rendered identically,
# and the reason a merge stack sat for two hours on runs that had already
# stopped.
set -euo pipefail
cd "$(dirname "$0")/.."

expect() {
    local fixture="$1" want="$2" got
    got=$(PR_STATUS_FIXTURE="tools/testdata/pr-status/$fixture.json" \
          PR_STATUS_RUNS_FIXTURE="${RUNS_FIXTURE:-tools/testdata/pr-status/runs-none.txt}" \
          ./tools/pr-status.sh 0 | sed 's/^PR0  *//')
    if [[ "$got" != "$want" ]]; then
        echo "FAIL $fixture" >&2
        echo "  expected: $want" >&2
        echo "  got:      $got" >&2
        return 1
    fi
    printf '  ok  %-10s %s\n' "$fixture" "$got"
}

failures=0
echo "pr-status:"
expect empty     'no checks  <- nothing reported yet'                                  || failures=1
expect running   '2 passed, 5 running'                                                 || failures=1
expect cancelled '2 passed, 5 CANCELLED  <- WAITING ON NOTHING: re-run it'             || failures=1
expect realgreen '7 passed  <- ready'                                                  || failures=1
expect failing   '6 passed, 1 FAILED  <- failing'                                      || failures=1
expect nomatrix  '2 passed  <- NO BUILD MATRIX: these checks do not include a build'   || failures=1

# The whole point: these two must not render the same. The poll this replaces
# printed "5 of 7" for both.
export PR_STATUS_RUNS_FIXTURE=tools/testdata/pr-status/runs-none.txt
a=$(PR_STATUS_FIXTURE=tools/testdata/pr-status/cancelled.json ./tools/pr-status.sh 0)
b=$(PR_STATUS_FIXTURE=tools/testdata/pr-status/running.json   ./tools/pr-status.sh 0)
if [[ "$a" == "$b" ]]; then
    echo "FAIL a cancelled stack and a running stack render identically" >&2
    failures=1
else
    echo "  ok  cancelled and running are distinguishable"
fi

# The same checks, with a build run that exists for the head commit. This is a
# pull request seconds after a push: the changelog check is in, the matrix is
# created but has reported nothing. It must not be called NO BUILD MATRIX.
RUNS_FIXTURE=tools/testdata/pr-status/runs-one.txt \
    expect nomatrix '2 passed  <- build queued for this commit but not yet reported' || failures=1

exit "$failures"
