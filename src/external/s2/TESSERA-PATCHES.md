# This copy of S2 is not pristine upstream

Realm patched this vendored S2 to depend on its own infrastructure, and those
patches were inherited by Tessera:

- `base/logging.h` routes S2's `CHECK`/`DCHECK` macros to this project's
  assertion macros (`TESSERA_ASSERT*`) and includes `tessera/util/assert.hpp`,
  `logger.hpp` and `to_string.hpp`.
- `util/math/mathutil.h` includes `tessera/util/features.h` and tests
  `TESSERA_WATCHOS` / `TESSERA_APPLE_DEVICE`.
- `s2polygon.cc`, `s2loop.cc`, `s2polyline.cc`, `s2cellid.cc` and others build
  error strings with `tessera::util::format`.

Upstream S2 has none of these dependencies.

**Consequence for the rename:** the Phase 0b rule "never rename `src/external/**`"
protects *third-party* code. These particular symbols are not S2's -- they are
downstream edits that call into this project -- so they follow the realm ->
tessera rename. S2's own identifiers are untouched.

**Consequence for future upgrades:** upgrading S2 is not a drop-in replacement.
These patches must be re-applied, or S2 must be de-coupled from this project's
logging and formatting first. That is worth doing at some point; the coupling
buys little and complicates every upgrade.
