#!/usr/bin/env bash
# End-to-end verification that Tessera is consumable as an installed package.
#
# Every other gate in this project checks that the tree builds and its tests
# pass. None of them check the thing an external user actually does:
# find_package, link, include, run. A package can compile perfectly in-tree and
# still be unusable -- during Phase 0b the CMake package was briefly installed
# into share/cmake/Realm/, where find_package(Tessera) would never look.
#
# Usage: tools/verify/consumer-smoke-test.sh [build-dir]
set -euo pipefail
BUILD="${1:-build.debug}"
PREFIX=$(mktemp -d)/tessera-install
WORK=$(mktemp -d)/consumer
trap 'rm -rf "$(dirname "$PREFIX")" "$(dirname "$WORK")"' EXIT

echo "installing to $PREFIX"
cmake --install "$BUILD" --prefix "$PREFIX" > /dev/null

test -f "$PREFIX/share/cmake/Tessera/TesseraConfig.cmake" \
  || { echo "FAIL: TesseraConfig.cmake not where find_package looks"; exit 1; }

# The exported target set is the package's advertised surface, and the README
# names it. They drifted once already: the README promised "a self-hostable sync
# server, included" while the package exported no server target, installed no
# server header and shipped no server executable. Building the tree does not
# reveal that, because the tests link the in-tree target directly.
#
# Listing the targets here rather than deriving them is the point. A derived
# list agrees with whatever the package happens to export, which is exactly the
# thing under test.
EXPECTED_TARGETS="Merge ObjectStore QueryParser Storage Sync"
ACTUAL_TARGETS=$(grep -ohE 'add_library\(Tessera::[A-Za-z0-9_]+' \
    "$PREFIX"/share/cmake/Tessera/*.cmake \
  | sed 's/.*Tessera:://' | sort -u | tr '\n' ' ' | sed 's/ $//')
if [ "$ACTUAL_TARGETS" != "$EXPECTED_TARGETS" ]; then
    echo "FAIL: exported targets changed"
    echo "  expected: $EXPECTED_TARGETS"
    echo "  actual:   $ACTUAL_TARGETS"
    echo "If this is intended, update EXPECTED_TARGETS here and the Self-hosting"
    echo "section of README.md, which names the same list."
    exit 1
fi

mkdir -p "$WORK"
cat > "$WORK/CMakeLists.txt" <<'CM'
cmake_minimum_required(VERSION 3.25)
project(TesseraConsumer CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(Tessera REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE Tessera::Storage)
add_executable(readme_example readme_example.cpp)
target_link_libraries(readme_example PRIVATE Tessera::Storage)
add_executable(tiers tiers.cpp)
target_link_libraries(tiers PRIVATE Tessera::Storage Tessera::ObjectStore)
CM

# README: "Anything not reachable from these two headers carries no stability
# promise." That makes them the public API, so they must be installed and each
# must be self-contained -- includable first, with nothing included before it.
cat > "$WORK/tiers.cpp" <<'CPP'
#include <tessera/api.hpp>
#include <tessera/engine.hpp>
int main() { return 0; }
CPP

cat > "$WORK/main.cpp" <<'CPP'
#include <tessera/db.hpp>
#include <tessera/history.hpp>
#include <tessera/table.hpp>
#include <tessera/transaction.hpp>
#include <cstdio>
int main(int, char** argv)
{
    auto db = tessera::DB::create(tessera::make_in_realm_history(), argv[1]);
    {
        auto wt = db->start_write();
        auto t = wt->add_table("Thing");
        auto c = t->add_column(tessera::type_Int, "value");
        t->create_object().set(c, 42);
        wt->commit();
    }
    auto rt = db->start_read();
    auto t = rt->get_table("Thing");
    std::printf("%zu %lld\n", t->size(), (long long)t->get_object(0).get<int64_t>("value"));
    return 0;
}
CPP

cp "$(dirname "$0")/readme-example.cpp" "$WORK/readme_example.cpp"
cmake -S "$WORK" -B "$WORK/build" -DCMAKE_PREFIX_PATH="$PREFIX" > /dev/null
cmake --build "$WORK/build" -j"$(getconf _NPROCESSORS_ONLN)" > /dev/null

DBFILE="$WORK/smoke.tess"
OUT=$("$WORK/build/consumer" "$DBFILE")
[ "$OUT" = "1 42" ] || { echo "FAIL: consumer produced '$OUT', expected '1 42'"; exit 1; }

# The on-disk identity: mnemonic at offset 16 must be TESS.
MAGIC=$(dd if="$DBFILE" bs=1 skip=16 count=4 2>/dev/null)
[ "$MAGIC" = "TESS" ] || { echo "FAIL: file magic is '$MAGIC', expected 'TESS'"; exit 1; }

# The README's example is a claim about the API. Compile and run it verbatim,
# as its own target -- overwriting main.cpp and rebuilding is unreliable, because
# two writes in the same second defeat CMake's timestamp check and silently rerun
# the previous binary.
README_OUT=$("$WORK/build/readme_example" "$WORK/readme.tess" 2>&1 || true)
case "$README_OUT" in
  *"found 1"*) ;;
  *) echo "FAIL: the README example no longer works: $README_OUT"; exit 1 ;;
esac

echo "PASS: installed package is consumable, exports $EXPECTED_TARGETS,"
echo "      api.hpp and engine.hpp are self-contained, file magic is TESS,"
echo "      and the README example works"
