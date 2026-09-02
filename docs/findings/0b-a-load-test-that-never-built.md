# The load test could never have compiled

`v0.3.0`'s release notes say, under what the release does not promise:

> No load testing has been done. The server is the one Realm shipped and it is
> exercised by 477 tests, but nobody has run it under production traffic.

That was true, and this is why. `test/benchmark-sync/load_test.cpp` -- 163 lines
of argument parsing, statsd hostnames, client ids, transaction counts -- opens
with:

```cpp
#include "load_tester.hpp"
```

There is no `load_tester.hpp`. Not in `test/`, not in `src/`, not anywhere in
the repository, and not anywhere in its history:

```
$ c++ -fsyntax-only -std=c++20 -Isrc test/benchmark-sync/load_test.cpp
test/benchmark-sync/load_test.cpp:4:10: fatal error: 'load_tester.hpp' file not found
$ git log --all --diff-filter=D -- '*load_tester*'
(nothing)
```

Beside it sat `load_test_clients_listen_start.sh`, a script that starts a fleet
of clients by invoking a binary that was never produced.

## Why nobody noticed

`test/benchmark-sync/CMakeLists.txt` names one source:

```cmake
add_executable(tessera-benchmark-sync bench_transform.cpp ../test_all.cpp)
```

So `load_test.cpp` was never compiled, and a file that is never compiled cannot
fail to compile. It sat in a directory called `benchmark-sync`, in a repository
whose release notes correctly said no load testing had been done, and the two
facts never met.

This is the fourth thing found in this state, and the pattern is now familiar
enough to name:

| | |
|---|---|
| [`tools/verify/clean-clone-test.sh`](0b-uncompiled-test-file.md) | written for a real failure, referenced by no workflow |
| [`tools/pre-push`](0b-hooks-nobody-runs.md) | rejected this repository's own remote |
| [`AccessControl::is_admin`](0b-is-admin-was-inverted.md) | returned true for a download-only token |
| `load_test.cpp` | could not compile |

All four were found by reading. None could have been found by testing, because
in each case the thing that would have failed was never run. Three of the four
were wrong, and the fourth -- this one -- was not even wrong: it was a fragment.

## What replaced it

A load generator that compiles, is built by `CMakeLists.txt`, and drives real
sessions against a real server over a real socket using only the installed
public API: N concurrent sessions, M write transactions each, unique primary
keys per client so that concurrent writers do not collide on one object and
measure lock contention instead of throughput.

It prints the numerator with the denominator, because a rate reported without
the count it came from cannot be checked, and exits non-zero if any client
failed or any transaction went missing. A load test that cannot fail is the
thing this document is about.

## The first numbers

Against `tessera-sync-server` on loopback, Debug build, everything on one
laptop:

| clients | transactions | time | rate |
|---|---|---|---|
| 4 | 200 | 0.39s | 514/s |
| 8 | 800 | 0.83s | 962/s |
| 16 | 1600 | 1.63s | 982/s |
| 32 | 1600 | 9.18s | 174/s |

All 3,600 transactions committed and uploaded; no client failed.

Throughput plateaus near 980/s between 8 and 16 clients and then falls by a
factor of five and a half at 32. That looks like a scalability cliff, and it
would have been tempting to write one down.

It is not. The same measurement in Release:

| clients | transactions | time | rate |
|---|---|---|---|
| 4 | 400 | 0.54s | 744/s |
| 8 | 800 | 0.66s | 1208/s |
| 16 | 1600 | 0.88s | 1821/s |
| 32 | 3200 | 1.93s | 1659/s |

No collapse. Throughput rises to about 1,800/s at sixteen clients and eases to
1,659/s at thirty-two -- a gentle decline of the kind contention normally
produces, not a cliff. The Debug figure of 174/s was the measuring apparatus:
Debug overhead multiplied across thirty-two clients and their thirty-two
socket-provider event loops, all competing with the server for the same eighteen
cores.

So the load test's first act was to correct an inference drawn from the load
test. Had the Debug run been the only one, "throughput collapses beyond sixteen
clients" would have gone into the documentation as a property of the server,
and it is a property of the build.

## Correction: what the repeated-round numbers actually measured

An earlier version of this section, and the changelog entry that shipped with
it, said that ten rounds against one long-lived server showed a steady state of
about 2,800/s against a published 1,208/s, and concluded that the published
figures understate sustained throughput by roughly a factor of two.

That was drawn from one confounded experiment and is withdrawn.

The load test writes primary keys of `index * 1000000 + i`. Those are identical
on every run, so a second run against the same server path **rewrites the first
run's rows instead of inserting new ones**. The "steady state" was the cost of
updating eight hundred existing objects, not of inserting eight hundred.

`--key-base` offsets the keys, which separates three cases that had been one:

| round | fresh server, fresh keys | same server, fresh keys | same server, same keys |
|---|---|---|---|
| 1 | 2128/s | 1097/s | 1153/s |
| 2 | 2198/s | 835/s | 2254/s |
| 3 | 2397/s | 620/s | 1784/s |
| 4 | 2533/s | 517/s | 1564/s |
| 5 | 2426/s | 400/s | 1217/s |

Against a fresh server the rate is flat at about 2,300/s, repeatably. Against a
server whose database is growing, insert throughput **declines** -- 1,097/s to
400/s across four thousand rows -- and each new client must also download
everything already there.

So the supportable statements are narrower than the withdrawn one:

- a fresh server sustains about 2,300/s for this shape of write, repeatably;
- insert throughput falls as the database grows, at least in this range;
- a figure quoted without saying which of those it measured is not a figure
  about the server.

The withdrawn claim is corrected in the changelog rather than deleted, because a
number that was wrong for a stated reason is more useful than one that quietly
disappeared.

The wider lesson is the one this file already carried, one level up. Its first
version said the load test's first act was to correct an inference drawn from
the load test. Its second act was to correct an inference drawn from its own
correction, and the cause both times was the same: a measurement whose
conditions were not stated, and therefore not checked.

Both tables are kept, because the pair is the useful artefact. What they
establish is the thing that did not exist before: a number to compare the next
one against, and a demonstration that the configuration has to be named beside
it.
