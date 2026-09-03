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

echo "PASS: only $ALLOWED installs to the include root"
