#!/usr/bin/env bash
# Prints one line per pull request summarising its checks by outcome.
#
# Written after a merge stack sat idle for two hours. The three pull requests in
# it were being polled with an ad-hoc filter that counted checks whose state was
# not SUCCESS and printed the total as "5 of 7" -- read, every time, as five
# checks still running. They were not running. The runs had been cancelled, and
# a cancelled run is terminal: waiting for it to finish is waiting forever.
#
# The distinction was always available. `gh pr checks --json bucket` reports
# pass / fail / pending / skipping / cancel, and cancel is a different bucket
# from pending. The filter simply never asked for it, so a terminal state and a
# running one rendered identically and the stack stalled in silence.
#
# So this prints the buckets by name and says outright when a pull request is
# waiting on nothing.
set -euo pipefail
cd "$(dirname "$0")/.."

# Reading the checks is isolated behind this function so the reporting below can
# be exercised against recorded JSON. PR_STATUS_FIXTURE makes the tests possible
# at all: cancelled runs cannot be conjured on demand against a live repository,
# and a check that has never been shown to fire is not a check.
fetch_checks() {
    if [[ -n "${PR_STATUS_FIXTURE:-}" ]]; then
        cat "$PR_STATUS_FIXTURE"
        return
    fi
    gh pr checks "$1" --repo "${PR_STATUS_REPO:-satishbabariya/tessera}" \
        --json name,state,bucket 2>/dev/null || echo '[]'
}

# Whether a build-workflow run exists for the pull request's head commit.
#
# The check list alone cannot tell "this pull request gets no build" from
# "the build has not registered yet". Immediately after a push the changelog
# check can be reported and green while the matrix is still being created, and
# nothing is pending -- so a verdict drawn from the check list alone announces
# NO BUILD MATRIX for a pull request whose build is seconds from appearing.
# That happened, on a pull request that was fine.
#
# This asks the question the check list cannot answer.
build_run_count() {
    if [[ -n "${PR_STATUS_RUNS_FIXTURE:-}" ]]; then
        cat "$PR_STATUS_RUNS_FIXTURE"
        return
    fi
    local pr="$1" repo="${PR_STATUS_REPO:-satishbabariya/tessera}" head branch
    head=$(gh pr view "$pr" --repo "$repo" --json headRefOid --jq .headRefOid 2>/dev/null) || { echo 0; return; }
    branch=$(gh pr view "$pr" --repo "$repo" --json headRefName --jq .headRefName 2>/dev/null) || { echo 0; return; }
    gh run list --repo "$repo" --branch "$branch" --workflow build --limit 10 \
        --json headSha --jq "[.[]|select(.headSha==\"$head\")]|length" 2>/dev/null || echo 0
}

# Whether GitHub considers the pull request mergeable.
#
# A conflicting pull request gets no workflow runs at all: GitHub cannot compute
# the merge ref that a `pull_request` event builds, so nothing triggers. The
# check list is then empty of builds, and the missing-matrix verdict below is
# true but useless -- it reports a symptom whose cause is one query away.
merge_state() {
    if [[ -n "${PR_STATUS_MERGESTATE_FIXTURE:-}" ]]; then
        cat "$PR_STATUS_MERGESTATE_FIXTURE"
        return
    fi
    gh pr view "$1" --repo "${PR_STATUS_REPO:-satishbabariya/tessera}" \
        --json mergeable --jq '.mergeable' 2>/dev/null || echo UNKNOWN
}

