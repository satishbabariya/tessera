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

## And both tables measure cold start

Every figure above is one process, started, run once, and stopped. Running ten
rounds against a single long-lived server says something different:

| round | 8 clients |
|---|---|
| 1 | 675/s |
| 2 | 810/s |
| 3-10 | ~2,800/s, stable |

and at sixteen clients the steady state is about 3,500/s against the 1,821/s a
single round reports. So the published numbers understate sustained throughput
by roughly a factor of two, because they include the cost of a cold server: an
empty file cache, a database being created rather than opened, pages not yet
resident.

Neither figure is wrong. They answer different questions -- "how long does one
burst take against a fresh server" and "what does this sustain" -- and only the
second is what anyone deploying it wants to know. The first was the only one
measured because the load test had never been run twice in a row.

18,600 transactions across those sixteen rounds, every one committed and
uploaded, no client failures, nothing in the server log at error level or above,
and a data directory of 1.0M. That is the first evidence this project has that
the server survives being used more than once.

Both tables are kept, because the pair is the useful artefact. What they
establish is the thing that did not exist before: a number to compare the next
one against, and a demonstration that the configuration has to be named beside
it.
