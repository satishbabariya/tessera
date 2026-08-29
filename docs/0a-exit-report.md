# Phase 0a Exit Report

Branch: `phase-0a-foundation`
Baseline: `f8752e180` (realm-core v14.14.0)
Date: 2026-08-29
Commits: 13 · **221 files changed, 2,119 insertions, 76,381 deletions**

## Verdict

**Phase 0a's purpose was to answer one question and remove one class of risk.**
Both are done. The bundled sync server works (criterion 4, GREEN), and the build
no longer depends on any host the project does not control (criterion 1).

Phase 0b is clear to proceed.

## Criteria

| # | Criterion | Status | Evidence |
|---|---|---|---|
| 1 | No dependency on uncontrolled hosts | **PASS** | `tools/check-no-vendor-hosts.sh` passes and is canary-tested. `static.realm.io` severed at four levels; `evergreen/` deleted; sha-1/sha-2 vendored |
| 2 | CI green on tier-1 platforms | **BLOCKED** | Prerequisite P1 unmet — no Tessera remote exists, so GitHub Actions cannot run. Workflows written but unexecuted |
| 3 | CoreTests + ObjectStoreTests pass | **PASS, with a correction** | Debug 1652, Release 1647, ObjectStoreTests 344 cases / 70,159 assertions. **See the Task 14 correction below.** |
| 4 | **147+ bundled-server sync tests pass** | **PASS — GREEN** | **All 463 sync tests pass, 121,505 checks.** `docs/findings/0a-thesis-validation.md` |
| 5 | Deletion manifest executed | **PASS, re-scoped** | 76,381 deletions. FLX deferred with evidence; see below |
| 9 | No benchmark regression >5% | **NOT MEASURABLE** | See "Criterion 9" below — the criterion is defective, not the code |

Criteria 6, 7, 8 and 10 (rename, library carve, header tiers, `v0.1.0` tag) are
Phase 0b scope and were never in Phase 0a.

## Deletion accounting

First-party LOC, excluding vendored third-party code:

| Area | Upstream | Now | Δ |
|---|---|---|---|
| `src/realm` | 229,018 | 211,756 | **−17,262** |
| `test/` (excl. Catch2) | 195,479 | 156,410 | **−39,069** |
| `bindgen/` | 3,103 | 0 | **−3,103** |
| **Total first-party** | **427,600** | **368,166** | **−59,434** |

By subsystem:

| Task | Removed | LOC |
|---|---|---|
| 2 | `evergreen/`, vendor fetch modules | 3,255 |
| 7 | Jenkins, 9 workflows, packaging manifests, Dockerfiles, `.devcontainer` | — |
| 8 | `bindgen/` + `package.json` | 3,103 |
| 9 | `object-store/c_api`, `src/realm.h`, c_api tests, modulemap | 20,116 |
| 10 | App Services, its tests, BAAS harness, audit backend, dead branches | ~28,900 |
| 12 | orphaned object-store `client_reset.cpp` | 7,966 |
| 13 | importer, realmd, realm_browser | 1,346 |
| 14 | `test_upgrade_database.cpp` + 11 fixtures (~1.4MB) | 887 |

**Caution on LOC comparisons.** The upstream `test/` figure of 195,479 was
measured while the Catch2 submodule was uninitialised. Catch2 is 69,578 LOC, so a
naive before/after comparison shows `test/` *growing*. Always exclude
`test/external/` and `src/external/`.

## Criterion 9 is defective and should be rewritten

The spec asks for "no regression greater than 5% against day-one numbers." That
cannot be evaluated on this hardware:

**Two runs of the identical binary, back to back, on a quiet machine:**

- **29 of 125 benchmarks differ by more than 5%**
- median run-to-run variance 2.6%, mean 4.5%, **max 79.6%**

The noise floor exceeds the threshold the criterion sets. Of 14 benchmarks that
looked >5% slower than baseline in the first run, only 5 reproduced in the
second.

**Unresolved signal worth carrying forward.** Three benchmarks in the same
family — `QueryChainedOrInts`, `QueryChainedOrIntsCount`,
`QueryChainedOrIntsIndexed` — reproduced at **+11% to +13% in both runs**. A
same-family cluster is more suggestive than scattered noise. No mechanism is
apparent: nothing deleted in Phase 0a touches query evaluation. This is recorded
as **unresolved, not dismissed**, and should be measured properly (many
repetitions, pinned CPU, quiet machine) once CI exists.

**Recommended rewrite for the spec:** compare medians over >=5 repetitions on a
dedicated runner, and set the regression threshold at 15% for this suite, or
adopt a benchmark harness that reports confidence intervals.

## Correction: Task 14's verification was incomplete

Recorded 2026-08-29, during Phase 0b.

Task 14 narrowed `accepted_versions_` to `{24}`, so files in formats v10-v23 are
rejected rather than upgraded. It was verified with **CoreTests only** (1652
passed, correct) and committed.

