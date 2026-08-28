#!/usr/bin/env bash
# Fails if the build references infrastructure the project does not control.
#
# Phase 0a's hard invariant: Tessera must build from sources we or the
# upstream projects publish, never from a third party's prebuilt artifact
# cache. realm-core fetched prebuilt OpenSSL and cross-compile toolchains
# from static.realm.io, infrastructure MongoDB has no reason to maintain.
set -euo pipefail

# Require a URL scheme so that prose describing the history (in comments and
# docs) does not trip the guard -- only actual fetches count.
FORBIDDEN='https?://[^[:space:]"]*static\.realm\.io|https?://[^[:space:]"]*realm\.io/downloads'

# src/external is vendored third-party code; docs describe the history on
# purpose; .git is not source.
HITS=$(grep -rInE "$FORBIDDEN" \
        --include='*.cmake' --include='CMakeLists.txt' --include='*.yml' \
        --include='*.yaml' --include='*.sh' --include='*.txt' \
        . 2>/dev/null \
    | grep -v '^\./src/external/' \
    | grep -v '^\./docs/' \
    | grep -v '^\./\.git/' \
    | grep -vE '^\./build' \
    | grep -v '^\./tools/check-no-vendor-hosts\.sh:' || true)

if [ -n "$HITS" ]; then
    echo "FAIL: the build references a host the project does not control:"
    echo "$HITS"
    exit 1
fi

echo "PASS: no vendor-controlled hosts referenced by the build"
