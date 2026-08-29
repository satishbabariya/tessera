# Finding: two environment-dependent test problems that will break CI

Date: 2026-08-29
Task: Phase 0a Task 11 (surfaced while chasing an apparent 8x slowdown)

Both were first mistaken for regressions caused by the Task 10 deletions. Neither
is. Both must be handled before CI (Tasks 4-5) can be trusted.

## 1. `sync: error handling / reports DNS error` is flaky and network-dependent

`test/object-store/sync/session/session.cpp:371` connects to a bogus hostname and
waits up to 35s for a DNS-failure callback. Four runs of the **same binary, same
code**:

| Run | Duration | Result |
|---|---|---|
| 1 | 4.177 s | pass |
| 2 | **680.625 s** | **FAIL** — "timed_wait_for exceeded 35000 ms" |
| 3 | 13.729 s | pass |
| 4 | 0.011 s | pass |

A 60,000x spread in wall-clock time, and one outright failure, with no code
change between runs. The variable is how the local resolver handles a name that
does not exist: some networks answer NXDOMAIN instantly, others hang, and some
ISPs hijack the lookup and answer slowly with a wildcard address.

**Action for Tasks 4-5:** this test must not gate merges. Tag network-dependent
tests and exclude them from the merge gate, running them in nightly only. A
merge gate that fails on the CI runner's DNS configuration trains everyone to
ignore red builds, which costs far more than the coverage is worth.

## 2. The test suite leaks temp directories, and a large TMPDIR degrades it badly

While chasing the slowdown, `TMPDIR` was found to hold **48,207 entries**,
including **1,471 leaked `realm_*` directories** accumulated over one session of
repeated (and sometimes interrupted) test and benchmark runs.

The effect is severe and non-obvious:

| Test | Dirty TMPDIR (48k entries) | Isolated / clean |
|---|---|---|
| `should throw when creating the notification pipe fails` | **243.8 s** | **0.006 s** |

Four orders of magnitude, for a test that creates a FIFO in the system temp
directory. Directory lookups degrade sharply once a directory is large enough,
and this suite touches temp paths constantly.

**Actions:**
- The suite should clean up after itself. 1,471 leaked directories in a single
  session is a defect worth fixing in its own right.
- CI must use a fresh `TMPDIR` per run, which containers give for free but local
  development does not.
- Developers seeing an inexplicably slow suite should check
  `ls -1 "$TMPDIR" | wc -l` before suspecting their changes.

## Why this matters beyond the two tests

Both problems produce **misleading evidence about a code change**. The 8x
slowdown looked exactly like a regression introduced by removing App Services,
and the DNS failure looked exactly like a broken sync path. Neither was. Had
either been accepted at face value, the conclusion would have been that the Task
10 deletion damaged the tree.

The general rule this reinforces: when a measurement changes, establish that the
*measurement conditions* were constant before concluding the *code* is at fault.
Re-running in isolation cost minutes and prevented a wrong diagnosis twice.
