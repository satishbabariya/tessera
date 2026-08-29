# Finding: the public API was 236 headers; the real surface is 147

Date: 2026-08-29
Task: Phase 0b Task 5 (header tiers)

## What the spec asked for, and why it was not done

The spec called for physically relocating headers into `engine/` and `internal/`
directories to express the API tiers. That is roughly 2,400 include rewrites
across the tree.

It was measured first. The transitive closure of everything reachable from the
tier-1 and tier-2 entry points is **147 headers**, out of 293 in the tree. The
build was installing **236**.

So the shipped "public API" included 89 headers that nothing public can reach --
among them `impl/`, which is implementation detail by name.

**A consumer cannot include what is not installed.** That is a stronger boundary
than a directory name, and it costs no include churn. Tiers are therefore
expressed as documented entry points plus an enforced check, not as directory
surgery.

## The tiers

| Entry point | Tier | Contents |
|---|---|---|
| `<tessera/api.hpp>` | 1 | Schemas, objects, results, lists, sets, dictionaries, change notifications |
| `<tessera/engine.hpp>` | 2 | `DB`, `Transaction`, `Table`, `Query`, `Obj`, keys, scalar types |
| `<tessera.hpp>` | legacy | Forwards to `<tessera/engine.hpp>`; predates the split and has existing users |

Both tiers are supported surfaces. Tier 1 is what most users want; tier 2 is for
binding authors and anyone who needs direct engine access. They compose: both can
be used in one program.

## `impl/` is not private, despite the name

Six `impl/` headers are genuinely reachable from the public API:

| Header | Required by |
|---|---|
| `impl/transact_log.hpp` | `replication.hpp` |
| `impl/output_stream.hpp`, `impl/array_writer.hpp`, `impl/cont_transact_hist.hpp` | `group.hpp` |
| `impl/changeset_input_stream.hpp` | `db.hpp` |
| `impl/destroy_guard.hpp` | `array_key.hpp` |

These are core engine headers, so the dependency is real. Eliminating it means
refactoring `group.hpp`, `db.hpp` and `replication.hpp` -- deliberately out of
scope for Phase 0b.

This is the fifth instance in this project of an inherited label not matching the
code: the `REALM_APP_SERVICES` guard contained core sync types, `audit.hpp` was a
cross-platform interface with one platform-specific implementation,
`src/external/s2` contains first-party patches, `upgrade_file_format` also
upgraded history schemas, and now `impl/` is not private.

## Why the check uses an allowlist rather than a prohibition

`tools/check-header-tiers.sh` caps `impl/` exposure at exactly those six headers
rather than forbidding `impl/` outright.

A prohibition would fail on the first run, so someone would add exceptions until
the rule meant nothing. An allowlist encodes the true state ("this is the
exposure we have") and defends the property that matters ("it must not grow").

A rule people route around is worse than no rule: it manufactures the appearance
of enforcement.

The check also forbids any public header including from `noinst/` -- "no install",
private by name and, unlike `impl/`, private in fact.

Canary-tested: adding an `impl/simulated_failure.hpp` include to `table.hpp` is
detected and rejected.

## The install set: what was culled, and what was deliberately not

**Culled:** `impl/simulated_failure.hpp`, a fault-injection facility used only
from `.cpp` files and never reachable from a public header. It was the only
test-only header being shipped. `noinst/` headers -- "no install", private by
name -- were already correctly excluded.

**Not culled:** the remaining gap between 236 installed and 147 measured as
reachable.

The 147 figure is the transitive closure from *the tier entry points this project
chose*. That is the right measure for "what does the documented API need", and
the wrong measure for "what may a consumer legitimately include". Someone with an
unusual requirement might reasonably include `<tessera/util/file.hpp>` or another
utility that no tier-1 or tier-2 header happens to reach. Culling to exactly 147
would break them for a cosmetic gain.

So the honest position, stated rather than quietly shipped: **Tessera installs
more headers than its documented API requires.** Anything not reachable from
`<tessera/api.hpp>` or `<tessera/engine.hpp>` carries no stability promise, which
is enforced by tier documentation rather than by withholding the file.

Revisiting this needs evidence about what consumers actually include, which does
not exist yet because there are no consumers. A decision that requires data we do
not have is better deferred than guessed.

# Addendum (separate concern): the clean-clone test found what no gate could

Recorded 2026-08-29, immediately before tagging v0.1.0.

Every gate in this project ran against a working tree that already had its git
submodules checked out. The first genuine clean clone -- `git clone` followed by
the README's build command, exactly as a new user would -- failed at configure:

```
CMake Error at test/CMakeLists.txt:12 (add_subdirectory):
  The source directory /tmp/cc/test/external/catch
  does not contain a CMakeLists.txt file.
```

Two defects, neither visible to any existing check:

1. **The README omitted `--recursive`.** Nothing tested from a fresh clone, so
   nothing could have caught it.
2. **The failure mode was hostile.** The error names a missing `CMakeLists.txt`
   inside a third-party directory. It does not say "a submodule is missing", and
   it does not say "you do not need this at all if you only want the library".

The second is the more important fix. A missing Catch2 now disables the test
suites with an actionable message instead of failing configure:

```
Tests are disabled: the Catch2 submodule is not checked out.
  To build the tests, run:  git submodule update --init --recursive
  To silence this warning:  cmake -DTESSERA_NO_TESTS=ON ...
```

**Someone who wants to use Tessera as a library should not be blocked by a test
dependency.** That is the difference between a project a stranger can adopt and
one they bounce off in the first two minutes.

This is the sixth instance of a gate passing while the property it implied was
false, and the only one that could not have been caught by widening an existing
check -- it needed a different *kind* of test: not "does the tree build" but
"does a stranger succeed".
