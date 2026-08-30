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
| Commits in Phase 0b | 12 |
| Cumulative vs upstream | 997 files, 89,806 deletions, 16,494 insertions |
| First-party source | 211,481 LOC |
| First-party tests | 155,912 LOC |
| Test results (Debug) | CoreTests 1652, SyncTests 461, ObjectStoreTests 343 |
| Test results (Release) | CoreTests 1647, SyncTests 460, ObjectStoreTests 343 |

## What CI established, once it existed

Phase 0b was written before this repository had any build CI. Everything below
was verified on the day it was added, and every item was broken or unverified
until the check ran:

| Platform | Verification |
|---|---|
| macOS arm64 | build and all three suites, Debug and Release |
| Linux x86-64, gcc-13 and clang-18 | build and all three suites, Debug and Release |
| Linux, sanitizers | ASan, UBSan and TSan all clean over 1652 tests |
| Windows | builds (tier 2; vcpkg supplies zlib and OpenSSL) |
| iOS, WASM | compile |
| Android | compiles **with sync and encryption disabled** -- both need OpenSSL and the NDK ships none |
| Fuzzers | ten minutes against the storage engine, no crash |

Two defects in the released v0.1.0 were found by the first runs, and neither was
detectable on the machine that built it:

- **The installed package exported `-fuse-ld=lld`.** A build-speed flag applied
  as a plain `PUBLIC` link option was baked into the exported target, so a
  consumer inherited whichever linker existed on the machine that built Tessera.
  Every test suite passed on the configuration that exhibited it; only the
  consumer smoke test failed. Fixed in v0.1.1.
- **A fresh `git clone` could not configure.** The README omitted `--recursive`,
  and a missing Catch2 produced an error naming a third-party file rather than
  the problem. Fixed in v0.1.1.

Every fix was small -- a missing include, a vcpkg install, a build flag, an
export scope. None required redesign. The value was in running the checks, not
in the fixes.

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

## The rename's blind spots

The rename matched code identifiers and was correct for what it matched. Six
categories fell outside it, each found only by looking deliberately. Full detail
in `docs/findings/0b-rename-blind-spots.md`.

| Category | How it would have reached a user |
|---|---|
| 80 user-facing message strings | "Can't compact a read-only Realm" -- what a Tessera user reads when something fails |
| A WebSocket subprotocol identifier | `com.mongodb.realm-sync#` silently became `com.mongodb.tess-sync#`. **Every test passed**: client and server are built from one tree, so both sides of the handshake changed together. Only an external peer would have failed, and nothing simulates one |
| A macOS Keychain item name | Visible in Keychain Access |
| The root log category | `Realm.Storage` is API -- users write it to filter logs. Tests proved it by calling `set_level_threshold("Realm.Storage")` |
| Two history comments | The rename made documentation assert things that never happened |
| Bare CMake identifiers | `CPACK_PACKAGE_NAME`, target names, `install(EXPORT realm)` -- would have shipped `realm`-named artifacts from a Tessera package |

Rewriting the messages then broke **178 test assertions**, because error messages
are a contract the tests assert. And three fixtures had to be reverted rather
than de-branded: `HTTPParser_ChunkedEncoding` embeds hex chunk lengths, so
rewriting ` Realm i` (8 bytes) as ` database i` (11) while leaving the `8`
corrupted the encoding. No pattern refinement catches that; running the tests is
what makes a mass substitution safe to attempt.

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
4. **One configuration.** A stale log-category name passed every Debug run and
   aborted the entire Release suite before a test executed, because the entry sat
   behind an `#ifdef`. The release gate now requires both configurations.
5. **A stale binary.** An ObjectStoreTests run reported 343 cases passing with
   53,540 assertions against a baseline near 70,000 -- same test count, 30% fewer
   assertions, because the binary predated the changes under test. Compare
   assertion counts, not only test counts.

