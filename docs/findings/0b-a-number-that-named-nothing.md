# A check that said the surface changed, but not which header

`tools/check-install-surface.sh` recorded the installed header surface as two
integers and failed when the tree disagreed:

```
FAIL: the installed header surface changed
    .hpp  expected 231, found 230
    .h    expected 4, found 4
```

That is a true statement and a useless one. One header differed, and the check
knew which -- it had both lists in hand when it counted them -- and it reported
a subtraction instead. Learning the name took a push, a CI run, and a guess, and
the guess was wrong four times:

* Debug versus Release. Measured: identical, 231 each.
* A stale local build directory. Measured: reconfiguring changed nothing.
* `TESSERA_ENABLE_GEOSPATIAL`, the one conditional install rule. Measured: on by
  default, so on in CI too, and its header was present in both trees.
* An untracked or generated header sneaking into an install glob. This one led
  somewhere: it surfaced `engine.hpp` at the include root, a file that #59 had
  stopped installing.

The actual cause was that the branch predated #59. CI builds the pull request's
merge commit, so CI installed the fixed surface and the branch alone installed
one header more -- on both platforms, which is why every job was off by exactly
one and the platform gap of two was never in question.

Every one of those five hypotheses was answerable from the list the check
already had.

## What replaced it

`tools/install-surface.txt`, one path per line, diffed against the installed
tree. A failure now reads:

```
    These are in the manifest but did not install:
      - tessera/version.hpp
```

The two headers that install only under `if(APPLE)` carry an `apple:` prefix in
the same file rather than living in a second list, so a header cannot be added
to one platform's expectations and forgotten in the other's. That was the shape
of the count's first CI failure: the number was recorded on macOS and asserted
on Linux.

## The canary found a bug in the replacement

Removing one header made the new check blame two others:

```
    These install but are not in the manifest:
      + tessera/version_id.hpp
      + tessera/version_numbers.hpp

    These are in the manifest but did not install:
      - tessera/version.hpp
```

Both lists were sorted with `LC_ALL=C` and both were sorted correctly. `comm`
validates its input's order using the ambient locale, and under `en_US.UTF-8`
punctuation collates differently enough that `version.hpp` and `version_id.hpp`
swap places. `comm` concluded the input was unsorted and emitted nonsense
quietly -- no warning, exit status 1, a plausible-looking report naming the
wrong files.

Sorting in one collation and comparing in another is invisible until two paths
differ only by punctuation. The script now exports `LC_ALL=C` once, for both.

Had the canary only checked that a *new* header is caught, this would have
shipped: that direction passed. It took removing a header, and specifically a
header with a punctuation neighbour, for the bug to appear at all.

## The lesson

A check gets its diagnosis right or somebody else pays for it later, with
interest, over several CI runs. When a check has the evidence in hand at the
moment it fails, it should print the evidence. A count discards it.
