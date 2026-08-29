# A public constant that contradicted its own implementation

`RobustMutex::is_robust_on_this_platform` was `false` in every translation unit
in the project. Including `thread.cpp`'s own, where the class lives.

    $ ./tessera-tests   # with the guard satisfied by disabling encryption
    Success: All 1 tests passed (0 checks). Test time: 20us

Twenty microseconds, no checks. The test's first statement returns unless the
constant is true.

## The mechanism

`thread.hpp` declares the constant:

    #ifdef TESSERA_HAVE_ROBUST_PTHREAD_MUTEX
        constexpr static bool is_robust_on_this_platform = true;
    #else
        constexpr static bool is_robust_on_this_platform = false;
    #endif

`thread.cpp` defined that macro -- fourteen lines *after* including `thread.hpp`.
By the time the preprocessor reached the `#define`, the class was already parsed
with `false`.

The implementation a hundred lines further down the same file then compiled full
robust-mutex support, because *its* `#ifdef` appears after the defines rather
than before. So `RobustMutex` used robust mutexes and told everybody it could
not, on every platform, through an installed public header.

Five tests gate on the constant. None of them had ever executed a check.

## Why it looked like something else, twice

The first reading was "the test is guarded `#if TESSERA_PLATFORM_APPLE`, so it is
verified on Apple and not on Linux". That is what the inner guard says, and it is
the sort of thing tests legitimately have.

The second was "an ODR violation: `thread.cpp` sees `true`, consumers see
`false`". A probe seemed to confirm it -- but the probe defined the macro *before*
including the header, which is not what `thread.cpp` does. It demonstrated a
hypothesis rather than the file. The include order is both simpler and worse:
nobody sees `true`.

Both wrong readings came from arguing about the code. The thing that settled it
was `_POSIX_THREADS = 200112L` printed from a probe, the constant read out of the
built library, and a CI test count that did not move.

## What it was hiding

Fixing it raised macOS `CoreTests` from 1652 to 1653. `Thread_ProcessSharedMutex`
is guarded on `TESSERA_HAVE_PTHREAD_PROCESS_SHARED`, defined in the same block, so
that test had never compiled in any translation unit either. One misplaced
definition accounted for a wrong public constant and two dead tests.

## The fix, and the mistake inside the fix

The detection moved into `thread.hpp`, which declares the constant: a value a
header exposes has to be computed where the header can see it.

The first attempt anchored the insertion on the `<condition_variable>` include
without checking that the include sits inside the header's
`#ifdef _WIN32 / #else / #endif` platform split. The block therefore compiled only
on Windows, `TESSERA_HAVE_PTHREAD_PROCESS_SHARED` was undefined everywhere else,
and `InterprocessMutex` threw "No support for process-shared mutexes" from most
of the Linux suite.

macOS reported a clean build and 1652 passing tests throughout, because Apple
uses a different interprocess mutex implementation that never reaches that path.
Linux CI failed on the first run.

The second attempt was verified with the preprocessor rather than by reading:

    TESSERA_HAVE_PTHREAD_PROCESS_SHARED : defined
    TESSERA_HAVE_ROBUST_PTHREAD_MUTEX   : NOT DEFINED
    is_robust_on_this_platform          : false

which is correct for macOS: `_POSIX_THREADS` is 200112L where the check requires
200809L.

## The check

`tools/check-header-macros.sh` fails when a header decides something on a
`TESSERA_` macro that only a `.cpp` defines. It checks 287 macros and found
exactly one instance -- this one. Canary-tested by restoring the pre-fix
`thread.hpp` and `thread.cpp`, against which it fails with the diagnosis above.

It does not cover the second mistake: a definition placed in the right file and
the wrong `#if`. Nothing static distinguishes that from a deliberate
platform guard. What catches it is a build on the platform being guarded, which
is what happened.
