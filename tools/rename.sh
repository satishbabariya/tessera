#!/usr/bin/env bash
# Idempotent rename: realm -> tessera.
#
# Committed and complete: re-running this on the upstream tree reproduces the
# rename, so a ~700-file diff is reviewed by reading this script rather than
# the diff. Every pattern below was found by a failing gate, not by inspection.
#
# NOT renamed:
#   any line matching Copyright or "Realm Inc"
#       Apache 2.0 section 4(b) requires retaining copyright notices in
#       derivative works. Stripping the attribution would breach the licence.
#       perl's `next` in -p mode skips the substitutions but still prints the
#       line, which is exactly the required behaviour. Verified before and
#       after: the notice count must be unchanged. It was 563 when the
#       rename ran; later deletions have reduced it. What matters is that
#       the count is identical either side of a rename, not its value.
#
# RENAMED despite living under src/external:
#   src/external/s2 is NOT pristine upstream S2. It is a downstream fork that
#   calls tessera::util::format, routes CHECK/DCHECK to this project's assert
#   macros, and tests TESSERA_WATCHOS. Those symbols are ours, not S2's, so
#   they follow the rename. See src/external/s2/TESSERA-PATCHES.md.
#
# RENAMED despite being generated:
#   parser/generated/** -- Bison 2.3 on this host cannot regenerate the grammar
#   (3.x-only api.token.constructor, api.value.type variant, api.symbol.prefix),
#   and regeneration is unnecessary: six mechanical references. The .yy/.ll
#   sources are renamed identically, so no drift is introduced.
set -euo pipefail

# --- source, tests, tools, and the patched parts of external -----------------
{
  find src test tools -type f \
       \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.c' \
          -o -name '*.mm' -o -name '*.ll' -o -name '*.yy' \
          -o -name 'CMakeLists.txt' -o -name '*.cmake' -o -name '*.in' \) \
    | grep -v '^src/external/' | grep -v '^test/external/'
  # s2 carries first-party patches; see the header comment.
  grep -rl 'realm::\|REALM_\|include <realm/' src/external/s2 2>/dev/null || true
  # Root-level build files live under none of the directories above.
  echo CMakeLists.txt
} | sort -u | while read -r f; do
      [ -f "$f" ] || continue
      perl -i -pe '
        if (/Copyright|Realm Inc/) { next }

        # Namespaces and macros. \b does not help at an underscore, because
        # underscore is a word character -- hence the explicit prefixed forms.
        s/\bnamespace realm\b/namespace tessera/g;
        s/\brealm::/tessera::/g;
        s/\bRealm::/Tessera::/g;          # the CMake alias namespace
        s/\bREALM_/TESSERA_/g;
        s/\bSRC_REALM_/SRC_TESSERA_/g;    # include guards: SRC_REALM_FOO_HPP_
        s/_REALM_/_TESSERA_/g;            # e.g. _REALM_USE_OPENSSL_...

        # Includes, in every form used in this tree.
        s{#include <realm/}{#include <tessera/}g;
        s{#include "realm/}{#include "tessera/}g;
        s{#include <realm\.hpp>}{#include <tessera.hpp>}g;
        s{#include "realm\.hpp"}{#include "tessera.hpp"}g;
        s{<realm/}{<tessera/}g;
        s{"realm/}{"tessera/}g;

        # Build-system paths and the project name. project(RealmCore) sets
        # RealmCore_SOURCE_DIR, which has no word boundary before the underscore.
        s{src/realm\b}{src/tessera}g;
        s{include/realm\b}{include/tessera}g;
        s{add_subdirectory\(realm\)}{add_subdirectory(tessera)}g;
        s/\bRealmCore_/Tessera_/g;
        s/\bRealmCore\b/Tessera/g;
        s/(OUTPUT_NAME\s+"?)realm/${1}tessera/g;

        # Reverse-DNS identifiers are NAMES, not file extensions. Must run
        # before the .realm extension rule below, which would otherwise turn
        # io.realm.sync.keychain into io.tess.sync.keychain -- a silently
        # different macOS Keychain service.
        s/\bio\.realm\b/io.tessera/g;

        # The file extension. Deliberately anchored to quote/whitespace/path
        # context: a bare s/\.realm\b/.tess/g also matches the METHOD CALL
        # x.realm() and the member entry.realm, which broke 12 sites.
        s/\.realm(?=["\s\/,;)])/.tess/g;
        s/\.realm$/.tess/g;
      ' "$f"
    done

echo "rename applied"
