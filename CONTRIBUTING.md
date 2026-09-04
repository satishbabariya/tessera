# Contributing

## Filing issues

Please include:

1. What you did
2. What you expected to happen
3. What actually happened
4. Steps to reproduce, ideally as a failing test
5. Version or commit of Tessera
6. Platform, compiler and CMake version

A reproduction in the form of a test case in `test/` is worth more than any
amount of description.

## Contributing code

Pull requests are welcome. Before opening one:

**Build everything and run every suite.** Not a subset:

```sh
cmake -B build.debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build.debug -j"$(getconf _NPROCESSORS_ONLN)"   # no target: build all
TMPDIR=$(mktemp -d)/ ./build.debug/test/tessera-tests.app/Contents/MacOS/tessera-tests
TMPDIR=$(mktemp -d)/ ./build.debug/test/tessera-sync-tests.app/Contents/MacOS/tessera-sync-tests
TMPDIR=$(mktemp -d)/ ./build.debug/test/object-store/tessera-object-store-tests.app/Contents/MacOS/tessera-object-store-tests
```

On Linux the binaries are at `build.debug/test/tessera-tests` and so on, without
the `.app` bundle.

Two things about that command that are not incidental:

- **`cmake --build` with no target.** The test suites do not compile the
  command-line tools, so a target-limited build silently excludes code from
  verification. This has caused breakage twice in this project's history.
- **A fresh `TMPDIR`.** The suites leak temporary directories, and a large
  temp directory degrades some tests by four orders of magnitude. A slow suite
  is usually this, not your change.

**Run the invariant checks:**

```sh
PREFIX="$(mktemp -d)/tessera"
cmake --install build.debug --prefix "$PREFIX" > /dev/null

for c in tools/check-*.sh tools/test-*.sh; do
  case "$c" in
    *copyright*) "$c" 560 ;;
    *)           "$c" "$PREFIX" ;;
  esac
done
tools/verify/consumer-smoke-test.sh build.debug
```

Each encodes a structural decision. If one fails, the fix is usually to
reconsider the change rather than to relax the check.

Two things about that loop, both of which it got wrong before:

- **It is a glob.** This section used to name five checks. There are sixteen,
  and the five it named were the five that existed when it was written. A list
  that enumerates its own members drifts behind them, which has now happened
  three times in this repository -- here, in `docs/RELEASING.md`, and in the
  workflow that runs these checks in CI.

- **The copyright count is the literal 560.** It used to read

  ```sh
  tools/check-copyright-notices.sh $(tools/check-copyright-notices.sh | cut -d' ' -f1)
  ```

  The bare script prints how many notices it *found*. Feeding that number back
  as the number it *expects* makes the check pass whatever it finds: delete ten
  notices and it reports `PASS: 550 copyright notices intact`. It was a licence
  compliance check a contributor could not fail. CI has always passed the
  literal 560, so the two disagreed about what was being verified, and only the
  contributor-facing one was wrong.

Some of the checks want an installed tree, so the prefix is built first and
handed to every one of them. The ones that do not want it ignore an extra
argument.

## What has to pass before a pull request can merge

Six checks are required on `main`, and GitHub will not let a pull request merge
until all six are green:

```
ubuntu-latest gcc Debug      ubuntu-latest gcc Release
ubuntu-latest clang Debug    macos-latest clang Debug
macos-latest clang Release   changelog
```

That is a branch protection rule rather than anything in this repository, so it
is written down here: an invisible setting is one nobody can plan around.

Three details of how it is configured, each deliberate:

- **Branches do not have to be up to date with `main` before merging.** Requiring
  that would force a rebase and a fresh CI run for every merge whenever anything
  else lands first, which is expensive and, for a stack of pull requests, close
  to unworkable.
- **Administrators are not bound by it.** The rule exists to stop a red build
  merging by accident, not to leave the maintainer unable to land a fix when
  something is broken.
- **`CodeRabbit` is not required.** It is a review bot; if it stopped running,
  requiring it would block every merge.

Force-pushing to `main` and deleting it are both blocked.

The rule was added after a pull request merged the instant it was asked to,
without waiting for the CI its rebase had invalidated. Nothing was wrong with
that change, but nothing would have stopped one that was: until then the checks
were advisory, in a project whose recurring finding is that a check nobody has
seen fail is not a check.

If a job is ever renamed or removed from the matrix in
`.github/workflows/build.yml`, the required context list has to change with it.
A required check that no longer runs never turns green, and every merge blocks
until somebody notices.

## Things that will get a patch rejected

**Removing a copyright notice.** 560 files carry `Copyright ... Realm Inc.`
headers. Apache 2.0 §4(b) requires retaining them in derivative works. They are
not stale text to tidy up. `tools/check-copyright-notices.sh` enforces this, and
a change that trips it should be reverted rather than fixed forward.

**Adding a build dependency on a host we do not control.** No downloading
prebuilt artifacts at configure time. Dependencies come from the environment.

**Making `tessera-merge` depend on the protocol or transport.** The merge engine
is usable standalone, and that property is easy to lose by accident.

## Style

Follow the surrounding code. `.clang-format` covers formatting; run
`git clang-format` before committing. The prose style guide is
[docs/development/coding_style_guide.cpp](docs/development/coding_style_guide.cpp).

## Changelog

Add an entry to `CHANGELOG.md` under the current unreleased section. Write it
for someone deciding whether to upgrade, not for someone reading the diff:

```
* Fixed a deadlock when opening a database from two processes on Windows
  (#123, since v0.1.0)
```

## Commit messages

Explain why, not what -- the diff already says what. If a change corrects an
earlier mistake, say what the mistake was; that is usually the most useful part
of the message six months later.

## Contributor licence agreement

**Not yet decided.** Tessera does not currently require a CLA. The upstream
project required one assigning rights to Realm Inc.; that agreement does not
apply to this fork and contributors should not sign it for work submitted here.

If a CLA is adopted, it will be announced before it is required, and it will not
be applied retroactively.

Contributions are accepted under Apache 2.0, the licence of the project.
