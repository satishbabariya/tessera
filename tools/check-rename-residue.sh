#!/usr/bin/env bash
# Fails if pre-rename identifiers survive outside the permitted exclusions.
#
# Two scans, because a rename leaves two different kinds of residue and only the
# first is visible to a compiler.
set -euo pipefail
FAIL=0

SOURCES=(--include='*.cpp' --include='*.hpp' --include='*.h'
         --include='*.mm' --include='*.c'
         --include='CMakeLists.txt' --include='*.cmake')

scan() {
  grep -rIn "$1" src test tools "${SOURCES[@]}" 2>/dev/null \
    | grep -v '^src/external/' \
    | grep -v '^test/external/' \
    | grep -v 'Copyright' \
    | grep -v 'Realm Inc' \
    || true
}

# ---------------------------------------------------------------------------
# 1. Code identifiers. A survivor here does not compile, so this scan mostly
#    guards against reintroduction.
# ---------------------------------------------------------------------------
for pat in 'namespace realm' 'realm::' 'REALM_' '#include <realm/'; do
    HITS=$(scan "$pat")
    if [ -n "$HITS" ]; then
        echo "FAIL: residual '$pat': $(echo "$HITS" | wc -l | tr -d ' ') occurrences"
        FAIL=1
    fi
done

# ---------------------------------------------------------------------------
# 2. Identity-bearing string literals. These compile perfectly and every test
#    passes, because the tests are built from the same tree that emits them.
#    They are what the project calls itself when it speaks to something else:
#    an error category name in a message, a subprotocol echoed to a client, an
#    HTTP Server header, a coordination file next to the database, a usage
#    line telling a user to run a binary that is not installed.
#
#    This scan is what found "realm.io" being sent as the negotiated WebSocket
#    subprotocol, and TESSERA_PRODUCT_NAME defined as "realm-core", eight
#    months after the rename was declared complete.
#
#    It looks only for the patterns that name the product outward. Prose that
#    happens to contain the word (\"cannot change primary key when realm is
#    synchronized\") is not matched: the public class is still called Realm,
#    and renaming it is a separate, larger decision.
# ---------------------------------------------------------------------------
IDENTITY_PATTERNS=(
    '"realm[-_.]'        # "realm.basic_system", "realm_XXXXXX", "realm-trawler"
    '"[Rr]ealm[Ss]ync/'  # the HTTP Server header and tool version banners
    '"realm-core'        # product name
    '%1realm_'           # coordination filenames built with util::format
    'Local\\\\realm_'    # Windows named objects
    'github_realm_realm' # the symbol planted in crash backtraces
    'realmfile'          # usage placeholders: the extension is .tess
    '<realm file>'
    '<realm-file-name>'
)

# Known and deliberate. Each entry names something that must keep the old
# spelling, with the reason:
#
#   realm:// realms://       The sync connection URL scheme. Renaming it is a
#                            public configuration change, and the obvious
#                            spelling (tesseras://) is poor; it is settled with
#                            the rest of the protocol in Phase 1.
#   mongodb-realm/          An App Services directory layout Tessera reads but
#   realm-object-server      does not create. These name a foreign thing, so
#                            they are correct as they stand.
ALLOWED='"realm://"|"realms://"|"realm:"|"realms:"|mongodb-realm/|realm-object-server'

for pat in "${IDENTITY_PATTERNS[@]}"; do
    HITS=$(scan "$pat" | grep -Ev "$ALLOWED" || true)
    if [ -n "$HITS" ]; then
        echo "FAIL: identity string residue matching '$pat':"
        echo "$HITS" | sed 's/^/    /'
        FAIL=1
    fi
done

if [ "$FAIL" -eq 0 ]; then
    echo "PASS: no residual pre-rename identifiers or identity strings"
fi
exit "$FAIL"
