# Finding: the merge engine's dependency on the wire protocol was three typedefs

Date: 2026-08-29
Task: Phase 0b Task 3 (carve out tessera-merge)

## What was done

19 files and 11,084 LOC moved from `sync/` and `sync/noinst/` into
`src/tessera/merge/`, with their own library target `tessera-merge`
(`Tessera::Merge`) linking `Storage` and nothing else.

`changeset_index` and `integer_codec` were promoted out of `noinst/`: they are
part of the merge engine's implementation, not sync-private utilities.

## Why this matters

The merge engine is a production-tested operational-transform implementation
with roughly 6,000 LOC of dedicated tests, including randomised concurrent-edit
convergence (`Transform_Randomized`). It is the component every local-first
competitor is currently building from scratch.

Before this change it was compiled into a monolithic `Sync` library that also
contained the wire protocol, the transport, and the session machinery. Nothing
could depend on the merge engine without also pulling in a websocket stack, so
its standalone value was invisible.

## The coupling was an accident of declaration site

`transform.hpp` included `sync/protocol.hpp` and `transform.cpp` included
`sync/noinst/protocol_codec.hpp`. Both looked like genuine protocol dependencies.
Everything they actually supplied was:

| Symbol | What it really is |
|---|---|
| `version_type` | alias for `Replication::version_type` -- a **storage engine** type |
| `file_ident_type` | plain `std::uint_fast64_t` |
| `timestamp_type` | plain `std::uint_fast64_t` |
| `parse_changeset` | the **merge engine's own** function, in `changeset_parser.hpp` |
| `util::compression`, `<iomanip>` | core utility and standard library |

Not one is protocol-specific. They merely *lived* in `protocol.hpp`, and that
accident of where three integer typedefs were declared is what made the engine
appear un-carvable for a decade.

The aliases now live in `instructions.hpp`, the merge engine's own base header.
`protocol.hpp` still declares them identically, which is well-formed.

## Correction to a Phase 0a finding

Phase 0a reported that **both** of `transform`'s protocol includes were dead --
one of the arguments that this carve would be straightforward. That was wrong.
Both were load-bearing, and removing them produced 60 compile errors.

The method was the flaw: the check grepped for `protocol::`-qualified symbols and
found none. Every symbol above is **unqualified** -- namespace-scope typedefs and
free functions that look local at the call site. A grep for a namespace prefix
cannot see them.

The conclusion survived but the reasoning is now better: the dependency was real
and removable, rather than absent. Understanding *why* it existed is what made the
fix correct rather than lucky.

## Enforcement

`tools/check-merge-deps.sh` fails the build if anything under `src/tessera/merge`
includes from `sync/` or `object-store/`. It is canary-tested -- a deliberate
violation is introduced and the check confirmed to reject it -- because a check
that cannot fail is worthless.

Without it, the carve would erode the first time someone needs a protocol type in
`transform.cpp`, and the erosion would be invisible until someone tried to consume
the merge engine on its own.

## Addendum: the carve was incomplete, and the check could not see it

Recorded 2026-08-29, found by the Android compile-only nightly job.

Everything around the carve was correct. `tessera-merge` was a separate library,
it linked `Storage` alone, `tools/check-merge-deps.sh` enforced that and was
canary-tested, and this document explained why it mattered.

And `add_subdirectory(merge)` sat inside `if(TESSERA_ENABLE_SYNC)`. **The
library carved out of the sync monolith could not be built without it.** The
claim "usable standalone" was false in the one configuration that would test it.

Nothing detected this, for a specific reason: the check was reading `#include`
directives. It answered *does merge reference sync?* The claim being made was
*can merge exist without sync?* -- a question about the build graph, not the
source. The check verified the property that was easy to inspect rather than the
property being asserted.

It surfaced only because the Android job disables sync (the NDK ships no
OpenSSL), which made `Merge` exported but never built:

```
CMake Error at CMakeLists.txt:378 (export):
  export given target "Merge" which is not built by this project.
```

That is a compile-only job on an unusual configuration doing exactly what such a
job is for: testing a combination nobody had tried. Two earlier "fixes" to that
job were each real, and each uncovered the next layer.

`check-merge-deps.sh` now also requires `add_subdirectory(merge)` to be
unconditional, and is canary-tested against that rule. Verified: with
`-DTESSERA_ENABLE_SYNC=OFF -DTESSERA_ENABLE_ENCRYPTION=OFF`, the tree configures
and `libtessera-merge.a` builds.

## Also fixed: the export set was still named `realm`

`install(EXPORT realm ...)` appeared in five CMakeLists files. A bare CMake
identifier -- not a string, not a path, not a namespace -- so no rename pattern
matched it, and nothing failed until a *new* target had to join the set.

This is a useful property of adding a component during a rename: new code written
in the target vocabulary collides with whatever the rename missed. A pure rename
has no such check; it changes what it matches and stays silent about the rest.

Consumers calling `find_package(Tessera)` would otherwise have imported from an
export set called `realm`. With `TesseraTargets.cmake` and `TesseraConfig.cmake`
(both fixed in Task 1), that completes the three `realm`-named artifacts that
ship to downstream users.
