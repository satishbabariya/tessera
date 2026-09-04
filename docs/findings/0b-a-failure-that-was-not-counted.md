# The test framework reported success while a test failed

`tessera-tests` with `UNITTEST_THREADS=2`, with one deliberately failing test
added to the suite:

```
run 1: exit=0   All 1660 tests passed      <- the failure is gone
run 2: exit=1
run 3: exit=0   All 1660 tests passed      <- gone again
run 4: exit=1
```

Two of four runs reported complete success, exit code 0, with a test that had
just failed. CI runs CoreTests with `UNITTEST_THREADS: 2`.

## Why

`test/util/unit_test.cpp`. Each thread accumulates `num_checks`,
`num_failed_checks` and `num_failed_tests` into its own `ThreadContextImpl`, and
`finalize()` folds those into the shared totals. The suite's exit status is
`shared_context.num_failed_tests == 0`.

The thread that will go on to run the nonconcurrent tests does not call
`finalize()`:

```cpp
if (!shared_context.no_concur_tests.empty()) {
    int num_remaining_threads = shared_context.num_threads - shared_context.num_ended_threads;
    if (num_remaining_threads == 1) {
        shared_context.last_thread_to_end = thread_index;
        return;                         // <- no finalize(lock)
    }
}
++shared_context.num_ended_threads;
finalize(lock);
```

and `nonconcur_run()`, which that same thread runs next, threw away what it was
holding:

```cpp
void TestList::ThreadContextImpl::nonconcur_run()
{
    clear_counters();                   // <- num_checks, num_failed_checks,
    ...                                 //    num_failed_tests all set to 0
    finalize(lock);
}
```

So one thread's entire concurrent-phase accounting was discarded on every
multi-threaded run: its checks, and its failures. Whether a failure survived
depended on which thread happened to finish last.

## Why one thread was safe

At one thread, every test is nonconcurrent:

```cpp
auto& tests = (test.allow_concur && num_threads > 1 ? concur_tests : no_concur_tests);
```

`concur_tests` is empty, the concurrent phase counts nothing, and the discard
throws away zero. That is why single-threaded totals were exactly right and
multi-threaded ones were not -- and why `SyncTests`, which CI runs at one
thread, was never exposed.

## The fix

Delete the `clear_counters()` call. The last thread then carries its
concurrent-phase counters into the nonconcurrent phase and `finalize()` folds
the combined total once, which is what every other thread already does.

Measured, CoreTests, same binary, same machine:

| | before | after |
|---|---|---|
| checks at 2 threads | 10,188,745 / 12,417,707 / 48,889,143 / 93,027,810 | 99,637,487 / 99,772,242 / 99,837,290 |
| checks at 4 threads | -- | 100,327,611 |
| true total (1 thread) | 99,505,110 | 99,505,110 |

and `SyncTests` at four threads went from a 39,039-125,116 range to 119,612 /
121,653 / 120,589.

## How it was found, and how it was nearly missed

By measuring something else. The check count varied ninefold between runs, which
looked like a test doing variable work; bisecting by name prefix found no test
that could account for it, and the count turned out to be stable at one thread
and unstable at two.

The first attempt at a mechanism was written up and published before it was
tested. It was this same code, read correctly and applied wrongly -- it predicted
that single-threaded runs would lose their checks too, which would have meant a
total of about 1.1 million rather than the 99.5 million actually reported. A
prediction off by a factor of ninety, from a mechanism that is nonetheless real.

The missing step was cheap: derive a number from the mechanism, then go and
measure that number. Doing it revealed the `num_threads > 1` condition that makes
one thread safe, which is the part that makes the whole thing consistent.

That a wrong explanation and a right one can be read out of the same twenty
lines is the reason a check that has never failed is not a check, and the reason
this one was tested by adding a failing test and watching the suite call it a
pass.

With the fix, the same deliberately failing test at two threads reported
`exit=1` on six runs out of six.

## A guard, and why the first one was useless

`test/test_unit_test_framework.cpp` is the first test in this repository aimed at
the test framework rather than at the code under test. It builds an inner
`TestList` holding one failing test, runs it silently at 1, 2 and 4 threads, and
requires `run()` to report failure every time.

The first version of it passed against the bug it was written for.

Sixty-five trivial tests and one failing one are grabbed almost entirely by
whichever thread starts first. That thread finishes before any other reaches the
end-of-run check, so it takes the normal path, calls `finalize()`, and its
failure is counted -- on a broken framework. Nothing ever landed on the thread
that ends last, which is the only thread that loses anything.

Making the failing test sleep for 60ms and putting it first in the list fixes
that: the thread that picks it up is still working while the others run out of
fast tests and finish, so it is deterministically the last thread to end. The
test now fails on the buggy framework at 2 and 4 threads, passes at 1, and
passes at all three once fixed.

A regression test that does not fail on the regression is decoration. This one
was, for about ten minutes, and only running it against the reintroduced bug
showed that.

## What was exposed

Only `CoreTests` in the merge gate, which runs at `UNITTEST_THREADS: 2`.
`SyncTests` runs at one thread there, the nightly sanitizer jobs run both at one
thread, and `ObjectStoreTests` uses Catch2 and not this framework at all.

So the window was one job -- and it is the job that runs most of the project's
tests on every pull request.