Three further tests read old-format fixtures, and all three live in the suites
that were *not* run:

| Test | Fixture | Suite |
|---|---|---|
| `test/object-store/backup.cpp` (2 sections) | v20 | ObjectStoreTests |
| `Sync_HistoryMigration` | v22 | SyncTests |
| `Sync_SubscriptionStoreInternalSchemaMigration` | v22 | SyncTests |

They failed with `UnsupportedFileFormatVersion` -- which is the *correct*
behaviour for the change; the tests simply exercised machinery the change had
deliberately removed. They were deleted during Phase 0b, as Task 14 should have
done.

**The lesson is about scope of verification, not about the change.** Task 14
altered how every database file is opened, and CoreTests was treated as
sufficient. Choosing which suites to run is a judgement about blast radius, and
for anything reached through `DB::open` the answer is all of them. The Phase 0b
plan's constraints have been amended accordingly.

The failure surfaced only because the Phase 0b rename forced a full three-suite
run. Without it, a broken tree would have sat in history under a commit message
claiming verification -- which is the more serious problem, and the reason this
correction is recorded here rather than quietly fixed.

## What was re-scoped, and why

Three scope judgements were made, in different directions. Each is documented
with evidence.

| Task | Decision | Reason |
|---|---|---|
| 10 App Services | **Executed in full** | A genuine vendor dependency. Required extraction, not just deletion: the guard was mis-scoped and `app_config.hpp` was two-thirds core sync |
| 12 FLX | **Deferred to Phase 1** | ~394 references woven through the sync session lifecycle, not leaf files. `test_client_reset.cpp` alone holds 76, and it belongs to the suite that proved the server works. `docs/findings/0a-flx-deferred.md` |
| 14 Format upgrades | **Goal met, code deferred** | Narrowing `accepted_versions_` to `{24}` achieves the clean break in one line. Removing `DB::upgrade_file_format` is surgery on `DB::open`'s retry loop and belongs with the Phase 0b format change |

The principle applied throughout: separate *what the phase needs to be true* from
*what the plan said to do*. Phase 0a needed no vendor dependencies and no
back-compat obligations. It did not need a maximally tidy tree — that is Phase
0b's job, when the rename makes the same edits unavoidable anyway.

## Open items

**Blocking Phase 0a completion:**

- **P1 — a Tessera git remote.** `origin` still points at
  `https://github.com/realm/realm-core`. Until a repository exists under the
  project owner's control, nothing can be pushed and criterion 2 cannot be met.
  Creating it is an outward-facing act reserved for the owner.

**Carried into Phase 0b:**

- ~~Bison 3.8.2~~ **Resolved 2026-08-29.** Bison 2.3 cannot process the grammar
  at any version constraint (it uses 3.x-only `api.token.constructor`,
  `api.value.type variant`, `api.symbol.prefix`). But regeneration is not needed
  for the rename: the generated files contain only six `realm` references, all
  includes and namespace qualifiers. Rename the grammar sources and the generated
  output together -- which preserves the no-drift rationale of the
  never-hand-edit rule -- and gate on the 73 tests in `test_parser.cpp`.
  Bison 3.8.2 is now a nice-to-have, not a prerequisite.
  `docs/findings/0a-toolchain-rot.md`
- Remove the now-unreachable file-format upgrade machinery alongside the format
  identity change.
- `doc/protocol.md` (1,126 lines) and `doc/algebra_of_changesets.md` should be
  linked prominently from the new README. `docs/findings/0a-existing-documentation.md`

**Carried into Phase 1:**

- FLX removal, during the sync client rework. I1/I2 facts recorded so they need
  not be rediscovered: core replication is entirely FLX-free; `client_reset`'s
  whole FLX coupling is six lines.
- `object-store/sync/` lost the identity and configuration model it was built
  around. Phase 1 must supply a Tessera-native equivalent, not merely repoint a
  URL. `docs/findings/0a-app-services.md`
- Verify `doc/protocol.md` still matches `sync/protocol.cpp` before publishing it.

**Test-suite health, needed before CI is trustworthy:**

- `reports DNS error` is network-flaky: four runs of one binary gave 4.2s,
  680.6s (failing), 13.7s, 0.011s. Must not gate merges.
- The suite leaks temp directories (1,471 in one session). With `TMPDIR` at 48k
  entries, one test went from 0.006s to 243.8s.
  `docs/findings/0a-flaky-and-slow-tests.md`

## Go / no-go for Phase 0b

**GO.** Criterion 4 is the one that mattered, and it passed on first attempt: the
bundled sync server compiled with zero errors and its full suite passes. Phase 1
is a revival of a working open sync stack rather than a greenfield build, which
is the assumption the entire Tessera strategy rests on.

The one caveat is that all evidence is macOS/arm64, Debug and Release, on a
single machine. Linux and Windows are unmeasured, and the transport layer is the
most platform-sensitive code in the tree. That gap closes when P1 is satisfied
and CI runs.
