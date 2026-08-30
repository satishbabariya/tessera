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

    local verdict=""
    if (( total == 0 )); then
        verdict="  <- nothing reported yet"
    elif (( fail )); then
        verdict="  <- failing"
    elif (( cancel && pending == 0 )); then
        verdict="  <- WAITING ON NOTHING: re-run it"
    elif (( matrix == 0 )); then
        verdict="  <- NO BUILD MATRIX: these checks do not include a build"
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
