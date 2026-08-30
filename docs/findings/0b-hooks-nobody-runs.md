# The pre-push hook rejected this repository

`tools/pre-push`, inherited from realm-sync, compared the push destination
against four permitted remotes:

```
git@github.com:realm/realm-sync.git
https://github.com/realm/realm-sync.git
git@github.com:kspangsege/realm-sync.git
https://github.com/kspangsege/realm-sync.git
```

and aborted otherwise. This repository is `satishbabariya/tessera`, so the hook
rejected its own origin. Fed the repository's real remote:

```
$ bash tools/pre-push origin "$(git remote get-url origin)"
attempting to push to a repo other than realm-sync or git remotes contains a
repo other than realm-sync. aborting...
$ echo $?
1
```

Not "pushes to main are blocked". *Every* push blocked, including the first one
anybody tried.

## Why nobody noticed

Nothing installs these hooks. `.git/hooks` contains only the samples git ships.
Nothing references them: no workflow, no script, and not `tools/README.md` --
whose opening sentence is "What runs automatically, and what only runs when
someone types it", and which lists twenty-two files without mentioning either
hook. The file has existed since the fork and has never been executed once.

`check-rename-residue.sh` did not catch it either. That check looks for
pre-rename identifiers and identity strings; these are repository URLs, which is
a different shape of residue and one nothing was looking for.

This is the second thing found in this state, after
[`tools/verify/clean-clone-test.sh`](0b-uncompiled-test-file.md) -- written for a
real failure, documenting its own purpose, sitting in a directory called
`verify`, and referenced by no workflow for its entire existence. Both were
discovered by reading, not by failing, because neither could fail.

## What was done

The remote whitelist is gone. It guarded against pushing a fork to the wrong
shared upstream, which was a real hazard in realm-sync and is not one here:
there is one remote. In its place the hook enforces the rule this project
actually has -- changes reach `main` through a pull request -- with
`TESSERA_ALLOW_MAIN_PUSH=1` for the deliberate exception.

`tools/install-hooks.sh` installs them, so they can stop being decorative.
`tools/test-pre-push.sh` feeds the hook the `<local ref> <local sha> <remote
ref> <remote sha>` lines git puts on its stdin and asserts eight outcomes; it
runs in CI with the other invariant checks.

The canary is unusually direct, because the defect is still in git history: run
the new test against the old hook and five of eight assertions fail, including
the one named for the bug. Two of the three it passes -- "main is refused" and
"deleting main is refused" -- it passes by refusing everything, which is the
whole finding in miniature. A gate that rejects every input looks, from the
direction of the thing it is meant to stop, exactly like a gate that works.
