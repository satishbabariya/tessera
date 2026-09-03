#!/usr/bin/env bash

set -e -x

readonly build_dir="$PWD"
readonly dmg_file="$build_dir/exfat.dmg"

# Locates a test binary under the build directory. Handles the macOS .app
# wrapper the suites are built into, the bare executable elsewhere, the
# Release/Debug subdirectories multi-config generators use, and the -dbg suffix.
#
# This replaced three hardcoded paths naming the pre-rename core-test binary,
# which has not existed since the rename. All three checks failed, so it exited
# with "Run this script from the build directory after building tests" no matter
# where it was run from or what had been built -- advice that could not be
# followed. Nothing in CI runs this script, so nothing reported it.
#
# tools/check-rename-residue.sh did not scan shell scripts. It does now.
find_test_binary() {
    local name="$1" dir suffix cand
    for dir in "$build_dir/test" "$build_dir/test/Release" "$build_dir/test/Debug"; do
        for suffix in "" "-dbg"; do
            cand="$dir/${name}${suffix}.app/Contents/MacOS/${name}${suffix}"
            [ -x "$cand" ] && { printf '%s\n' "$cand"; return 0; }
            cand="$dir/${name}${suffix}"
            [ -x "$cand" ] && [ -f "$cand" ] && { printf '%s\n' "$cand"; return 0; }
        done
    done
    return 1
}

core_tests=$(find_test_binary tessera-tests) || {
    echo "Could not find tessera-tests under $build_dir/test."
    echo "Run this script from the build directory, after building the test suites."
    exit 1
}
sync_tests=$(find_test_binary tessera-sync-tests) || {
    echo "Could not find tessera-sync-tests under $build_dir/test."
    echo "Run this script from the build directory, after building the test suites."
    exit 1
}

echo "core: $core_tests"
echo "sync: $sync_tests"

function cleanup() {
    rm -f "$dmg_file"
    if [ -n "$device" ]; then
        hdiutil detach "$device"
    fi
}
trap cleanup EXIT

rm -f "$dmg_file"
hdiutil create -fs exFAT -size 400MB "$dmg_file"
hdiutil_out=$(hdiutil attach exfat.dmg)
device=$(echo "$hdiutil_out" | head -n1 | cut -f1 | awk '{$1=$1};1')
path=$(echo "$hdiutil_out" | tail -n1 | cut -f3)

UNITTEST_ENABLE_SYNC_TO_DISK=1 "$core_tests" "$path/"
# one test runner because several sync tests make large uploads which if run together may exceed our 400MB space limit
UNITTEST_THREADS=1 UNITTEST_ENABLE_SYNC_TO_DISK=1 "$sync_tests" "$path/"
echo "finished running tests"