# Whether the newest build run was produced by the commit currently at the head
# of the pull request.
#
# After a force-push the previous head's checks stay attached to the pull request
# until the new run reports, so the tool showed seven green checks for a commit
# that was no longer there. On a release decision that is the one failure that
# matters: a stale green invites tagging a commit nothing verified.
#
# Returns "stale <sha>" when they differ, and nothing when they agree or when the
# question cannot be answered.
stale_against_head() {
    if [[ -n "${PR_STATUS_STALE_FIXTURE:-}" ]]; then
        cat "$PR_STATUS_STALE_FIXTURE"
        return
    fi
    # Every failure path returns 0 with no output: "cannot tell" is not "stale",
    # and a non-zero return here kills the caller under set -e. The first version
    # of this used `|| return`, which did exactly that -- the suite stopped
    # halfway through with no message, on the assertions that call this script
    # without a fixture.
    local pr="$1" repo="${PR_STATUS_REPO:-satishbabariya/tessera}" head branch run
    head=$(gh pr view "$pr" --repo "$repo" --json headRefOid --jq .headRefOid 2>/dev/null) || return 0
    branch=$(gh pr view "$pr" --repo "$repo" --json headRefName --jq .headRefName 2>/dev/null) || return 0
    run=$(gh run list --repo "$repo" --branch "$branch" --workflow build --limit 1 \
          --json headSha --jq '.[0].headSha' 2>/dev/null) || return 0
    if [[ -n "$run" && -n "$head" && "$run" != "$head" ]]; then
        echo "stale ${run:0:9}"
    fi
    return 0
}

status_line() {
    local pr="$1" json
    json=$(fetch_checks "$pr")

    local total pass fail pending cancel skip
    total=$(jq 'length' <<<"$json")
    pass=$(jq '[.[]|select(.bucket=="pass")]|length' <<<"$json")
    fail=$(jq '[.[]|select(.bucket=="fail")]|length' <<<"$json")
    pending=$(jq '[.[]|select(.bucket=="pending")]|length' <<<"$json")
    cancel=$(jq '[.[]|select(.bucket=="cancel")]|length' <<<"$json")
    skip=$(jq '[.[]|select(.bucket=="skipping")]|length' <<<"$json")

    local parts=()
    (( pass ))    && parts+=("$pass passed")
    (( fail ))    && parts+=("$fail FAILED")
    (( pending )) && parts+=("$pending running")
    (( cancel ))  && parts+=("$cancel CANCELLED")
    (( skip ))    && parts+=("$skip skipped")
    (( total ))   || parts+=("no checks")

    local summary
    summary=$(printf '%s, ' "${parts[@]}"); summary=${summary%, }

    # The sentence the ad-hoc filter could not produce. A pull request with
    # cancelled checks and nothing running will never go green on its own; it
    # needs a re-run, and saying so is the whole point of this script.
    # A pull request based on another pull request's branch got no build at all
    # while the workflow only triggered on `branches: [main]`. It showed two
    # green checks -- changelog and the review bot -- and "0 failing", which
    # reads exactly like a passing build. Nothing distinguishes "the matrix
    # passed" from "the matrix never ran" except looking for the matrix.
    local matrix
    matrix=$(jq '[.[]|select(.name|test("ubuntu-latest|macos-latest|windows-latest"))]|length' <<<"$json")

    local verdict="" stale
    stale=$(stale_against_head "$pr" || true)
    if [[ -n "$stale" ]]; then
        # Said first, because every other verdict below describes checks that
        # belong to a commit which is no longer the head.
        verdict="  <- STALE: these checks ran against ${stale#stale }, not the current head"
    elif (( total == 0 )); then
        verdict="  <- nothing reported yet"
    elif (( fail )); then
        verdict="  <- failing"
    elif (( cancel && pending == 0 )); then
        verdict="  <- WAITING ON NOTHING: re-run it"
    elif (( matrix == 0 )); then
        local runs state
        runs=$(build_run_count "$pr")
        state=$(merge_state "$pr")
        if [[ "$state" == "CONFLICTING" ]]; then
            verdict="  <- CONFLICTS with the base: GitHub runs nothing until it is rebased"
        elif (( runs > 0 )); then
            verdict="  <- build queued for this commit but not yet reported"
        else
            verdict="  <- NO BUILD MATRIX: these checks do not include a build"
        fi
    elif (( total && total == pass )); then
        verdict="  <- ready"
    fi

    printf 'PR%-5s %s%s\n' "$pr" "$summary" "$verdict"
}

if [[ $# -eq 0 ]]; then
    echo "usage: $0 <pr-number> [pr-number...]" >&2
    exit 2
fi
for pr in "$@"; do status_line "$pr"; done
