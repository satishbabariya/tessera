#!/usr/bin/env bash
# Checks the repository's own surface, which no other check in this project
# looks at. Every other check inspects code -- layering, includes, exported
# symbols, installed headers. None of them notices a stray private key in the
# root, two documentation folders one letter apart, or a source file importing
# a module that was deleted.
#
# All of those were present after Phase 0b and none was detected by anything.
set -euo pipefail
FAIL=0

# 1. Runtime artifacts must not be committed. The engine writes coordination
#    files next to a database, and tests run from the repository root leave
#    them behind: .note is the notification FIFO, .cv files hold
#    condition-variable state.
#
#    This originally matched .tess artifacts only, because that is the format's
#    extension. The test suite does not use it: test_path.hpp still names its
#    files .realm, so twenty-one such files were committed and the check passed
#    over every one. It now matches both, and .mx as well -- the management
#    files that accompany them.
STRAY=$(git ls-files \
    | grep -E '(^|/)\.(note|new_commit\.cv|pick_writer\.cv)$|\.(tess|realm)(\.lock|\.management)?$|\.mx$' \
    || true)
if [ -n "$STRAY" ]; then
    echo "FAIL: runtime artifacts are committed:"; echo "$STRAY"; FAIL=1
fi

# 2. Every tracked path must be a legal filename on Windows. `git checkout`
#    there fails outright on < > : " | ? * and on a trailing dot or space, so a
#    single such file makes the repository uncloneable rather than merely
#    untidy -- the nightly Windows job failed at actions/checkout, before it
#    could configure.
#
#    The twenty-one files that caused it came from running the test binary with
#    an unrecognised argument: the framework treats it as a path prefix, so
#    `tessera-tests --help` wrote its temporary databases into the repository
#    root as `--helpCompaction_Large<std::__1::integral_constant<bool,+true>>...`.
ILLEGAL=$(git ls-files | grep -E '[<>:"|?*]|[ .]$' || true)
if [ -n "$ILLEGAL" ]; then
    echo "FAIL: tracked paths that are not legal filenames on Windows:"
    echo "$ILLEGAL" | sed 's/^/    /'
    echo "  git checkout fails on these, so the repository cannot be cloned there."
    exit 1
fi

# 3. One documentation directory. "doc" and "docs" side by side is how a
#    project looks unmaintained, and a reader cannot guess which is current.
if [ -d doc ] && [ -d docs ]; then
    echo "FAIL: both doc/ and docs/ exist -- consolidate them"; FAIL=1
fi

# 4. Nothing may reference a target or module that no longer exists. src/swift
#    imported RealmFFI for an entire phase after that module was deleted,
#    because nothing built it and so nothing failed.
for dead in RealmFFI RealmFFIStatic; do
    # Exclude this script (it names the symbols it searches for) and docs,
    # which record the history deliberately. Without the self-exclusion the
    # check passes while untracked and fails the moment it is committed.
    HIT=$(git ls-files | xargs grep -l "$dead" 2>/dev/null \
          | grep -v '^docs/' | grep -v '^tools/check-repo-hygiene.sh$' \
          | grep -v '^CHANGELOG' || true)
    if [ -n "$HIT" ]; then
        echo "FAIL: '$dead' was deleted but is still referenced by:"; echo "$HIT"; FAIL=1
    fi
done

# 5. Private keys do not belong in the repository root. The ones inherited here
#    were unused test keys, but a .pem in the root reads as a leak.
ROOTPEM=$(git ls-files --full-name | grep -E '^[^/]+\.pem$' || true)
if [ -n "$ROOTPEM" ]; then
    echo "FAIL: certificate or key files in the repository root:"; echo "$ROOTPEM"
    echo "  test fixtures belong beside the tests that use them"; FAIL=1
fi

# 6. Every findings document is listed in docs/findings/README.md, and every row
#    there points at a file that exists. The index is the only way anyone finds
#    these documents; one written and not indexed is one nobody reads, which
#    makes writing it a waste rather than a record.
#
#    Both directions matter. A missing row hides a document. A dangling row
#    sends a reader to a 404 and, worse, reads as evidence that a question was
#    settled somewhere.
#
#    Added after a document was very nearly committed unindexed. All
#    forty-three present at the time were listed, so this check codifies a
#    property the repository already had rather than repairing one it lacked --
#    which is the cheapest moment to add a check and the easiest to skip.
INDEX=docs/findings/README.md
if [ -f "$INDEX" ]; then
    for f in docs/findings/*.md; do
        b=$(basename "$f")
        [ "$b" = "README.md" ] && continue
        grep -q "($b)" "$INDEX" || {
            echo "FAIL: docs/findings/$b is not listed in $INDEX"; FAIL=1
        }
    done
    grep -oE '\]\([0-9a-z][a-z0-9-]*\.md\)' "$INDEX" \
        | sed -e 's|^](||' -e 's|)$||' | sort -u \
        | while read -r n; do
            [ -f "docs/findings/$n" ] || echo "DANGLING:$n"
        done > /tmp/.findings-dangling.$$
    if [ -s /tmp/.findings-dangling.$$ ]; then
        echo "FAIL: $INDEX links to findings that do not exist:"
        sed 's|^DANGLING:|  |' /tmp/.findings-dangling.$$
        FAIL=1
    fi
    rm -f /tmp/.findings-dangling.$$
fi

# 7. Every check and test script is invoked by a workflow. The workflow names
#    them one per line rather than globbing, because the pre-configure step has
#    no install tree to hand the two checks that need one -- so a script added to
#    tools/ and not to the workflow runs nowhere, which is this project's most
#    repeated finding in a new place.
#
#    docs/RELEASING.md solved the same problem with a glob and recorded why: a
#    gate that enumerates its own members drifts behind them. This is that gate's
#    enumeration, so it gets a check instead.
for c in tools/check-*.sh tools/test-*.sh; do
    b=$(basename "$c")
    grep -rq -- "$b" .github/workflows/ 2>/dev/null || {
        echo "FAIL: tools/$b is invoked by no workflow"
        echo "  add it to .github/workflows/build.yml, or delete it"
        FAIL=1
    }
done

[ "$FAIL" -eq 0 ] && echo "PASS: repository surface is clean"
exit "$FAIL"
