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
| [0b-base64-empty-input.md](0b-base64-empty-input.md) | `base64_decode("")` aborted the process, and `AccessToken::parse("")` with it. Unreachable only because nothing authenticates; a remote abort the moment something does |
| [0b-tokens-without-tests.md](0b-tokens-without-tests.md) | Fourteen access-token fixtures, six unused, and three tests that all assert nothing goes wrong. A suite that only asserts acceptance cannot tell a system that accepts the right things from one that accepts everything |
| [0b-client-terminates-on-errors.md](0b-client-terminates-on-errors.md) | Twelve protocol errors aborted the client, including the two a server sends to reject a bind. A client could not survive being told its authentication failed |
| [0b-auth-belongs-at-the-handshake.md](0b-auth-belongs-at-the-handshake.md) | The client has always sent a token, as `?baas_at=` on the WebSocket handshake. Upstream moved authentication out of BIND deliberately in 2022. Read this before adding a check to BIND |
| [0b-both-ends-of-the-token.md](0b-both-ends-of-the-token.md) | The client sends an empty token because the server ignores it, and the server ignores it because nothing sends one. Each end justified by the other |
| [0b-server-has-no-auth.md](0b-server-has-no-auth.md) | The sync server accepts a token on every bind, logs it, and never looks at it. 761 lines of access control that nothing calls. Read this before making the server installable |
| [0b-keyless-still-demanded-a-token.md](0b-keyless-still-demanded-a-token.md) | **A server with no public key refused every connection.** Keyless-ness was detected from a parse result, so it was unreachable for a client sending `?baas_at=` empty -- what a client does while its token is refreshing. `ObjectStoreTests` hung on all five platforms until the 60-minute timeout |
| [0b-self-hostable.md](0b-self-hostable.md) | The README promised a self-hostable sync server. The package exports no server target, installs no server header, and there is no server executable. The server exists only inside the build tree |
| [0b-server-installability-cost.md](0b-server-installability-cost.md) | What it would actually take to ship the server: 36 include sites, one header not yet installed, and 761 lines of App Services authentication behind four call sites |
| [0a-app-services.md](0a-app-services.md) | The identity and configuration model Phase 0a removed. Phase 1 must supply a replacement, not repoint a URL |
| [0b-merge-carve.md](0b-merge-carve.md) | How `tessera-merge` was separated, and how its central claim was false for a while |

## Before changing the build

| | |
|---|---|
| [0b-windows-invalid-filenames.md](0b-windows-invalid-filenames.md) | Twenty-one files with `<>` in their names made the repository uncloneable on Windows. The hygiene check looked for `.tess` artifacts; the test suite writes `.realm` ones |
| [0a-supply-chain.md](0a-supply-chain.md) | The vendor dependency ran four levels deep and reached downstream consumers. Also records the Android OpenSSL decision and how it resolved |
| [0b-test-resource-race.md](0b-test-resource-race.md) | Three test targets share one resources directory and copied into it concurrently. Invisible until a resource actually changed, because `copy_if_different` writes nothing when nothing differs |
| [0b-header-macro-visibility.md](0b-header-macro-visibility.md) | `RobustMutex::is_robust_on_this_platform` was false in every translation unit, including its own, while the implementation below it compiled full robust-mutex support. Five tests gated on it and none ran |
| [0a-toolchain-rot.md](0a-toolchain-rot.md) | The upstream tree did not compile on a 2026 toolchain. Also why Bison 3.8.2 is not a prerequisite |
| [0b-file-format.md](0b-file-format.md) | A format version is a position in a lineage, not a number. Four constants encoded it, each failing differently |
| [0b-header-tiers.md](0b-header-tiers.md) | The public API is 147 headers; 236 were installed. Why `impl/` is not private, and why the install set was not culled |

## Before running or trusting the tests

