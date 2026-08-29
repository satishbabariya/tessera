# An inventory of the tests that do not run

Several findings here concern tests that were not running and did not say so.
This counts them all, so the answer to "what is not being tested" is a number
rather than a discovery.

Measured on `main`, Linux Debug, after the fixes in #6, #7, #11 and #12.

| | Count | Visible? |
|---|---|---|
| Run and pass | 1,664 | yes, in the total |
| Disabled by a `TEST_IF` condition | 31 | **yes, since #6** -- the framework computed this and printed nothing |
| Inside an `#if 0` region | 10 | no |
| Compiled out by a platform or feature `#if` | varies | no |
| Never compiled at all | 0 | was 3, all fixed |

## The `#if 0` regions

Six of them, ten tests:

| | |
|---|---|
| `test_shared.cpp:1996-2211` | `Shared_WaitForChange`, `Shared_WaitForChangeAfterOwnCommit`, `Shared_InterprocessWaitForChange` -- `// FIXME: Reenable when it can pass reliably` |
| `test_sync.cpp:1054-1313` | `Sync_NonDeterministicMerge` |
| `test_util_future.cpp` (two regions) | four `Future_MoveOnly_*_getNothrow*` tests |
| `test_lang_bind_helper.cpp:310-359` | `Transactions_ConcurrentFrozenQueryAndObjAndTransactionClose` |
| `test_shared.cpp:2828-2890` | `Shared_encrypted_pin_and_write` |

These are honestly disabled. Each carries a comment, and `#if 0` is unambiguous
in a way that a three-deep guard is not. What they share with the accidental
cases is that nothing reports them: a `#if 0` test is not disabled, it does not
exist, and no total is short by ten.

## What this means for one README claim

> The trade-off worth knowing up front: **one write transaction at a time**,
> across all threads and processes.

The mechanism is tested. `Thread_InterprocessMutexTryLock` exercises the
interprocess mutex directly and passes with four checks.

The database-level interprocess tests are the three in the `#if 0` above. So the
claim is verified at the level of the lock and not at the level of two processes
opening the same file and contending for the write transaction. That is a real
gap, and it is a different kind from the ones in the other findings: someone knew
about it, wrote the test, found it unreliable, and disabled it deliberately
rather than silently.

Re-enabling it is not free -- "when it can pass reliably" is doing a lot of work
in that comment, and a flaky test on the merge gate is worse than a disabled one.
Recording it here is not a plan to fix it. It is so that the next person reading
the README's trade-off knows exactly how much of it is checked.

## Why the count matters more than any single entry

Before today the honest answer to "how many tests do not run" was that nobody
knew, and the suite total could not tell you: 1,662 passing tests is the same
sentence whether the 1,663rd ran, was switched off, or was never compiled.

Now three of those four states are countable and one is reported automatically.
That does not make the untested things tested. It makes them visible, which is
the prerequisite.
