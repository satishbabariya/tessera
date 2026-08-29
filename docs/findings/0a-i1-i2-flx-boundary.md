# Finding: the FLX / full-sync boundary (investigations I1, I2, I3)

Date: 2026-08-29
Task: Phase 0a Task 11
Purpose: decide exactly what Task 12 may delete. Every verdict below cites a
`file:line`; a verdict without evidence is a guess, and Task 12 deletes code on
the strength of this table.

## Summary of verdicts

| Investigation | Question | Verdict |
|---|---|---|
| **I1** | Where does `client_reset*` sit on the full-sync/FLX line? | **Core protocol feature — KEEP.** Excise 6 lines of FLX coupling. |
| **I2** | Does FLX leak into core replication? | **No. Completely FLX-free.** Deletion cannot touch the engine. |
| **I3** | Which tests only pass against a live BAAS? | **Resolved in Task 10.** Atlas-bound tests deleted; bundled-server tests unaffected. |

---

## I1 — `client_reset*` is a client-side protocol feature, not FLX

**Verdict: KEEP** `src/realm/sync/noinst/client_reset*` (2,048 LOC).

### Evidence for "core"

The core sync client depends on it directly:

| Site | Evidence |
|---|---|
| `sync/client.cpp:6` | `#include <realm/sync/noinst/client_reset.hpp>` |
| `sync/client.cpp:106` | `void handle_pending_client_reset_acknowledgement();` |
| `sync/client.cpp:143` | `std::optional<ClientReset> m_client_reset_config;` |
| `sync/client.cpp:698` | `SessionImpl::get_client_reset_config()` |

Client reset is how a client recovers when its history diverges irrecoverably
from the server's. That is a property of the sync protocol, not of flexible sync.

### Evidence against "FLX-specific"

The bundled server does not reference `client_reset` at all
(`grep -rli client_reset src/realm/sync/noinst/server/` returns nothing), so
client reset is driven entirely from the client side.

Total FLX coupling across all six files is **six lines**:

| File | Line | Coupling |
|---|---|---|
| `client_reset.cpp` | 31 | `#include <realm/sync/noinst/pending_reset_store.hpp>` |
| `client_reset.cpp` | 32 | `#include <realm/sync/subscriptions.hpp>` |
| `client_reset.cpp` | 483 | `sync::SubscriptionStore* sub_store` parameter |
| `client_reset.cpp` | 539-544 | `if (sub_store) { … mark_active_as_complete / set_active_as_latest … }` |
| `client_reset.hpp` | 73 | the same `sub_store` parameter in the declaration |

Per-file FLX reference counts: `client_reset.cpp` 6, `client_reset.hpp` 2,
`client_reset_operation.cpp` 2, `client_reset_operation.hpp` 2,
`client_reset_recovery.cpp` 2, `client_reset_recovery.hpp` 0.

### Action for Task 12

Remove the `SubscriptionStore*` parameter and the guarded block that uses it,
plus the two includes. The `if (sub_store)` guard means the FLX path was always
optional — non-FLX callers already passed `nullptr`, so removing it changes no
behaviour for full sync.

### The object-store test is a separate question

`test/object-store/sync/client_reset.cpp` (7,966 LOC) sat inside the
`REALM_APP_SERVICES` guard and depends on the deleted App/FLX test harness. It
cannot build and is already out of the build as of Task 10.

Deleting it is safe **because the coverage that matters survives**: the core
`test/test_client_reset.cpp` provides 27 test cases that run against the bundled
in-process server, and all of them pass (`0a-thesis-validation.md`). The
object-store variant tested client reset through Atlas; the core variant tests it
through the server in this repository.

---

## I2 — core replication is entirely FLX-free

**Verdict: no leakage. Task 12 cannot destabilise the engine.**

Searched for `subscription|flx|FLX|pending_bootstrap|migration_store|schema_migration|pending_reset`:

| File | LOC | FLX references |
|---|---|---|
| `sync/history.cpp` | 12 | **0** |
| `sync/history.hpp` | 74 | **0** |
| `impl/transact_log.cpp` | 64 | **0** |
| `replication.cpp` | 474 | **0** |
| `replication.hpp` | 514 | **0** |

Zero across 1,138 LOC. This was the investigation with the most potential to
expand Task 12's blast radius, and it came back clean: the boundary between core
replication and the sync layer holds. FLX lives strictly above it.

---

## I3 — BAAS-dependent tests, resolved during Task 10

**Verdict: deleted, not retargeted. Nothing was lost that could have been saved.**

The two test suites targeted *different servers*, which is why the split is clean:

| Suite | Target | Outcome |
|---|---|---|
| `test/object-store/**` App Services, FLX, auth tests | Atlas (`REALM_MONGODB_ENDPOINT`) | **Deleted.** The service no longer exists; there is nothing to point them at. |
| `test/test_sync.cpp`, `test_client_reset.cpp`, `test_sync_history_migration.cpp` | the **bundled** in-process server | **Retained.** All 463 pass. |

The whole `REALM_ENABLE_AUTH_TESTS` path was removed with its option, its CMake
plumbing, and the `baas_admin_api.*` / `redirect_server.hpp` infrastructure
(2,361 LOC). See `0a-app-services.md`.

---

## Consequence: Task 12 is bounded

With all three answered, the FLX deletion is narrower than the spec feared:

- **Delete:** `subscriptions.*`, `migration_store.*`, `sync_schema_migration.*`,
  `pending_bootstrap_store.*`, `pending_reset_store.*` (2,973 LOC), plus
  `test/object-store/sync/client_reset.cpp` (7,966 LOC, already unbuilt).
- **Excise, do not delete:** six lines of `SubscriptionStore` coupling in
  `client_reset.{cpp,hpp}`.
- **Do not touch:** core replication (I2), and `client_reset*` itself (I1).

The one caveat carried forward: `test/object-store/util/test_file.hpp` references
`SyncConfig::FLXSyncEnabled`, so FLX has reached into `SyncConfig`. Task 12 should
expect to touch shared config types, not only leaf files.
