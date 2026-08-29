#!/usr/bin/env bash
# Fails if a test source file is not referenced by the CMakeLists that should
# compile it.
#
# A test file left out of the build is invisible in every direction. It does not
# fail, because it does not run. It does not show as skipped, because the
# framework never learns it exists. It does not break the build, because nothing
# references it. And the suite's own total is no help: the count simply does not
# include it, and no one knows which number to expect.
#
# test/test_util_enum.cpp was in this state. Sixty-five lines, one test, covering
# tessera/util/enum.hpp -- a header this project installs as public API and
# compiles into the library. Adding it to the build required no other change: it
# compiled and passed first time. It had simply never been listed.
set -euo pipefail
cd "$(dirname "$0")/.."

status=0
checked=0

check_dir() {
    local dir="$1" cmakelists="$2"
    [ -f "$cmakelists" ] || return 0
    local file base
    for file in "$dir"/*.cpp; do
        [ -e "$file" ] || continue
        base=$(basename "$file")
        checked=$((checked + 1))
        if ! grep -q "$base" "$cmakelists"; then
            echo "FAIL: $file is not referenced in $cmakelists"
            echo "      It compiles into no target, so its tests never run."
            status=1
        fi
    done
}

check_dir test test/CMakeLists.txt
check_dir test/object-store test/object-store/CMakeLists.txt
check_dir test/object-store/sync test/object-store/CMakeLists.txt
check_dir test/object-store/util test/object-store/CMakeLists.txt

if [ "$checked" -eq 0 ]; then
    echo "FAIL: no test sources found; this check would pass over an empty set"
    exit 1
fi

[ "$status" -eq 0 ] && echo "PASS: all $checked test sources are referenced by a CMakeLists"
exit "$status"
