# An installed public header that could not be included, linked, or found

`tessera/object-store/audit_serializer.hpp` shipped in every release. It
declares `AuditObjectSerializer`, and:

* **Nothing includes it.** Not a header, not a source file, not a test. Its only
  appearance anywhere in the repository is its own line in a CMake list.
* **Nothing implements it.** `AuditObjectSerializer::` matches no definition in
  the tree, and `nm` finds no such symbol in `libtessera-object-store.a`. A
  consumer who used it would compile and fail to link.
* **It has no include guard.** No `#ifndef`, no `#pragma once`. Including it from
  two translation units is fine; including it twice in one is not.
* **It was reachable from no public entry point.** Twelve documented entry
  points were compiled with `-H`; it appears in none of their include trees.

So it could not be used, and the measurement says nobody was positioned to try.

## What it cost anyway

It was the only installed header that includes `<external/json/json.hpp>`, so
that vendored copy of nlohmann/json had to install too -- as
`include/external/json/json.hpp`. A consumer compiles with
`-I<prefix>/include`, which puts a directory named `external` into their include
namespace. Any project that vendors its own dependencies under `external/` --
which is to say, the convention this project itself follows -- gets a silent
collision decided by `-I` order.

A header nobody could use published a third-party library into everybody's
include path.

## The check that said this was fine

`tools/check-include-root-is-clean.sh` existed for exactly this hazard and
reported:

```
PASS: only tessera.hpp installs to the include root
```

It reads `install(FILES ...)` declarations out of `src/CMakeLists.txt`. The
`external/json` rule is in `src/tessera/sync/CMakeLists.txt`, so the check could
not see it. Its conclusion was about one file's contents and was phrased as a
fact about the installed tree.

It now checks the tree when given an install prefix, and CI passes it the one
the install-surface step already builds. Canaried both ways: a stray directory
at the include root fails, and so does a stray header.

`external/mpark/variant.hpp` stays. It is reached from five installed headers --
`table.hpp`, `query_expression.hpp`, `geospatial.hpp`, `merge/object_id.hpp`,
`merge/instructions.hpp` -- so removing it means changing the public API, which
is a different decision than this one. The include root is now `tessera.hpp`,
`tessera/`, and `external/` holding that one file.

## Audit itself

`make_audit_context` terminates with "Audit is not implemented in this build".
The only implementation was `audit.mm`, an Apple-only path reporting to Atlas
App Services, deleted with the rest of App Services. `audit.hpp` stays installed:
it is a documented extension point, and its hooks are inert while
`RealmConfig::audit_config` is unset, which is the default. A serializer for a
backend that no longer exists is not an extension point.

## The pattern, again

Four checks in this project have now reported a property they did not measure:
a header count that named no header, a release gate that ran a check with no
arguments, a hook nobody installed, and this one -- a check reading declarations
in one file and reporting on a tree assembled from many. Each said PASS.

The install tree is now built once in CI and handed to every check that wants
it. Measuring the artifact is not more work than measuring the intent; it is
just less familiar.
