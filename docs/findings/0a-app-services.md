# Finding: the App Services guard was mis-scoped, and object-store's sync layer is built around App

Date: 2026-08-29
Task: Phase 0a Task 10 (remove Atlas App Services)

## Summary

Removing App Services was planned as a mechanical deletion behind an existing
CMake seam. It was not. Three headers named and build-guarded as App Services
turned out to contain **core sync types**, and the object-store sync layer is
architecturally coupled to `App` in ways the guard did not express.

The deletion still succeeded, but it required extraction and a small refactor
rather than `git rm` alone.

## The mis-scoped guard

| Header | Guarded as App Services | What it actually contained |
|---|---|---|
| `sync/app_config.hpp` | yes | `SyncClientTimeouts` (line 29) and `SyncClientConfig` (line 42) — **core sync configuration**, used by `SyncManager` and `SyncSession`. Only `AppConfig` (line 74) was App Services. Two thirds of the file was core. |
| `sync/impl/app_metadata.hpp` | yes | `MetadataStore`, `UserData` — genuinely App Services. Two *core* files included it, but neither used a symbol from it: `SyncFileAction`, the type they needed, is declared in `sync_user.hpp`. |
| `sync/jwt.*` | listed as App Services in the Phase 0 spec | Core. Used by `sync_user.hpp` and present in the **unconditional** CMake source list. Never guarded at all. |

`generic_network_transport.*` was likewise wrongly on the spec's deletion list:
`sync_session.hpp` and the Emscripten transport depend on it, and it holds
`AppError`, which core sync error handling uses.

**Corrected scope:** the guarded App Services set is **6,173 LOC across 22
files**, not the 6,410 estimated in the spec.

## Why `REALM_APP_SERVICES=OFF` building cleanly was misleading evidence

The plan's first step was to prove the seam by configuring with
`-DREALM_APP_SERVICES=OFF`. That build succeeded with zero errors, which was
taken as evidence that deletion would be mechanical.

It was not evidence. The flag only removes files from the CMake **source list**.
The headers remain on disk, so every `#include` of them still resolves and every
type stays complete. Nothing breaks until the files actually leave the
filesystem.

**Rule: a build flag that excludes sources proves far less about separability
than deleting the files does.** Where the two disagree, deletion is the truth.

## The deeper coupling in object-store/sync

Once the files were genuinely gone, real dependencies surfaced:

| Site | Coupling | Resolution |
|---|---|---|
| `impl/sync_file.cpp:245` | `SyncFileManager(const app::AppConfig&)` — the constructor took the App Services config type | Changed to `SyncFileManager(const std::string& base_file_path, const std::string& app_id)`. Only those two fields were ever read. |
| `sync_session.{cpp,hpp}` | 5 uses of `app::AppError` | Include repointed to `generic_network_transport.hpp`, where `AppError` is defined. No code change. |
| `sync_manager.cpp` | included `app.hpp` | Dead include, zero symbol uses. Removed. |
| `audit.mm`, `audit.hpp`, `audit_serializer.hpp` (1,687 LOC) | Apple-only audit feature that reports events through `App` and MongoDB collections | Deleted, with `test/object-store/audit.cpp`. |

`SyncFileManager` is the clearest example of the general problem: a core
file-management class whose only tie to App Services was accepting a config
struct it barely read. The coupling was in the *signature*, not the logic.

## What was deleted

| Component | LOC |
|---|---|
| App Services sources (22 guarded files) | 6,173 |
| App-services / FLX object-store tests (11 files) | 16,534 |
| BAAS admin-API test infrastructure (`baas_admin_api.*`, `redirect_server.hpp`) | 2,361 |
| Audit feature (`audit.mm/.hpp`, `audit_serializer.hpp`, `audit.cpp` test) | ~1,687 + test |
| `#if REALM_APP_SERVICES` dead branches (7 files) | 551 |
| `#if REALM_ENABLE_AUTH_TESTS` dead branches (8 files) | 1,635 |

## Retained pending investigation I1

`test/object-store/sync/client_reset.cpp` (7,966 LOC) sat inside the App
Services guard, but where client reset falls on the full-sync/FLX line is exactly
what investigation I1 exists to answer. Deleting 8k lines of coverage for a
subtle convergence feature on a snap judgement would be the kind of guess this
plan is structured to prevent.

Removing the CMake block already drops it from the build, so the file sits on
disk, inert, until I1 reports. If I1 finds it is FLX-only, it goes in Task 12; if
it is protocol-level, it becomes a candidate for retargeting at the bundled
server in Phase 1.

## Investigation I3, resolved here

I3 asked which sync tests only pass against a live BAAS and must therefore be
deleted rather than retargeted. Task 10 answered it:

- The entire `REALM_ENABLE_AUTH_TESTS` path required `REALM_MONGODB_ENDPOINT`,
  a live Atlas instance. **Deleted** — there is nothing to retarget it at.
- The object-store App Services and FLX tests were likewise BAAS-bound. **Deleted.**
- The **core** sync tests are unaffected: `test_sync.cpp` and friends run against
  the *bundled* in-process server, not BAAS, and all 463 pass
  (`0a-thesis-validation.md`).

The split is clean because the two test suites targeted different servers. The
object-store suite tested against Atlas; the core suite tested against the server
in this repository. Only the former died with the service.

## Consequence for Phase 1

`object-store/sync/` is the SDK-facing sync layer, and it was designed around
Atlas App Services: authentication, user metadata, app configuration, and routing
all assumed it. Phase 0a removed that layer, which leaves `SyncManager` and
`SyncSession` intact but without the identity and configuration model they were
built to consume.

Phase 1 must therefore supply a Tessera-native equivalent — user identity,
credentials, and server routing for the bundled server — rather than simply
pointing the existing code at a new URL. That work is now visible instead of
latent, which is the point of doing the deletion early.
