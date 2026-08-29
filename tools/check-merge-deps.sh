#!/usr/bin/env bash
# tessera-merge must depend only on the storage engine.
#
# The merge engine is a production-tested operational-transform implementation
# with ~6,000 lines of dedicated tests. Its value as a standalone component is
# that it can be consumed WITHOUT a protocol, a transport, or a session layer.
# If it ever reaches back into sync/ or object-store/, that value is silently
# gone and the carve-out has regressed.
#
# This is enforced rather than intended because good intentions do not survive
# month four of a refactor.
set -euo pipefail

BAD=$(grep -rIn '#include <tessera/\(sync\|object-store\)/' \
        --include='*.cpp' --include='*.hpp' src/tessera/merge 2>/dev/null || true)

if [ -n "$BAD" ]; then
    echo "FAIL: tessera-merge reaches outside its layer:"
    echo "$BAD"
    exit 1
fi
echo "PASS: tessera-merge depends only on storage"
