# Finding: the engine is clean under all three sanitizers

Date: 2026-08-29
Task: first nightly CI run
Run: https://github.com/satishbabariya/tessera/actions/runs/33243581844

## Result

The full CoreTests suite -- 1652 tests -- passes under each of:

| Sanitizer | Result |
|---|---|
| AddressSanitizer | **clean** |
| UndefinedBehaviorSanitizer | **clean** |
| ThreadSanitizer | **clean** |

Built with clang-18 on Linux x86-64, Debug, `UNITTEST_THREADS=1`.

## A prediction that was wrong

TSan was expected to report something. The reasoning: this engine's concurrency
model is a shared-memory interprocess mutex plus a **lock-free version
ringbuffer**, and lock-free code is exactly what TSan flags -- if not for genuine
races then for patterns it cannot prove safe, such as relaxed-ordering atomics or
ABA-avoidance schemes.

It found nothing.

## Why this matters more than a green tick

These are not easy passes for this particular codebase:

- **ASan** on a memory-mapped storage engine with a hand-rolled slab allocator,
  bit-packed integer arrays, and pointer arithmetic throughout.
- **UBSan** on code that manipulates bit widths and type-puns deliberately as
  part of its storage format.
- **TSan** on hand-written lock-free reader registration shared across
  *processes*, not merely threads.

The Phase 0a assessment described the inherited engine as "high-quality,
battle-tested". That was inherited framing repeated without evidence. This is the
first hard evidence for it.

## Consequence for Phase 1

Phase 1 reworks the sync client, which touches this concurrency machinery. There
is now a **verified-clean baseline**: if TSan reports something after that work,
it is new, and the question is what changed rather than whether it was always
there.

Establishing that baseline before the work rather than after is the whole value
of running these now.

## What is not covered

- Only CoreTests runs under the sanitizers. SyncTests and ObjectStoreTests do
  not, because they are substantially slower and the nightly already takes
  around half an hour. Extending coverage to the sync suite would be worthwhile
  before Phase 1 touches the sync client.
- Linux x86-64 with clang only. The sanitizers are not run on macOS or with gcc.
- `UNITTEST_THREADS=1`. Higher thread counts would exercise more interleavings,
  at the cost of runtime.
