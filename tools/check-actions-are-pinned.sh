#!/usr/bin/env bash
# Fails if a workflow references a GitHub Action by anything other than a full
# commit SHA.
#
# A tag is a mutable pointer. `actions/checkout@v4` means "whatever v4 points at
# when this job runs", and whoever controls the repository can move it -- so a
# tag reference is a standing grant to run future code. For a third-party action
# it is a standing grant to a stranger.
#
# This project pins its dependency tarballs by digest and measured each one
# against the publisher's own checksum rather than copying it. Its workflows did
# not hold to that: check-changelog.yml pinned both of its actions by SHA, while
# build.yml and nightly.yml used @v4, and nightly ran a third-party
# setup-emsdk@v14. So the practice existed in the repository and was applied in
# one file out of three.
#
# The version stays in a trailing comment, because a SHA says nothing to a
# reader and an unreadable pin is one nobody dares update.
set -euo pipefail
export LC_ALL=C

cd "$(dirname "$0")/.."
[ -d .github/workflows ] || { echo "SKIP: no .github/workflows"; exit 0; }

# A `uses:` value is either owner/repo@ref or a local path (./...) or a docker://
# image. Only the first kind is pinnable this way.
offenders=$(grep -rn "^[[:space:]]*-\{0,1\}[[:space:]]*uses:" .github/workflows/ \
  | grep -v "uses:[[:space:]]*\./" \
  | grep -v "uses:[[:space:]]*docker://" \
  | grep -vE "uses:[[:space:]]*[^@]+@[0-9a-f]{40}([[:space:]]|$)" \
  || true)

if [ -n "$offenders" ]; then
    echo "FAIL: these actions are not pinned to a commit SHA:"
    printf '%s\n' "$offenders" | sed 's|^|    |'
    echo
    echo "    A tag is mutable: whoever controls the action's repository can move it,"
    echo "    so a tag reference is a standing grant to run code that does not exist"
    echo "    yet. Resolve the tag and pin the SHA, keeping the version in a comment:"
    echo
    echo "      gh api repos/OWNER/REPO/git/ref/tags/TAG --jq .object.sha"
    echo "      uses: OWNER/REPO@<40-hex sha> # TAG"
    exit 1
fi

n=$(grep -rc "uses:" .github/workflows/ | awk -F: '{t+=$2} END {print t}')
echo "PASS: all $n action references are pinned to a commit SHA"