| | |
|---|---|
| [0b-format-rejection-untested.md](0b-format-rejection-untested.md) | The fork's central promise -- that Tessera opens no other format -- was asserted in two documents and tested nowhere. A missing test that something fails looks exactly like a passing suite |
| [0b-what-does-not-run.md](0b-what-does-not-run.md) | A count of every test that does not run: 31 disabled by `TEST_IF`, 10 inside `#if 0`, 0 never compiled. Before today the honest answer was that nobody knew |
| [0b-green-by-absence.md](0b-green-by-absence.md) | **Zero failures is not a pass.** Twice the merge stack read as healthy when it was stopped: once with no build matrix, once with cancelled runs. Both times a poll counted failures and absence counted as zero |
| [0b-hooks-nobody-runs.md](0b-hooks-nobody-runs.md) | **The `pre-push` hook rejected this repository.** Inherited from realm-sync, it whitelisted four `realm-sync` remotes and aborted on anything else -- every push, not just pushes to main. Nothing installed it, so it had never run once |
| [0b-uncompiled-test-file.md](0b-uncompiled-test-file.md) | `test_util_enum.cpp` was in no CMakeLists and had never run. A test file left out of the build does not fail, does not show as skipped, and does not break anything |
| [0a-flaky-and-slow-tests.md](0a-flaky-and-slow-tests.md) | `reports DNS error` is network-flaky (0.011s to 680s across four runs). The suites leak temp directories, and a large `TMPDIR` degrades some tests 40,000x |
| [0b-certificate-expiry.md](0b-certificate-expiry.md) | The SSL tests' certificates were 57 days from expiry, and had lapsed five times before. The failure would have shown as a cluster of socket tests failing on every platform with no commit to blame |
| [0a-include-hygiene.md](0a-include-hygiene.md) | An include can be unused by its host file and load-bearing for its consumers. Seven such dependencies have had to be made explicit |
| [0b-identity-strings.md](0b-identity-strings.md) | The rename renamed the code, not what the code says. The product name, the sync User-Agent, the HTTP Server header, a WebSocket subprotocol, five error categories and six tools' help text still said realm |
| [0b-rename-blind-spots.md](0b-rename-blind-spots.md) | What a code-identifier rename cannot see: message strings, a wire identifier, a Keychain item, a log-category API, and comments it turned into false claims |
| [0a-existing-documentation.md](0a-existing-documentation.md) | `doc/` was not cruft. It holds a 1,126-line protocol specification and the formal merge algebra |

## The pattern these share

Three failure modes recur across almost every document here, and they are worth
stating on their own because none of them is specific to this codebase.

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
claiming a stranger could use the package. The rename check scanned code
identifiers and reported clean while the product name, the sync `User-Agent` and
a WebSocket subprotocol still said `realm`. And a check added *in response to all
of this* was first called `check-tests-compiled.sh`, which greps for a filename
in a `CMakeLists` -- it establishes that a file is visible to the build, not that
anything compiles it, and it was renamed before it merged. In every case the property being
asserted was *adjacent to* the property being inspected, and the defect lived in
the gap.

The practical consequence: **the checks you can run where you are cannot see the
conditions of somewhere else.** What found real defects was a foreign toolchain,
a fresh clone, and an unusual build configuration -- not better reasoning about
the tree from inside it.

**A stale result is indistinguishable from a fresh one.** This one cost more
time in a single day than the other two combined, in four different disguises.

`make` treats a source file that is not *newer* than its object as up to date,
and its timestamps have one-second granularity. An edit-build-test cycle fast
enough to land inside one second silently runs the previous binary. Four
measurements in this project were taken that way, including one that was written
into a pull request as evidence. Reproducible in eight lines:

    cmake -S . -B b -G "Unix Makefiles" && cmake --build b   # main returns 1
    python3 -c "open('src/main.cpp','w').write('int main(){return 7;}')"
    cmake --build b && ./b/app                               # still returns 1

`test/CMakeLists.txt` copies the certificate fixtures into the test bundle at
*build* time. Regenerating them and running the suite without rebuilding tests
the old certificates. That produced "SSL suite 21/21, sync suite 461/461" for
certificates that in fact broke five tests -- real numbers, measured against the
files the bundle already held.

GitHub does not create workflow runs for a pull request that cannot be merged,
and goes on displaying the checks from before the conflict. Two commits were
pushed to a branch and neither was ever built, while `gh pr checks` reported
everything green.

And a test that was never compiled reports exactly what a passing test reports:
nothing. `1662 tests passed` is the same sentence whether the 1663rd ran, was
switched off, or never existed.

The common shape is that freshness is not part of the result. A number, a green
tick and a passing suite all look the same regardless of what produced them, so
the question "is this measuring the thing I just changed?" has to be asked
separately every time -- and answered by evidence, not by having intended it.
