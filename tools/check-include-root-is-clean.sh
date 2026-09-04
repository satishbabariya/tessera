#!/usr/bin/env bash
# Fails if anything but the umbrella header would install to the root of the
# consumer's include path.
#
# `install(FILES tessera.hpp tessera/engine.hpp DESTINATION include)` put a
# second, byte-identical copy of tessera/engine.hpp at the include root, as
# <engine.hpp>. Nothing referenced it -- the README names the entry point as
# <tessera/engine.hpp> and every test uses that -- so it was a duplicate
# occupying an extremely generic name in the include path of everyone who links
# Tessera. A collision with somebody else's engine.hpp is diagnosed from their
# side, where nothing points here.
#
# tessera.hpp is the exception, deliberately: it is the documented umbrella,
# retained because it predates the tier split, and it says so in its own
# docstring.
set -euo pipefail
cd "$(dirname "$0")/.."

ALLOWED="tessera.hpp"

# Every install(FILES ...) whose DESTINATION is exactly `include`.
offenders=$(awk '
    /install\(FILES/       { collecting = 1; files = "" }
    collecting             { files = files " " $0 }
    collecting && /\)/     {
        if (files ~ /DESTINATION[ \t]+include[ \t]*$/ || files ~ /DESTINATION[ \t]+include[ \t]+COMPONENT/) print files
        collecting = 0
    }
' src/CMakeLists.txt | tr ' ' '\n' \
  | grep -E '\.hpp$' | sed 's|.*/||' | sort -u | grep -v "^${ALLOWED}$" || true)

if [ -n "$offenders" ]; then
    echo "FAIL: these would install to the root of the include path:"
    while IFS= read -r f; do printf '    %s\n' "$f"; done <<< "$offenders"
    echo
    echo "    Only $ALLOWED belongs there. Everything else goes under include/tessera/,"
    echo "    so it cannot collide with a consumer's own headers."
    exit 1
fi

# The scan above reads declarations in src/CMakeLists.txt. That is not the
# include root -- it is one file's opinion of it, and it said "only tessera.hpp"
# while the installed tree had an `external/` directory at top level, holding a
# vendored nlohmann/json published from src/tessera/sync/CMakeLists.txt. A check
# that reads one CMakeLists cannot see an install rule in another.
#
# So when given an install prefix, the tree itself is checked. A consumer adds
# -I<prefix>/include, so every name at that level enters their namespace: a
# generic directory like `external/` collides with their own.
PREFIX="${1:-}"
if [ -n "$PREFIX" ] && [ -d "$PREFIX/include" ]; then
    ALLOWED_DIRS="tessera external"
    strays=""
    for entry in "$PREFIX/include"/*; do
        [ -e "$entry" ] || continue
        name=$(basename "$entry")
        if [ -d "$entry" ]; then
            case " $ALLOWED_DIRS " in *" $name "*) continue ;; esac
        else
            [ "$name" = "$ALLOWED" ] && continue
        fi
        strays="$strays $name"
    done
    if [ -n "$strays" ]; then
        echo "FAIL: unexpected names at the root of the installed include path:"
        for n in $strays; do printf '    %s\n' "$n"; done
        echo
        echo "    A consumer compiles with -I\$PREFIX/include, so each of these enters"
        echo "    their include namespace and can collide with their own headers."
        exit 1
    fi
    echo "PASS: the installed include root holds only $ALLOWED and $ALLOWED_DIRS"
    exit 0
fi

echo "PASS: only $ALLOWED installs to the include root (declarations only; pass an"
echo "      install prefix to check the tree)"
