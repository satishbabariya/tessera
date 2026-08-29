# Finding: the upstream baseline does not build on a modern toolchain

Date: 2026-08-28
Task: Phase 0a Task 1 (baseline) / Task 3 (toolchain floor)
Baseline: realm-core v14.14.0, commit f8752e180, pristine

## Summary

At the pristine upstream commit, `realm-core` **fails to build** with Apple
clang 21.0.0 / libc++ (macOS arm64, C++17 as configured upstream). This is not a
Tessera regression — it is the state of the dormant upstream.

The failures are all one species: **the toolchain moved and nobody was left to
follow.** Two of the last three real commits upstream ever made were of exactly
this kind (`add missing cstdlib header in cli_args.cpp` #8089, `Fix Xcode 27
Build error` #8096), which is a fair operational definition of "dormant": the
engine is sound, its build assumptions rotted.

## Failures and fixes

| # errors | Location | Root cause | Fix |
|---|---|---|---|
| 79 | `src/external/s2/base/macros.h:163`, surfacing in 6 s2 headers | `DECLARE_POD` and `PROPAGATE_POD_FROM_TEMPLATE_ARGUMENT` specialise `std::is_pod`. libc++ now marks it `_LIBCPP_NO_SPECIALIZATIONS`, making user specialisation a hard error; C++20 also deprecates the trait. | Both macros neutralised to no-ops, retaining the trailing `typedef` so existing `DECLARE_POD(foo);` call sites stay valid. |
| 1 | `test/util/spawned_process.cpp:203` | `std::back_inserter` used without `<iterator>`; newer libc++ dropped the transitive include. | Added `#include <iterator>`. |
| 6 | `src/realm/util/scope_exit.hpp:64-65` | `std::is_nothrow_destructible` and `std::declval` used with only `<exception>` and `<optional>` included. | Added `#include <type_traits>` and `#include <utility>`. |

### Why neutralising DECLARE_POD is safe, not expedient

The macro's own comment states it existed solely to inform google3 containers
(`compact_vector`, `sparse_hash_map`) that a type is POD. Neither container
exists in this tree, and a direct search confirms **nothing in `src/realm/` or
`src/external/s2/` reads `std::is_pod` for any s2 type**. The specialisations
were dead weight before they became illegal. Marked with a `// Tessera:` comment
per the plan's third-party-patch rule so it survives a future s2 upgrade.

## Implications for the plan

1. **Sequencing is vindicated.** Had demolition come first, these failures would
   have surfaced interleaved with 26k lines of deletion diffs, and each would
   have been ambiguous: did the deletion break it, or was it already broken?
   The beachhead guarantees every later failure has exactly one candidate cause.
2. **Task 3 (C++20) is larger than estimated.** These failures appear at C++17.
   Raising to C++20 will surface more of the same species, and the plan's
   "fix nothing on the deletion manifest" rule is what keeps that bounded.
3. **`src/external/` needs a patch policy, not a prohibition.** The plan
   originally said third-party code is never modified; that was too absolute.
   Amended: never renamed or refactored, patchable for compiler compatibility
   with a mandatory `// Tessera:` marker.
4. **Windows and Linux are unmeasured.** These findings are macOS/libc++ only.
   Expect a distinct set from libstdc++ and MSVC when CI lands in Task 4.

## Addendum: Bison 3.8.2 is a hard Phase 0b prerequisite

Surfaced from a configure warning during Task 9:

```
Could NOT find BISON: Found unsuitable version "2.3",
but required is exact version "3.8.2" (found /usr/bin/bison)
```

`src/realm/parser/CMakeLists.txt:2-3` requires **exact** versions:
`find_package(BISON 3.8.2 EXACT)` and `find_package(FLEX 2.6.4 EXACT)`. This
host has bison 2.3 (the ancient GPLv2 version Apple ships) and flex 2.6.4
(satisfied).

**Today this is only a warning**, because the parser is built from checked-in
generated files in `src/realm/parser/generated/`, and those are used when bison
is absent.

**In Phase 0b it becomes blocking.** The rename plan states that generated parser
files must be *regenerated* from `query_bison.yy` / `query_flex.ll`, never
hand-edited — and the generated files do carry `realm` identifiers
(`query_bison.cpp`: 4, `query_flex.cpp`: 2), so they cannot simply be left alone
either.

Phase 0b must therefore either:

1. Obtain bison exactly 3.8.2 — awkward on this host, since Homebrew is broken
   (see the environment notes in `docs/baseline-2026-08-28.md`); or
2. Relax the `EXACT` requirement to a minimum version and verify the generated
   output still compiles and passes `test_parser.cpp`; or
3. Treat the small number of `realm` identifiers in the generated files as an
   exception to the never-hand-edit rule, documented as such.

### Resolved 2026-08-29: option 3, and the risk that worried me is absent

Investigated rather than assumed. The first two options are dead ends and the
third turns out to be safe.

**Option 1** is unavailable while Homebrew is broken on this host.

**Option 2 cannot work at any version constraint.** `query_bison.yy` uses
`%define api.token.constructor` (line 6), `%define api.value.type variant`
(line 7) and `%define api.symbol.prefix` (line 65) — all **Bison 3.x-only
features**. Bison 2.3 cannot process this grammar however the requirement is
phrased, so relaxing `EXACT` to a minimum merely moves the failure.

**Option 3 is safe, because the generated files barely mention `realm`.** The
rename surface in generated output is six references, every one a mechanical
include or namespace qualifier:

| File | References |
|---|---|
| `generated/query_bison.cpp` | 2 `#include`, 2 `using namespace` |
| `generated/query_flex.cpp` | 1 `#include`, 1 qualified function definition |

The grammar sources carry the matching set: `query_bison.yy` has 9,
`query_flex.ll` has 2.

**The rule's rationale is preserved, not waived.** "Never hand-edit generated
files" exists to prevent generated output drifting from its grammar source.
Renaming *both* consistently maintains exactly that property: regenerating later
with Bison 3.8.2 against the renamed `.yy`/`.ll` produces equivalent output. This
is a uniform rename, not an edit that diverges from its source.

**And the specific fear was misplaced.** The original concern was that "a subtly
different parser is among the worst kinds of regression to diagnose." That risk
comes from *regenerating with a different Bison version*, which produces
genuinely different output. Not regenerating avoids it entirely. Verification is
strong regardless: `test/test_parser.cpp` provides **73 tests across 6,597 LOC**,
so a bad substitution fails loudly and immediately.

**Phase 0b action:** include `parser/generated/**` in the rename script rather
than excluding it, and gate on the parser tests. Bison 3.8.2 becomes a
nice-to-have for future grammar work, not a prerequisite for the rename.