The pattern: **a check verifies what it was written to look for, and silently
approves everything else.** Each of these passed while the thing it claimed to
guarantee was false.

The defence is the same every time, and it is worth stating as a rule rather than
a habit: `cmake --build` with no target argument, then every suite, in both
configurations, and confirm the numbers moved the way the change predicts. A
count that did not move when it should have is as much a failure as a red
build.

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
- Verify `docs/protocol.md` still matches `sync/protocol.cpp` before publishing it
  as Tessera's protocol.

**Known limitations, documented rather than hidden:**

- Verified by CI on macOS (Apple clang, arm64) and Linux x86-64 (gcc-13 and
  clang-18), in Debug and Release. Windows, Linux on ARM, iOS, Android and WASM
  remain unverified.

  The Linux result contradicted a prediction made in this report's first draft.
  Three of Phase 0a's build failures were transitive includes that only libc++
  exposed, so libstdc++ was expected to surface its own set. It did not: both
  Linux compilers built the tree clean on the first attempt. The likely reason
  is the C++20 migration, which forced explicit includes that would otherwise
  have failed here -- a benefit claimed at the time without evidence, and this
  is the evidence.
- One write transaction at a time, across all threads and processes.
- `reports DNS error` is network-flaky and must not gate merges.
- The test suite leaks temporary directories; a large `TMPDIR` degrades some
  tests by four orders of magnitude.

---

## Addendum, later the same day

The figures above were true when this report was written and several are no
longer. They are left as written, because an exit report records the state at
exit; editing it would make it describe a moment that never existed. This says
what changed instead.

| | At exit | Now |
|---|---|---|
| CoreTests, Linux Debug | 1651 | 1664 |
| CoreTests, macOS Debug | 1652 | 1664 |
| Tests disabled by `TEST_IF` | not reported | 31 Linux, 32 macOS |
| Executable checks | 6 | 12 |
| Sanitizers | ASan, UBSan, TSan clean | unchanged, over the larger suite, and over ObjectStoreTests as well |

The first version of this table had a single row reading `CoreTests (Linux) |
1652 | 1664`. 1652 was the macOS figure; Linux was 1651. The delta was roughly
right and the label was wrong, in a table whose purpose is saying what changed.

The two platforms now agree at 1664 and disagree by one on what is switched off:
`Shared_RobustAgainstDeathDuringWrite` runs on Linux, which has robust POSIX
mutexes, and is correctly disabled on macOS, which does not. That single row of
difference is the entire visible result of the work in
[0b-header-macro-visibility.md](findings/0b-header-macro-visibility.md), and
until that work the framework did not print the number at all.

Twelve of those additional tests exist because a claim was measured for the first
time. The other, `test_util_enum.cpp`, existed already and was in no
`CMakeLists`.

### What the report did not know

Six defects were found after it was written, all in code it had already
described as verified. Each has a finding under `docs/findings/`:

| | |
|---|---|
| `RobustMutex::is_robust_on_this_platform` was `false` in every translation unit, including its own, while the implementation below it compiled full robust-mutex support | five tests gated on it; none had ever run |
| Three identity strings went out over the network still saying `realm` | including a WebSocket subprotocol and the sync `User-Agent` |
| The TLS test certificates expired in 57 days | and had lapsed silently five times before |
| The README promised a self-hostable sync server | the package exports no server target and contains no server executable |
| Twenty-one files with `<` and `>` in their names | made the repository uncloneable on Windows |
| `tools/verify/clean-clone-test.sh` and `test/test_util_enum.cpp` | correct, purposeful, and referenced by nothing |

### The correction that matters most

This report says, under Enforced Invariants, that the value was "in running the
checks, not writing them". That is right and did not go far enough. Two of the
six defects above are checks and tests that were written, were correct, and were
never wired into anything -- so they had never run at all. The distinction the
report needed is not between writing a check and running it once, but between
running it and *arranging that it keeps running*.

`tools/README.md` now states, for every script in that directory, whether
anything runs it.
