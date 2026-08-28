# Finding: "unused include" analysis must consider consumers, not just the host file

Date: 2026-08-28
Task: Phase 0a Task 9 (C API deletion)
Status: mistake made and corrected; process rule added

## What happened

`src/realm/sync/noinst/pending_bootstrap_store.hpp:24` included
`realm/object-store/c_api/util.hpp` — a *sync* header depending on the *C API*,
which is both a layering violation and a blocker for deleting the C API.

A grep confirmed the header referenced none of `util.hpp`'s symbols
(`wrap_err`, `to_capi`, `set_out_param`, …), so it was removed as dead.

**It was not dead.** It had been transitively supplying
`<realm/transaction.hpp>`. Removing it produced 21 errors across
`sync/client.cpp` and `sync/noinst/pending_bootstrap_store.cpp`, all variants of
"incomplete type `Transaction`", plus one "incomplete return type `TableView`".

The flaw in the analysis: symbol usage was checked **in the file containing the
include**, never in that file's *consumers*. An include can be unused by its host
and still load-bearing for every translation unit that includes it.

This trap is sharper here than in most codebases because the project compiles
with `_LIBCPP_REMOVE_TRANSITIVE_INCLUDES` (`CMakeLists.txt:157`) — it has already
opted into strict hygiene for *standard* headers, while its own headers still
rely on accidental chains. The strictness removes the slack that would otherwise
hide a mistake like this.

## How it was fixed

Not by reverting. `<realm/transaction.hpp>` and `<realm/table_view.hpp>` were
added to the two `.cpp` files that actually dereference those types.
`pending_bootstrap_store.hpp` needs no include — it only declares
`Transaction&` parameters, for which `realm/db.hpp`'s forward declaration
suffices.

The result is strictly better than the original: an explicit dependency on the
correct header, instead of an implicit one routed through a C API being deleted.

## A second, more dangerous failure in the same step

The CMake configure had *also* failed in that run — `export()` at
`CMakeLists.txt:380` still listed the deleted `RealmFFI` and `RealmFFIStatic`
targets. The build then fell back to a **stale cache** and the test run reported
`All 1665 tests passed`.

A green test result immediately after a failed configure. Taking it at face value
would have meant committing a tree that does not configure, with evidence that
appeared to prove the opposite.

## Process rules adopted for Tasks 10-14

1. **Never conclude an include is dead from single-file analysis.** Remove it and
   rebuild; the compiler is the only authority. Where a header is genuinely
   needed only for a reference or pointer, prefer a forward declaration in the
   header and a real include in the `.cpp`.
2. **Gate on configure, build, and test exit codes independently**, and assert
   each. A test pass that follows a failed configure is not evidence — it is a
   stale artifact. `grep -c 'CMake Error'` on the configure log, then the build
   exit code, then the test exit code.
3. **Deleting a target requires grepping for its name in `export()`,
   `install()`, and `target_link_libraries()`**, not only in `add_subdirectory`
   and source includes. `RealmFFIStatic` survived in
   `test/object-store/CMakeLists.txt:98` after the sources were gone.
