# Where the CI minutes go, and why the obvious saving is not worth taking

`SyncTests` runs with `UNITTEST_THREADS=1` in the merge gate. Nothing recorded
why. In `nightly.yml` the same setting is a documented choice -- one thread for
the sanitizer baseline, so that a TSan report is about the code and not about
the runner -- but the merge gate's copy had no reason attached, and it predates
the rename.

So it was measured. Twelve runs of the suite on macOS:

| Threads | Runs | Result | Elapsed |
|---|---|---|---|
| 1 | 4 | 476/476 pass | 21.1s |
| 4 | 5 | 476/476 pass | 6.8-7.3s |
| 8 | 4 | 476/476 pass | 5.2-10.5s |

Parallel works, and it is three times faster.

## It stays at one thread

The step takes **2.9 minutes of a 21-minute job**. `Build everything` takes
**13.6**. Measured on `ubuntu-latest gcc Release`:

```
13.6m  Build everything
 3m    ObjectStoreTests
 2.9m  SyncTests
 1m    CoreTests
```

So the whole prize is about two minutes, under a tenth of the job, and buying it
means a concurrency change to the merge gate on evidence from one platform. A
race that passes twelve times can fail the thirteenth, and this repository
already knows what that costs -- the note beside the flaky DNS test in the same
file says a merge gate that fails on conditions unrelated to the change "trains
everyone to ignore red builds".

Two minutes is not worth that. The measurement is still worth having: the
setting is now a decision rather than an inheritance.

## The build is the cost

13.6 minutes, five jobs per pull request, and **no compiler cache anywhere** --
no `ccache`, no `sccache`, no `actions/cache` in any workflow. A warm `ccache`
on a pull request touching two files would take that to a minute or two.

It is not added here, and the reason is worth writing down rather than leaving
as an omission. A build cache is an input to the compiler that the compiler
trusts. GitHub scopes caches by ref, so a pull request cannot write the default
branch's cache, but the default branch's cache is readable by every pull request
and a stale or wrong entry is a silently miscompiled object. For a project that
pins dependency tarballs by measured digest and refuses vendored hosts, adding
an unauthenticated artifact store to the merge-gate build is a decision to make
deliberately, with the key strategy and fork policy worked out, and not as a
speed tweak.

Recorded as the next lever, with its prize measured, rather than taken quietly.

## Check counts here are not reproducible

Across those twelve runs, with all 476 tests passing every time:

```
39,039   39,497   38,999   91,450   103,655   105,117
113,335  118,762  122,761  124,383  124,463   125,116
```

A 3.2x spread, at every thread count including one.

The obvious explanation is wrong. `test_all.cpp` seeds the framework's random
generator from `produce_nondeterministic_random_seed()` unless
`UNITTEST_RANDOM_SEED` is set, so randomised tests do different work on every
run -- which would explain all of this. Pinning the seed does not fix it:

```
UNITTEST_RANDOM_SEED=1234, one thread:   107,180   123,057   123,252
UNITTEST_RANDOM_SEED=1234, four threads: 119,866   103,380
```

So the seed is not the cause. Bisecting by test-name prefix, three runs each,
finds where the checks are and where they move:

```
Sync*         1916    1916    1916
Transform*     608     608     608
Util*         1679    1679    1679
ClientReset*   580     580     580
Network*    120080  116776  112135
```

`Network*` holds almost the entire check count of the suite, and all of the
variance. Within it:

```
Network_RepeatedCancelAndRestartRead   84220   67377   85395
Network_AsyncReadWriteLargeAmount      16646   16646   16646
Network_ReadWriteLargeAmount           16390   16390   16390
```

One test, about 70% of the suite's checks, and unstable by tens of thousands.

## Why that test cannot have a stable count

`Network_RepeatedCancelAndRestartRead` pushes 64 MiB through a socket pair. Its
read handler checks the error code and re-arms itself:

```cpp
auto handler = [&](std::error_code ec, size_t n) {
    num_bytes_read += n;
    if (ec == MiscExtErrors::end_of_input) { end_of_input_seen = true; return; }
    CHECK(!ec || ec == error::operation_aborted);
    initiate_read();
};
```

So the check count *is* the number of read completions. Meanwhile the writer
loops, writing a random 1-1024 bytes at a time and posting a
`socket_2.cancel()` after each write, while the reader runs `service_2.run()`
on another thread.

How many times that handler fires is decided by the kernel's socket buffering
and by how the cancels interleave with the reads across two threads. Pinning
the random seed fixes the write sizes and changes none of that, which is exactly
what the measurement showed.

