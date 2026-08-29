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
tools/check-copyright-notices.sh $(tools/check-copyright-notices.sh | cut -d' ' -f1)
tools/check-no-vendor-hosts.sh
tools/check-layering.sh
tools/check-merge-deps.sh
tools/check-header-tiers.sh
tools/verify/consumer-smoke-test.sh build.debug
```

Each encodes a structural decision. If one fails, the fix is usually to
reconsider the change rather than to relax the check.

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
