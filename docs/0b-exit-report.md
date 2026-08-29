# Phase 0b Exit Report

Branch: `phase-0b-identity`
Predecessor: `phase-0a-foundation` (see [0a-exit-report.md](0a-exit-report.md))
Date: 2026-08-29

## Verdict

Phase 0b's job was to make the fork *its own project*: a name, a file format, a
public API, and documentation honest enough to hand someone.

Done, with one external blocker. `v0.1.0` cannot be tagged until a Tessera git
remote exists (prerequisite P1), which is the project owner's call to make.

## Criteria

| # | Criterion | Status | Evidence |
|---|---|---|---|
| 6 | Rename complete | **PASS** | ~12,000 occurrences across 721 files; 560 copyright notices intact, asserted before and after |
| 7 | `tessera-merge` carved out | **PASS** | 19 files, 11,084 LOC, links `Storage` only, enforced by `check-merge-deps.sh` |
| 8 | Header tiers | **PASS** | `<tessera/api.hpp>` and `<tessera/engine.hpp>`, enforced by `check-header-tiers.sh` |
| — | File format identity | **PASS** | `TESS` magic, version 1, verified on disk in a file written by an external consumer |
| — | Documentation | **PASS** | README, ARCHITECTURE.md, FORK.md, CONTRIBUTING.md; README example compiled and run in CI |
| 10 | `v0.1.0` tagged | **BLOCKED** | Requires P1 |

## What shipped

| | |
|---|---|
| Commits in Phase 0b | 8 |
| Cumulative vs upstream | 997 files, 89,806 deletions, 16,494 insertions |
| First-party source | 211,481 LOC |
| First-party tests | 155,912 LOC |
| Test results | CoreTests 1652, SyncTests 461, ObjectStoreTests 343 |

## The recurring finding

Six times in this project, an inherited label did not describe the code beneath
it:

| Label | Reality |
|---|---|
| `REALM_APP_SERVICES` build guard | contained core sync types (`SyncClientConfig`) |
| `audit.hpp` | cross-platform interface, one platform-specific implementation |
| `src/external/s2` | a downstream fork calling into this project |
| `DB::upgrade_file_format` | also upgraded history schemas, a live path |
| `impl/` | not private; six headers required by core engine headers |
| `sync/protocol.hpp` | held three plain integer typedefs the merge engine needed |

Every one was found by reading the body or by a failing gate. None by trusting
the name. In a decade-old codebase, inherited names describe history rather than
structure, and the cost of assuming otherwise ranged from a compile error to
deleting a live upgrade path.

## Verification gaps found, and closed

The gate was too narrow twice, and each time the fix was discovered by damage
rather than by design:

1. **Phase 0a Task 14** ran CoreTests only. Three tests in the other two suites
   read v20/v22 fixtures and were left broken until the rename forced a full run.
   Gate widened to all three suites.
2. **Phase 0b Task 2** ran all three suites but never built `--target all`. The
   command-line tools are not compiled by the test suites, so `tessera2json`
   silently kept a reference to a deleted `DBOptions` field. Gate widened to
   `cmake --build` with no target argument.
3. **`check-no-vendor-hosts.sh`** searched for download URLs, so it passed while
   `.github/workflows/check-pr-title.yml` used `realm/ci-actions/title-checker@main`
   -- a MongoDB-owned action, unpinned, running on every pull request. Check
   widened to cover CI actions from vendor-controlled organisations.

The pattern: **a check verifies what it was written to look for, and silently
approves everything else.** Each of these passed for weeks while the thing it
claimed to guarantee was false.

## Enforced invariants

Six executable checks, each canary-tested by introducing a deliberate violation
and confirming rejection:

| Check | Invariant |
|---|---|
| `check-copyright-notices.sh` | Apache 2.0 §4(b) attribution survives every tree-wide edit |
| `check-no-vendor-hosts.sh` | no build or CI dependency on hosts we do not control |
| `check-layering.sh` | no upward includes between layers |
| `check-merge-deps.sh` | `tessera-merge` depends only on storage |
| `check-header-tiers.sh` | the public API does not leak private headers |
| `verify/consumer-smoke-test.sh` | the installed package is consumable, and the README example works |

The last one is the only gate that tests what a user experiences rather than what
the tree contains. It found three defects on first run, including a CMake package
installed to `share/cmake/Realm/` where `find_package(Tessera)` would never look.

## Deliberately not done

| | Why |
|---|---|
| Renaming the `Realm` class (~2,600 sites) | It is the tier-1 API's principal type, and `class DB` already occupies the engine tier. An API naming decision, not a substitution |
| Removing FLX (~394 references) | Woven through the sync session lifecycle; `test_client_reset.cpp` alone holds 76. Phase 1 reworks these files anyway |
| Curating the install set (236 → 147 headers) | Analysis done; shipping unreachable headers misrepresents the API but harms nothing |
| Removing the six `impl/` exposures | Requires refactoring `group.hpp`, `db.hpp`, `replication.hpp` |

## Open items

**Blocking:**

- **P1 — a Tessera git remote.** `origin` still points at MongoDB's repository.
  25 commits are local, backed by a verified `git bundle`. Creating a repository
  publishes the fork, which is the owner's decision.

**Carried to Phase 1:**

- FLX removal, during the sync client rework. I1/I2 facts are recorded so they
  need not be rediscovered.
- `object-store/sync/` lost the identity and configuration model it was built
  around. Phase 1 must supply a Tessera-native equivalent, not repoint a URL.
- Verify `doc/protocol.md` still matches `sync/protocol.cpp` before publishing it
  as Tessera's protocol.

**Known limitations, documented rather than hidden:**

- Verified on macOS/arm64 only. Linux and Windows are unmeasured; the transport
  layer is the most platform-sensitive code in the tree.
- One write transaction at a time, across all threads and processes.
- `reports DNS error` is network-flaky and must not gate merges.
- The test suite leaks temporary directories; a large `TMPDIR` degrades some
  tests by four orders of magnitude.
