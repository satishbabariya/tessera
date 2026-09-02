# A crash told the user to report it to a different project

Tessera's terminate handler ended every abort with:

```
!!! IMPORTANT: Please report this at https://github.com/realm/realm-core/issues/new/choose
```

This shipped in v0.3.0. Anyone whose Tessera process aborted was told, by
Tessera, to report the crash to a project that did not write the code that
failed and cannot act on the report.

It surfaced by accident. The new load test crashed on its first run -- a missing
socket provider in its own configuration, entirely my mistake -- and the
backtrace ended with that line.

## Why the rename check did not catch it

`tools/check-rename-residue.sh` looks for pre-rename identifiers and identity
strings. This is a URL. It survived a tree-wide rename precisely because the
rename had no reason to touch it: nothing in
`https://github.com/realm/realm-core/issues/new/choose` is a C++ identifier or a
product name in the sense that check means.

One URL *was* touched, and wrongly. `sync_manager.hpp` cited upstream as

```
https://github.com/realm/realm-sync/blob/develop/src/tessera/sync/client.hpp#L126
```

realm-sync never had a `src/tessera`. The rename rewrote a path inside a
citation and produced a link that resolves to nothing. Restored to
`src/realm/sync/client.hpp`, which is what it cites.

## The distinction the new check draws

`tools/check-no-upstream-urls-in-output.sh` refuses a Realm URL inside a string
literal and ignores one inside a comment. That asymmetry is the whole design:

```cpp
// See <https://github.com/realm/realm-core/issues/3005> for details.   <- keep
ss << "... report this at https://github.com/realm/realm-core/...";     <- refuse
```

The comment is provenance. It records why a workaround exists, and rewriting it
would destroy a citation to a real issue that really explains the code. The
string is something a user can be shown.

## Two versions of the check that did not work

Both were caught by the canary, and neither would have been caught by anything
else, because both *passed* against the defect they were written for.

The first stripped comments before searching:

```sh
sed 's|//.*||'
```

which deletes the URL, because `https://` contains `//`.

The second skipped lines matching `:[[:space:]]*//` -- "a colon, then a comment
marker". `https://` matches that too: the colon of the scheme, then the slashes.

A URL is remarkably good at looking like a comment. The working version removes
the `path:line:` prefix that `grep -rn` adds and then decides on what the source
line actually begins with.

The general point is the one this repository keeps arriving at from new
directions: a check is worth nothing until it has been watched to fail. Twice
here the check was written, run against a clean tree, and reported PASS -- and
twice that PASS meant the check was broken rather than the tree was clean.
