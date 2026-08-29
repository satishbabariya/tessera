# Findings

What was actually true, as opposed to what the code, its names, or its
documentation claimed. Each document records something discovered during the
fork, with the evidence, so it need not be rediscovered.

Read these before changing the area they describe.

## Start here

| | |
|---|---|
| [0a-thesis-validation.md](0a-thesis-validation.md) | **The bundled sync server works.** 461 tests pass against it. This is the finding the whole project rests on: it made Phase 1 a revival rather than a greenfield build |
| [0b-sanitizers-clean.md](0b-sanitizers-clean.md) | ASan, UBSan and TSan are clean over CoreTests and SyncTests. The first hard evidence for the "high-quality engine" claim, and the baseline Phase 1 will be measured against |

## Before touching sync

| | |
|---|---|
| [0b-protocol-spec-drift.md](0b-protocol-spec-drift.md) | `doc/protocol.md` documents version 1; the code implements 14. Smaller than it sounds: 12 messages are documented and implemented, 7 are obsolete, **0 need writing** |
| [0a-flx-deferred.md](0a-flx-deferred.md) | Why FLX was not removed. ~394 references woven through the session lifecycle, and `test_client_reset.cpp` alone holds 76 |
| [0a-i1-i2-flx-boundary.md](0a-i1-i2-flx-boundary.md) | Core replication is entirely FLX-free; `client_reset` is protocol, not FLX. Bounds the blast radius of removing it |
| [0b-self-hostable.md](0b-self-hostable.md) | The README promised a self-hostable sync server. The package exports no server target, installs no server header, and there is no server executable. The server exists only inside the build tree |
| [0a-app-services.md](0a-app-services.md) | The identity and configuration model Phase 0a removed. Phase 1 must supply a replacement, not repoint a URL |
| [0b-merge-carve.md](0b-merge-carve.md) | How `tessera-merge` was separated, and how its central claim was false for a while |

## Before changing the build

| | |
|---|---|
| [0a-supply-chain.md](0a-supply-chain.md) | The vendor dependency ran four levels deep and reached downstream consumers. Also records the Android OpenSSL decision and how it resolved |
| [0b-test-resource-race.md](0b-test-resource-race.md) | Three test targets share one resources directory and copied into it concurrently. Invisible until a resource actually changed, because `copy_if_different` writes nothing when nothing differs |
| [0b-header-macro-visibility.md](0b-header-macro-visibility.md) | `RobustMutex::is_robust_on_this_platform` was false in every translation unit, including its own, while the implementation below it compiled full robust-mutex support. Five tests gated on it and none ran |
| [0a-toolchain-rot.md](0a-toolchain-rot.md) | The upstream tree did not compile on a 2026 toolchain. Also why Bison 3.8.2 is not a prerequisite |
| [0b-format-rejection-untested.md](0b-format-rejection-untested.md) | The fork's central promise -- that Tessera opens no other format -- was asserted in two documents and tested nowhere. A missing test that something fails looks exactly like a passing suite |
| [0b-file-format.md](0b-file-format.md) | A format version is a position in a lineage, not a number. Four constants encoded it, each failing differently |
| [0b-header-tiers.md](0b-header-tiers.md) | The public API is 147 headers; 236 were installed. Why `impl/` is not private, and why the install set was not culled |

## Before running or trusting the tests

| | |
|---|---|
| [0b-uncompiled-test-file.md](0b-uncompiled-test-file.md) | `test_util_enum.cpp` was in no CMakeLists and had never run. A test file left out of the build does not fail, does not show as skipped, and does not break anything |
| [0a-flaky-and-slow-tests.md](0a-flaky-and-slow-tests.md) | `reports DNS error` is network-flaky (0.011s to 680s across four runs). The suites leak temp directories, and a large `TMPDIR` degrades some tests 40,000x |
| [0b-certificate-expiry.md](0b-certificate-expiry.md) | The SSL tests' certificates were 57 days from expiry, and had lapsed five times before. The failure would have shown as a cluster of socket tests failing on every platform with no commit to blame |
| [0a-include-hygiene.md](0a-include-hygiene.md) | An include can be unused by its host file and load-bearing for its consumers. Seven such dependencies have had to be made explicit |
| [0b-identity-strings.md](0b-identity-strings.md) | The rename renamed the code, not what the code says. The product name, the sync User-Agent, the HTTP Server header, a WebSocket subprotocol, five error categories and six tools' help text still said realm |
| [0b-rename-blind-spots.md](0b-rename-blind-spots.md) | What a code-identifier rename cannot see: message strings, a wire identifier, a Keychain item, a log-category API, and comments it turned into false claims |
| [0a-existing-documentation.md](0a-existing-documentation.md) | `doc/` was not cruft. It holds a 1,126-line protocol specification and the formal merge algebra |

## The pattern these share

Two failure modes recur across almost every document here, and they are worth
stating on their own because they are not specific to this codebase.

**An inherited name describes history, not structure.** Six times, a label did
not match the code beneath it: the `REALM_APP_SERVICES` guard contained core sync
types; `audit.hpp` was a cross-platform interface with one platform-specific
implementation; `src/external/s2` is a downstream fork that calls into this
project; `upgrade_file_format` also upgraded history schemas; `impl/` is not
private; `protocol.hpp` held three plain integers the merge engine needed. Each
was found by reading the body or by a failing build. None by trusting the name.

**A check verifies what it was written to look for, and silently approves
everything else.** The vendor-host check searched URLs and missed a
vendor-owned CI action, then missed a git remote. The test gate covered one
suite, then one target, then one configuration, then ran against a stale binary.
The merge dependency check read `#include` directives while claiming build
independence. The consumer smoke test installed from a local build tree while
claiming a stranger could use the package. In every case the property being
asserted was *adjacent to* the property being inspected, and the defect lived in
the gap.

The practical consequence: **the checks you can run where you are cannot see the
conditions of somewhere else.** What found real defects was a foreign toolchain,
a fresh clone, and an unusual build configuration -- not better reasoning about
the tree from inside it.
