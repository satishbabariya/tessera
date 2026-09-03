# A licence-compliance check a contributor could not fail

`CONTRIBUTING.md` told contributors to run the invariant checks like this:

```sh
tools/check-copyright-notices.sh $(tools/check-copyright-notices.sh | cut -d' ' -f1)
```

Run bare, that script prints how many notices it **found**:

```
560 files retain a Realm Inc. copyright notice
```

Given an argument, it compares that argument to what it finds:

```
$ tools/check-copyright-notices.sh 559
FAIL: copyright notice count changed: expected 559, found 560
  Apache 2.0 section 4(b) requires retaining these notices.
  A transformation that removes them is a licence violation, not a bug
  to fix forward: revert it.
```

The documented command feeds the first into the second. It passes whatever it
finds, always. Delete ten notices and it reports `PASS: 550 copyright notices
intact`.

The check itself is sound and CI has always invoked it correctly, with the
literal `560`. So the two invocations disagreed about what was being verified,
and the one that was wrong was the one aimed at people least likely to know
better -- an outside contributor, running the command the project told them to
run, before touching the files the check exists to protect.

`CONTRIBUTING.md` is also where the project explains that removing a copyright
notice will get a patch rejected under Apache 2.0 §4(b). It said that on the same
page as a command that could not detect it.

## The list beside it had drifted too

The same block named five checks. There are sixteen. The five were the five that
existed when it was written.

`docs/RELEASING.md` hit this and fixed it with a glob, recording why: *"a gate
that enumerates its own members drifts behind them."* Then its glob drifted
anyway, because it matched `check-*.sh` and two suites are named `test-*.sh`.
The workflow that runs these checks in CI enumerates them too, which is why a
repository-hygiene check now verifies that every check script is named by some
workflow.

Three files, one lesson, three separate discoveries. The contributor-facing copy
was the last to be looked at, which is the wrong order: it is the one read by
people who cannot tell that the list is short.

## Both are now the same loop

```sh
for c in tools/check-*.sh tools/test-*.sh; do
  case "$c" in
    *copyright*) "$c" 560 ;;
    *)           "$c" "$PREFIX" ;;
  esac
done
```

Which is the loop from `RELEASING.md`, with the same literal count and the same
install prefix handed to every check. Verified by running it verbatim.