The count is not flaky in the sense of a test that sometimes fails. It passes
every time. It is simply not a quantity this test controls, and the suite's
headline check total is mostly this one test's socket scheduling.

(The test body is wrapped in `for (int i = 0; i < 1; ++i)`, a loop that runs
once. Left alone: it is inert, and this was a measurement, not a cleanup.)

## CoreTests is worse, and it is the thread count

Four runs of one Release binary at `UNITTEST_THREADS=2`:

```
93,027,810   12,417,707   10,188,745   48,889,143
```

A 9.1x spread, all 1659 tests passing every time. Three runs of the same binary
at `UNITTEST_THREADS=1`:

```
99,508,221   99,442,447   99,655,290
```

Stable to 0.2%, 38-39 seconds each. So this is not a test whose work varies. It
is the parallel runner's accounting, and CI runs CoreTests with
`UNITTEST_THREADS: 2`.

Bisecting by name prefix found nothing that could account for it -- the largest
groups measured single-threaded are `Table*` at 4.2M, `LangBindHelper*` at 3.5M
and `EncryptedFile*` at 3.2M, all stable across repeat runs. Nothing in one test
explains a swing of eighty million.

### There is a real accounting defect in the framework

`test/util/unit_test.cpp`. Each thread accumulates into its own plain
`num_checks` and folds it into the shared atomic total in `finalize()`. The
thread that will go on to run the nonconcurrent tests does not do that:

```cpp
if (!shared_context.no_concur_tests.empty()) {
    int num_remaining_threads = shared_context.num_threads - shared_context.num_ended_threads;
    if (num_remaining_threads == 1) {
        shared_context.last_thread_to_end = thread_index;
        return;                       // <- returns without finalize(lock)
    }
}
++shared_context.num_ended_threads;
finalize(lock);
```

and `nonconcur_run()`, which that thread runs next, begins by discarding what it
was holding:

```cpp
void TestList::ThreadContextImpl::nonconcur_run()
{
    clear_counters();                 // <- num_checks = 0
    ...
    finalize(lock);
}
```

So one thread's entire concurrent-phase check count is dropped on every run,
single-threaded included. The counter itself is atomic and not racy; the checks
are simply never added.

### What that does not explain

The direction. If a fixed share were dropped, the multi-threaded totals would
straddle the single-threaded one rather than sitting far below it, and they
would not vary by a factor of nine. Which thread ends last, and how much of the
suite it happened to have run, varies with scheduling -- that accounts for
instability but not obviously for a deficit of eighty million against the
single-threaded figure.

So: the accounting is demonstrably wrong, the number moves by 9x with thread
count, and the mechanism above is not sufficient on its own. What follows from
that is the same either way -- the figure cannot be used -- and it is written
down here rather than resolved, because resolving it means changing the test
framework and that is not this change. That has a consequence beyond
this suite, because `docs/RELEASING.md` tabulated assertion counts as a
pre-release baseline -- one Debug sample and one Release sample each -- and drew
inferences from the differences:

> The two suites where Release is several times Debug are not a mistake and not a
> stale binary [...] Nobody has yet explained the ratio, which is why it is
> written down.

The ratio may not exist. CoreTests' recorded Debug-to-Release gap is 1.14x,
inside a 9.1x noise band. SyncTests' recorded Debug 40,400 and Release 127,236
both fall inside the range Release alone produces, so those two numbers are not
evidence of a Debug-to-Release difference; they are two draws from one
distribution. And the recorded Release figures, 102,273,888 for CoreTests and
127,236 for SyncTests, are each above every run measured here.

The document was honest about the numbers being noisy -- "treat a small drift as
normal and a factor as worth explaining" -- and then explained a factor that was
noise. The fix is not a better number. It is to stop comparing them: test counts
are stable at 1659 / 476 / 343 every run and are the ones to check, and a test
that stops asserting anything is what `tools/analyse-zero-check-tests.sh` is
for, counting per test rather than per suite.

## The consequence

This project uses check-count baselines to detect tests that stop checking
anything -- the "green by absence" problem -- and for `SyncTests` such a
baseline would be meaningless. Pinning the seed does not rescue it, because the
variance is not in the seed.

What would work is a baseline that excludes
`Network_RepeatedCancelAndRestartRead`, or a per-test count for the other 475.
`tools/analyse-zero-check-tests.sh` already runs tests one at a time, because
the framework tracks `num_executed_checks` only in its `Summary` and never per
test -- so a per-test baseline is possible today, just not cheap.

Neither is built here. What is established is that the suite total cannot serve
as one, and precisely why.
