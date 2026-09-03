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

So the seed is not the cause, or not the only one. The shape of the numbers
suggests two effects rather than one: a spread of roughly 20,000 that survives a
pinned seed on a single thread, and a much larger drop to about 39,000 seen only
at four and eight threads. Neither is explained. `Transform_Randomized`,
`Network_StressTest` and `Util_Network_SSL_StressTest` were each measured in
isolation and are stable, so the variable test is elsewhere and has not been
identified.

What this does settle is the consequence. This project uses check-count
baselines to detect tests that stop checking anything -- the "green by absence"
problem -- and for `SyncTests` such a baseline would be meaningless. The
tempting fix, pinning the seed, has been tried and does not produce a
reproducible count, so a baseline here cannot be rescued that way.
