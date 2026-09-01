# GitHub reports a timed-out job as "cancelled"

`ObjectStoreTests` hung on every platform. The matrix ran for an hour and was
killed by the job's `timeout-minutes: 60`. What that produced, in the API and in
`gh pr checks`, was:

```
conclusion: cancelled
```

Which is the same word GitHub uses when `concurrency.cancel-in-progress`
supersedes a run. One means *this code deadlocks*; the other means *a newer push
arrived*. They are not close to the same thing, and nothing in the check list
distinguishes them. The hang read as "still running" for about two hours.

## The measurement

A step's own `timeout-minutes` is reported differently from a job's. Rather than
assume, a throwaway workflow was pushed with one step that sleeps past its own
one-minute cap:

```
JOB probe: completed/failure
    1. Set up job: completed/success
    2. A step that finishes: completed/success
    3. A step that overruns its own timeout: completed/failure
    4. A step after the timeout: completed/skipped
    5. Complete job: completed/success
```

**Step-level timeout → `failure`. Job-level timeout → `cancelled`.**

## What was done

Every long step in `build.yml` now carries its own `timeout-minutes`, so a hang
reports as a failure at the step that hung, naming it. The job-level 60 stays as
a backstop for anything the per-step caps miss.

The caps are several times the observed durations -- on a passing
`macos-latest clang Release` run the steps took 6m51s, 34s, 43s, 24s and 9s:

| step | observed | cap |
|---|---|---|
| Build everything | 6m51s | 35m |
| CoreTests | 34s | 20m |
| ObjectStoreTests | 43s | 20m |
| SyncTests | 24s | 25m |
| Consumer smoke test | 9s | 10m |

A cap that fires on a slow runner rather than a real hang would be worse than no
cap, because a merge gate that fails for reasons unrelated to the change trains
everyone to ignore it -- which is why `SyncTests` is excluded from the gate over
a DNS-dependent test.

## A second, smaller instance of the same error

`tools/pr-status.sh` announced `NO BUILD MATRIX` for a pull request whose build
was seconds from appearing. Right after a push the changelog check can be
reported and green while the matrix is still being created, and nothing is
pending -- so a verdict drawn from the check list alone concludes the build is
absent rather than not yet registered.

That is [0b-green-by-absence.md](0b-green-by-absence.md) again, in the tool
written to catch it: an absent signal read as a settled answer. The fix is to
ask a question the check list cannot answer -- whether a build run exists for
the head commit -- and to report the two cases in different words.
