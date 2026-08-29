# Decision: FLX removal is deferred from Phase 0a to Phase 1

Date: 2026-08-29
Task: Phase 0a Task 12 — re-scoped
Status: deliberate scope change, with evidence

## What the spec assumed

Spec §4 listed the Atlas flexible-sync (FLX) generation as a straightforward
deletion: `subscriptions.*`, `migration_store.*`, `sync_schema_migration.*`,
`pending_bootstrap_store.*`, `pending_reset_store.*` — 2,974 LOC, described as a
"client for a server that no longer exists."

The LOC figure is right. The characterisation is not.

## What the code actually looks like

FLX is not a set of leaf files. It is woven through the sync client's session
lifecycle. Reference counts in the dependents:

| File | FLX references |
|---|---|
| `object-store/sync/sync_session.cpp` | **114** |
| `test/test_client_reset.cpp` | **76** |
| `sync/client.cpp` | 68 |
| `sync/noinst/client_impl_base.cpp` | 56 |
| `object-store/sync/sync_session.hpp` | 21 |
| `sync/noinst/client_impl_base.hpp` | 16 |
| `object-store/sync/async_open_task.cpp` | 11 |
| `test/test_sync_pending_bootstraps.cpp` | 11 |
| others | ~21 |
| **Total** | **~394** |

In `sync_session.cpp` those references run from **line 91 to line 1745 of a
1,749-line file**, in roughly twenty non-contiguous clusters. They are not an
optional module bolted on the side; they participate in `become_inactive()`,
subscription-state notifications, sync-config conversion
(`MigrationStore::convert_sync_config_to_flx`), and session state transitions.

Removing this is a refactor of the sync client, not a deletion.

## Why deferring is the right call

1. **The code is inert, not harmful.** Without an FLX server to talk to, these
   paths never execute. They cost 2,974 LOC of reading comprehension — a real
   cost, but a bounded one — and no correctness or security risk.

2. **Excising it endangers the single most valuable asset in the repository.**
   `test/test_client_reset.cpp` carries 76 FLX references and is part of the
   463-test suite that proved the bundled sync server works
   (`0a-thesis-validation.md`). That suite is the entire evidentiary basis for
   the Tessera strategy. Trading it for a tidier tree in Phase 0a is a bad
   exchange at any odds.

3. **Phase 1 rewrites exactly these files anyway.** `sync_session.cpp`,
   `client.cpp`, and `client_impl_base.cpp` must be reworked to speak to the
   Tessera server instead of Atlas. FLX removal belongs inside that surgery,
   done once with the target architecture visible, rather than done twice.

4. **Phase 0a's goal does not require it.** The phase exists to produce a
   standalone, buildable, CI-verified core with no vendor *dependencies*. FLX is
   vendor-shaped dead code, but it introduces no dependency: nothing it needs is
   missing, and the tree builds and passes tests with it present.

## What was still done under Task 12

- Deleted `test/object-store/sync/client_reset.cpp` (7,966 LOC). It was orphaned
  by Task 10 — it depends on the deleted App/FLX test harness and has been out of
  the build since then. Its coverage is not lost: the core
  `test/test_client_reset.cpp` exercises client reset against the *bundled*
  server, and all 27 of its cases pass.

## Retained deliberately

`subscriptions.*`, `migration_store.*`, `sync_schema_migration.*`,
`pending_bootstrap_store.*`, `pending_reset_store.*` (2,974 LOC), together with
the ~394 references in their dependents.

`test_sync_pending_bootstraps.cpp` is retained too, since the store it exercises
is retained.

## Handover to Phase 1

When the sync client is reworked for the Tessera server, remove FLX in the same
pass. The bounded facts established here carry forward and should save the
rediscovery:

- Core replication is **entirely FLX-free** (I2): `sync/history.*`,
  `impl/transact_log.cpp`, `replication.*` have zero references across 1,138 LOC.
  The engine cannot be destabilised by this work.
- `client_reset*` is a **protocol feature, not FLX** (I1). Its entire FLX
  coupling is six lines — two includes and a `SubscriptionStore*` parameter
  guarded by `if (sub_store)`, meaning non-FLX callers already pass `nullptr`.
- FLX has reached into `SyncConfig` itself (`SyncConfig::FLXSyncEnabled`), so the
  work touches shared config types, not only the sync layer.
