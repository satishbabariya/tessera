# tools/

What runs automatically, and what only runs when someone types it.

The distinction matters because it has been wrong before.
`tools/verify/clean-clone-test.sh` was written in response to a real failure,
documents its own purpose, sits in a directory called `verify`, and was
referenced by no workflow for its entire existence. Nothing about the file said
so. See [docs/findings/0b-uncompiled-test-file.md](../docs/findings/0b-uncompiled-test-file.md).

## Enforced on every pull request

`.github/workflows/build.yml` runs these before it configures anything, because a
structural violation should not wait on a build.

| | |
|---|---|
| `check-copyright-notices.sh` | Apache 2.0 §4(b) attribution survives tree-wide edits |
| `check-no-vendor-hosts.sh` | the build fetches nothing from hosts we do not control |
| `check-layering.sh` | no upward includes between layers |
| `check-merge-deps.sh` | `tessera-merge` depends only on storage |
| `check-header-tiers.sh` | the public API does not leak private headers |
| `check-repo-hygiene.sh` | no runtime artefacts, stray keys, or paths illegal on Windows |
| `check-test-sources-listed.sh` | every test source is named by a `CMakeLists` |
| `check-header-macros.sh` | no header decides on a macro only a `.cpp` defines |
| `check-rename-residue.sh` | no pre-rename identifiers, identity strings, or binary names in scripts, workflows, manifests and ignore files |
| `check-no-upstream-urls-in-output.sh` | a crash does not tell the user to report it to realm/* |
| `check-cert-expiry.sh` | test certificates are neither near expiry nor over Apple's 825-day ceiling |
| `test-pr-status.sh` | `pr-status.sh` still distinguishes a cancelled run from a running one |
| `test-pre-push.sh` | the `pre-push` hook refuses main and permits everything else |
| `verify/authorization-end-to-end.sh` | a scoped token is refused elsewhere, and a download-only token cannot upload -- through the shipped binaries, over a socket |
| `verify/survives-a-hard-kill.sh` | SIGKILL the server mid-life; the rows it acknowledged are still there and it reopens its directory |
| `verify/consumer-smoke-test.sh` | the installed package is consumable and exports the documented targets |
| `verify/test-binary.sh` | resolves a test binary's path across platforms; used by the workflows |

Each check is canary-tested: it has been shown to fail against the defect it
exists to catch. A gate that cannot fail is not a gate.

## Enforced nightly

`.github/workflows/nightly.yml`. Slow enough to be worth separating from the
merge gate.

| | |
|---|---|
| `verify/clean-clone-test.sh` | a stranger can clone the published repository and build it |

## Installed by hand, then run by git

`tools/install-hooks.sh` symlinks these into `.git/hooks`. They are opt-in, so
treat them as a convenience rather than a guarantee -- the pull-request checks
above are what actually holds.

| | |
|---|---|
| `pre-commit` | refuses a commit whose staged files are not clang-formatted. Needs `git-clang-format` on PATH; without it the hook passes everything |
| `pre-push` | refuses a direct push to `main`. `TESSERA_ALLOW_MAIN_PUSH=1` overrides |

Until now neither was listed here, neither was installable, and `pre-push` had
rotted into a hook that rejected this repository's own remote -- it whitelisted
four `realm-sync` URLs inherited from the fork. Nothing ran it, so nothing
noticed. See
[docs/findings/0b-hooks-nobody-runs.md](../docs/findings/0b-hooks-nobody-runs.md).

## Run by hand

Nothing runs these. They are here because they are occasionally useful, and none
of them is verified by anything -- if one has rotted, you will find out when you
run it. `test/benchmark-util-network/main.cpp` was in this state and had stopped
compiling three API changes ago.

| | |
|---|---|
| `analyse-zero-check-tests.sh` | lists tests that run and check nothing. Deliberately not a gate: 105 of the 106 are regression tests that assert by not crashing |
| `rename.sh` | the Realm-to-Tessera identifier rename. Kept as the record of what was done; see FORK.md |
| `coverage.sh` | builds with coverage instrumentation |
| `cross_compile.sh`, `build-android.sh` | Android cross-compilation |
| `build-cocoa.sh`, `build-apple-device.sh` | Apple platform packaging |
| `run-in-simulator.sh` | runs a test binary in an iOS simulator |
| `run-tests-on-exfat.sh` | runs the suite on an exFAT volume, which has its own path semantics |
| `generate-version-numbers-for-soong.sh` | version numbers for AOSP's Soong build system. Tessera has no Soong integration; the script still works and currently feeds nothing |
