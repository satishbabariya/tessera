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

mkdir -p "$WORK"
cat > "$WORK/CMakeLists.txt" <<'CM'
cmake_minimum_required(VERSION 3.25)
project(TesseraConsumer CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(Tessera REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE Tessera::Storage)
CM

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

cmake -S "$WORK" -B "$WORK/build" -DCMAKE_PREFIX_PATH="$PREFIX" > /dev/null
cmake --build "$WORK/build" -j4 > /dev/null

DBFILE="$WORK/smoke.tess"
OUT=$("$WORK/build/consumer" "$DBFILE")
[ "$OUT" = "1 42" ] || { echo "FAIL: consumer produced '$OUT', expected '1 42'"; exit 1; }

# The on-disk identity: mnemonic at offset 16 must be TESS.
MAGIC=$(dd if="$DBFILE" bs=1 skip=16 count=4 2>/dev/null)
[ "$MAGIC" = "TESS" ] || { echo "FAIL: file magic is '$MAGIC', expected 'TESS'"; exit 1; }

# The README's example is a claim about the API. Compile and run it verbatim.
cp "$(dirname "$0")/readme-example.cpp" "$WORK/main.cpp"
cmake --build "$WORK/build" -j4 > /dev/null
README_OUT=$("$WORK/build/consumer" "$WORK/readme.tess" 2>&1 || true)
case "$README_OUT" in
  *"found 1"*) ;;
  *) echo "FAIL: the README example no longer works: $README_OUT"; exit 1 ;;
esac

echo "PASS: installed package is consumable, file magic is TESS, README example works"
