#!/usr/bin/env bash
# Prints the path to a test executable, which differs by platform: on macOS the
# targets are built as .app bundles, elsewhere they are plain executables.
#
# Usage: tools/verify/test-binary.sh <build-dir> <name>
#   e.g. tools/verify/test-binary.sh build tessera-tests
set -euo pipefail
BUILD="$1"; NAME="$2"

for candidate in \
    "$BUILD/test/$NAME" \
    "$BUILD/test/$NAME.app/Contents/MacOS/$NAME" \
    "$BUILD/test/object-store/$NAME" \
    "$BUILD/test/object-store/$NAME.app/Contents/MacOS/$NAME"; do
    if [ -x "$candidate" ] && [ -f "$candidate" ]; then
        echo "$candidate"
        exit 0
    fi
done

echo "error: no executable named '$NAME' under '$BUILD'" >&2
echo "  looked in test/ and test/object-store/, as both a plain binary and an .app bundle" >&2
exit 1
