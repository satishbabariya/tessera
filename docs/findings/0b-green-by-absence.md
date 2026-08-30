# A check that never ran looks exactly like a check that passed

Twice now the merge stack has been read as healthy when it was not, and both
misreadings share one shape: **something was absent, and absence rendered as
success.**

## The two cases

**#31 had no build matrix.** `build.yml` triggered on
`pull_request: branches: [main]`, so a pull request based on another feature
branch got no build at all. `gh pr checks` listed `changelog` and `CodeRabbit`,
both green. Asked "how many are failing", the answer was zero. It was one step
from being merged on that basis. Fixed in #32 by broadening the trigger; the
finding is that the question "how many are failing" cannot distinguish a passing
build from an absent one.

**#29 and #30 had cancelled runs.** Both sat for roughly two hours while being
polled with this filter:

```
gh pr checks $p --json name,state --jq '"\([.[]|select(.state!="SUCCESS")]|length) of \(length)"'
```

which printed `5 of 7` on every poll. Read, every time, as five checks still
running. They were not running. `concurrency.cancel-in-progress` had cancelled
them, and a cancelled run is terminal -- it will never finish, so waiting for it
is waiting forever. The stack was not slow; it was stopped, and the poll could
not say so.

## Why the poll could not say so

`gh pr checks` has always reported a `bucket` field alongside `state`, and its
values are `pass`, `fail`, `pending`, `cancel` and `skipping`. `cancel` and
`pending` are different buckets. The filter asked only whether the state was
`SUCCESS`, which collapses every non-success into one number, so a terminal
failure to run and an in-progress run produced the same string.

Recorded check output, four situations, the old filter against the new tool:

| situation | `state != SUCCESS` | `tools/pr-status.sh` |
|---|---|---|
| 5 cancelled, 2 passed | `5 of 7` | `2 passed, 5 CANCELLED  <- WAITING ON NOTHING: re-run it` |
| 5 running, 2 passed | `5 of 7` | `2 passed, 5 running` |
| 7 passed | `0 of 7` | `7 passed  <- ready` |
| 1 failed, 6 passed | `1 of 7` | `6 passed, 1 FAILED  <- failing` |
| 2 passed, no build job at all | `0 of 2` | `2 passed  <- NO BUILD MATRIX: these checks do not include a build` |

The first two rows are the ones that stalled the stack: indistinguishable in the
middle column, distinct in the right one. The last row is #31, and its middle
column reads *better* than the genuinely-built row above it -- `0 of 2` against
`0 of 7` -- because counting failures rewards having fewer checks.

## What was done

`tools/pr-status.sh` reports checks by bucket and names the two silent states
outright. It reads the checks through one function that `PR_STATUS_FIXTURE`
can replace with recorded JSON, because a cancelled run cannot be conjured on
demand against a live repository, and a check that has never been shown to fire
is not a check.

The table above is `tools/testdata/pr-status/*.json`, asserted by
`tools/test-pr-status.sh`, which runs in CI with the other invariant checks. Its
last assertion is the finding itself: a cancelled stack and a running stack must
not render the same string. Both guards were canary-tested by reverting them --
restoring the failure-counting behaviour makes the cancelled fixture print
`2 passed, 5 running`, and removing the matrix guard makes the #31 fixture print
`2 passed  <- ready`.

One rigged attempt is worth recording: the first fixtures emitted
`state: "PASS"` for passing checks, where real `gh` emits `SUCCESS`. Every row
came out `7 of 7` and the old filter looked far worse than it is. The
comparison above uses the state strings `gh` actually returns.

## The general form

This is the third recurring pattern in these findings, after "the test binary
was stale" and "the check was never shown to fail". All three are the same
error: **treating the absence of a negative signal as a positive one.** Zero
failures is not a pass. No output is not a clean run. A check that was never
executed reports nothing, and nothing is what success looks like from a
distance.

The defence is always the same and always costs something: assert on what
should be *present*, not on what should be missing.
