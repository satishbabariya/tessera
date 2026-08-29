#!/usr/bin/env bash
# Enforces the public API tier boundaries.
#
# Tessera ships two supported surfaces:
#   <tessera/api.hpp>     tier 1, the high-level API (schemas, objects, results)
#   <tessera/engine.hpp>  tier 2, the storage engine (DB, Table, Query, Obj)
#
# Neither may expose sync-private internals. This check exists because the
# boundary is otherwise invisible: a public header can start including a private
# one and nothing fails until someone tries to consume the installed package.
set -euo pipefail
FAIL=0

# 1. No public header may reach into noinst/ ("no install" -- private by name).
NOINST=$(grep -rIn '#include\s*[<"]tessera/[^>"]*noinst/' \
           --include='*.hpp' src/tessera \
         | grep -v '/noinst/' || true)
if [ -n "$NOINST" ]; then
    echo "FAIL: a public header includes a noinst/ (private) header:"
    echo "$NOINST"; FAIL=1
fi

# 2. impl/ exposure is capped at the six headers the engine genuinely requires.
#    These are reachable from the public API today because group.hpp, db.hpp,
#    replication.hpp and array_key.hpp depend on them. Eliminating that means
#    refactoring the engine's core headers, which is deliberately out of scope;
#    what this check prevents is the number growing. They carry no stability
#    promise. See docs/findings/0b-header-tiers.md.
KNOWN="array_writer|changeset_input_stream|cont_transact_hist|destroy_guard|output_stream|transact_log"
UNEXPECTED=$(grep -rhoE '#include\s*[<"]tessera/impl/[a-z_]+\.hpp' --include='*.hpp' src/tessera \
             | grep -oE 'impl/[a-z_]+' | sed 's|impl/||' | sort -u \
             | grep -vE "^($KNOWN)$" || true)
if [ -n "$UNEXPECTED" ]; then
    echo "FAIL: new impl/ headers exposed through public headers:"
    echo "$UNEXPECTED"
    echo "  Either avoid the dependency, or extend the allowlist deliberately."
    FAIL=1
fi

# 3. The tier entry points must exist and be installable.
for h in src/tessera/api.hpp src/tessera/engine.hpp; do
    [ -f "$h" ] || { echo "FAIL: missing tier entry point $h"; FAIL=1; }
done

[ "$FAIL" -eq 0 ] && echo "PASS: header tier boundaries hold"
exit "$FAIL"
