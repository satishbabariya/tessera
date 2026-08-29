#!/usr/bin/env bash
# Fails if pre-rename identifiers survive outside the permitted exclusions.
set -euo pipefail
FAIL=0
scan() {
  grep -rIn "$1" src test tools --include='*.cpp' --include='*.hpp' --include='*.h' \
       --include='*.mm' --include='*.c' --include='CMakeLists.txt' --include='*.cmake' 2>/dev/null \
    | grep -v '^src/external/' \
    | grep -v '^test/external/' \
    | grep -v 'Copyright' \
    | grep -v 'Realm Inc' \
    || true
}
for pat in 'namespace realm' 'realm::' 'REALM_' '#include <realm/'; do
    HITS=$(scan "$pat")
    if [ -n "$HITS" ]; then
        echo "FAIL: residual '$pat': $(echo "$HITS" | wc -l | tr -d ' ') occurrences"
        FAIL=1
    fi
done
[ "$FAIL" -eq 0 ] && echo "PASS: no residual pre-rename identifiers"
exit "$FAIL"
