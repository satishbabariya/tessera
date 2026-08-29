# Tessera is a fork of Realm Core

## What this is

Tessera is an embedded, local-first database. It is a fork of
[realm-core](https://github.com/realm/realm-core) at commit `f8752e180`, released
as v14.14.0.

Realm Core is the storage engine that powered the Realm mobile databases. MongoDB
acquired Realm in 2019 and has since wound down Atlas Device Sync and the Realm
SDKs. The engine itself is excellent and was left in good condition; it simply has
no living upstream.

Tessera continues it as an independent project with a different focus: a
local-first database with a working open sync stack, rather than a client for a
proprietary cloud service.

## Why the rename was required, not chosen

The Apache License 2.0 grants copyright and patent rights. It explicitly does
**not** grant trademark rights (section 6). "Realm" is a MongoDB trademark, so a
fork cannot keep the name regardless of preference.

That obligation is met throughout: the project, namespace, macro prefix, library
names, file extension and CMake package are all renamed.

## What is retained, and why

**Every copyright notice.** 560 files carry `Copyright ... Realm Inc.` headers,
and they are untouched. Apache 2.0 section 4(b) requires retaining all copyright,
patent, trademark and attribution notices in derivative works. The rename script
(`tools/rename.sh`) skips any line matching `Copyright` or `Realm Inc`, and the
count is asserted identical before and after.

`LICENSE` and `THIRD-PARTY-NOTICES` are likewise unmodified.

If you contribute to Tessera, do not "tidy up" those headers. They are a licence
obligation, not stale text.

## What changed

### The file format is new and incompatible

Tessera files use the magic mnemonic `TESS` and file format version 1. Realm files
use `T-DB` -- a fossil of TightDB, Realm's original name -- and versions 10 to 24.

**Tessera cannot open a `.realm` file.** It rejects any file whose format is not
its own, rather than attempting an upgrade. This is deliberate: supporting a
decade of migration paths for a format we no longer produce would be a permanent
tax for a shrinking audience.

If you need to migrate data, do it while you still have a working Realm build:
export with Realm, import with Tessera.

### Atlas App Services and flexible sync are gone

The layers that talked to MongoDB's proprietary cloud are removed: App, MongoDB
remote access, push notifications, JWT auth against Atlas endpoints, the audit
backend, and the BAAS test infrastructure. The services they targeted no longer
exist.

Flexible sync (FLX) remains present but unused, pending removal in Phase 1. See
`docs/findings/0a-flx-deferred.md` for why it was not removed here.

### The bundled sync server is now a first-class component

realm-core contained a complete, self-hostable sync server at
`sync/noinst/server/`, marked `EXCLUDE_FROM_ALL` so it never built by default.
It works: 461 sync tests pass against it, including randomised concurrent-edit
convergence.

Tessera builds it as a supported target. This is the single most valuable thing
inherited from the fork, and the reason the project is viable.

### The merge engine is a standalone library

The operational-transform engine -- changesets, instructions, convergence -- is
carved out as `tessera-merge`, depending only on the storage engine. Previously it
was compiled into a monolithic sync library alongside the wire protocol and
transport, so nothing could use it without a websocket stack.

### The build depends on nothing we do not control

realm-core downloaded prebuilt OpenSSL and cross-compile toolchains from
`static.realm.io`, and its installed CMake package made downstream consumers do
the same. All of that is gone; dependencies come from the environment.
Enforced by `tools/check-no-vendor-hosts.sh`.

## What has not changed

The storage engine. The copy-on-write, memory-mapped MVCC design, the cluster
tree, the query engine, the indexes and the encryption layer are Realm's work,
lightly modernised. Roughly 156,000 lines of tests come with it and continue to
pass.

Tessera's contribution so far is removal, repair and clarification -- not
reinvention.

## Common questions

**Is this Realm?** No. It is a fork of Realm's storage engine under a different
name, with no relationship to MongoDB and no support from them.

**Can I open my `.realm` files?** No. See the file format section above.

**Can I use the Realm SDKs with this?** No. The C API and binding generator those
SDKs depended on were removed; a stable C ABI is planned for a later phase.

**Why not contribute upstream instead?** There is no upstream to contribute to.
The last commits were build fixes, the release notes template is empty, and the
products it served are discontinued.

## Fetching upstream history

This repository has **no `upstream` remote, deliberately**. `gh` resolves its
target repository from the git remotes, and with one configured
`gh release create` targeted `realm/realm-core` rather than this repository. It
failed only because the account lacked permission to write there.

If you need upstream history, add the remote for the duration of the fetch and
remove it again:

```sh
git remote add upstream https://github.com/realm/realm-core
git fetch upstream
git remote remove upstream
```

`tools/check-no-vendor-hosts.sh` fails if such a remote is left configured.

Note that realm-core's tags (`v0.1.0` through `v14.14.0`) are not carried into
this fork. They are another project's release history, they collide with
Tessera's version scheme, and version tooling would otherwise read `v14.14.0` as
the latest release of a project at 0.1.0. They remain in the upstream repository.

## Lineage

- Upstream: https://github.com/realm/realm-core
- Fork point: `f8752e180` (v14.14.0)
- Licence: Apache 2.0, unchanged
- Original copyright: Realm Inc. and contributors, retained in full
