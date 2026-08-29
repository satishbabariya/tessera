#!/usr/bin/env bash
# Enforces the layering invariants established in Phase 0a.
#
# Phase 0a can only enforce what exists before the rename; Phase 0b extends this
# with the public/internal header-tier rules from the design spec.
set -euo pipefail
FAIL=0

# 1. Deleted subsystems must not reappear. Each entry cost real work to remove;
#    a stray reintroduction should fail loudly rather than be noticed months on.
for dead in "object-store/c_api" "bindgen/" "sync/mongo_client" "sync/push_client" \
            "sync/app.hpp" "sync/app_config.hpp" "impl/app_metadata" \
            "baas_admin_api" "AcquireRealmDependency" "linux.toolchain.base"; do
    if git ls-files | grep -q -- "$dead"; then
        echo "FAIL: deleted subsystem has reappeared: $dead"
        FAIL=1
    fi
done

# 2. The storage layer must not include upward from sync or object-store.
#    Storage is the bottom of the stack; anything above it is a layering break.
UP=$(grep -rIn '#include <realm/\(sync\|object-store\)/' \
       --include='*.cpp' --include='*.hpp' src/realm \
     | grep -v '^src/realm/sync/' \
     | grep -v '^src/realm/object-store/' \
     | grep -v '^src/realm/exec/' || true)
if [ -n "$UP" ]; then
    echo "FAIL: the storage layer includes upward from a higher layer:"
    echo "$UP"
    FAIL=1
fi

# 3. The sync layer must not depend on object-store. Sync sits below the
#    high-level API. This one caught a real violation in Phase 0a:
#    pending_bootstrap_store.hpp included object-store/c_api/util.hpp.
SYNC_UP=$(grep -rIn '#include <realm/object-store/' \
            --include='*.cpp' --include='*.hpp' src/realm/sync || true)
if [ -n "$SYNC_UP" ]; then
    echo "FAIL: the sync layer depends on object-store:"
    echo "$SYNC_UP"
    FAIL=1
fi

if [ "$FAIL" -eq 0 ]; then
    echo "PASS: layering invariants hold"
fi
exit "$FAIL"
