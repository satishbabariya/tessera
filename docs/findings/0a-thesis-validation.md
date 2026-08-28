# Finding: bundled sync server viability — **GREEN**

Date: 2026-08-28
Task: Phase 0a Task 6 (spec §5 step 0.5) — the thesis validation gate
Question: does the in-repo sync server at `src/realm/sync/noinst/server/` work?

## Verdict

**GREEN.** The server builds with zero errors and the entire sync test suite
passes. Phase 1 is a revival project, not a greenfield build. Proceed to 0b.

## Build

| Target | Result |
|---|---|
| `SyncServer` (as-is, before promotion) | **0 errors**, 7 warnings, linked `librealm-server-dbg.a` |
| `SyncTests` | **0 errors**, linked `realm-sync-tests` |

The server compiled *before* any intervention — the only change needed was
dropping `EXCLUDE_FROM_ALL` from `src/realm/sync/noinst/server/CMakeLists.txt:24`
so it builds by default rather than only on explicit request. No source changes
whatsoever were required, on C++20, with a 2026 toolchain, in a codebase whose
upstream stopped maintaining this component years ago.

## Tests

```
Realm - Success: All 463 tests passed (121505 checks).
Realm - Test time: 24.49s
```

Exit code 0. **Zero failures.** The only two lines in the log matching
`error|failure` are test *names* (`Sync_ReadFailureSimulation`,
`Util_Network_SSL_Certificate_Failure`), not results.

Breakdown of what actually executed:

| Prefix | Tests | What it covers |
|---|---|---|
| `Sync_` | 141 | Client↔server sync against the in-process bundled server |
| `Transform_` | 91 | **The operational-transform merge engine** — the crown jewels |
| `Util_` | 54 | Network, SSL, URI, misc utilities |
| `Network_` | 33 | Transport layer, incl. stress and cancel/restart |
| `ClientReset_` | 32 | Client reset and recovery |
| `WebSocket_` | 4 | Framing, fragmentation, interleaving |
| **Total** | **463** | all passing |

Server-backed tests: **173** (`Sync_` + `ClientReset_`). This exceeds the 147
estimated from counting `TEST` macros, because `TEST_TYPES` macros expand into
several test instances each. Server instantiation was confirmed in the log
rather than assumed.

Slowest tests, which double as evidence the heavy paths genuinely ran:

| Test | Time |
|---|---|
| `Network_RepeatedCancelAndRestartRead` | 1.98s |
| `Network_StressTest` | 1.62s |
| `Sync_HistoryMigration` | 1.24s |
| `ClientReset_ThreeClients` | 1.21s |
| `Transform_Randomized` | 1.19s |

`ClientReset_ThreeClients` and `Transform_Randomized` matter most here: the first
exercises multi-client convergence through a real server, the second randomised
concurrent-edit merging. Those are precisely the hard parts.

## Why this is the decisive result for Tessera

The strategy rests on a claim that looked optimistic when the plan was written:
that this repository contains a *working* open sync stack, not merely the remains
of one. It does.

- **Phase 1 changes character.** Not "write an open-source sync server" — a
  multi-quarter project with substantial risk — but "revive, modernise, and
  document the server already here, which has 173 passing integration tests."
- **The merge engine is proven, not merely present.** 91 `Transform_` tests pass,
  including randomised concurrent-edit convergence. This is the component every
  local-first competitor is currently building from scratch.
- **The 0b work is now worth doing.** Rename, carve-out, and API stabilisation
  are a lot of careful work; they would have been wasted effort under a RED
  verdict. Gating them behind this test was the right sequencing.

## What this does not establish

Stated plainly, so the GREEN is not over-read:

1. **macOS/arm64 only.** Linux and Windows are unmeasured; the transport layer
   is the most platform-sensitive code in the tree. Task 4 CI will tell us.
2. **Debug build only.** Release, and the sanitizers (especially TSan, given the
   server's threading), are unmeasured. Task 5.
3. **Correctness under test, not fitness for production.** These tests prove the
   protocol and merge logic converge. They say nothing about the server's
   performance under real concurrency, its operational maturity, its security
   posture, or whether its access-control and JWT model is one we want to keep.
   Those are Phase 1 questions.
4. **The protocol is the legacy full-sync generation**, not FLX. That was the
   intent — see `docs/findings/0a-i1-i2-flx-boundary.md` when written — but it
   means the wire protocol needs specifying before it can be called open.

## Consequence for the plan

No re-plan needed. Task 6 exit condition satisfied on the first branch: tests
pass, so 0b proceeds. Two follow-ups recorded:

- Add a `sync` job to nightly CI (Task 5) so this does not silently rot again.
- The 7 build warnings in `SyncServer` are unreviewed; not blocking, worth a look
  when the server becomes a first-class target in Phase 1.
