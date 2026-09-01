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
EXPECTED_TARGETS="Merge ObjectStore QueryParser Storage Sync SyncServer"
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
add_executable(server_consumer server_consumer.cpp)
target_link_libraries(server_consumer PRIVATE Tessera::SyncServer)
CM

# README: "Anything not reachable from these two headers carries no stability
# promise." That makes them the public API, so they must be installed and each
# must be self-contained -- includable first, with nothing included before it.
cat > "$WORK/tiers.cpp" <<'CPP'
#include <tessera/api.hpp>
#include <tessera/engine.hpp>
int main() { return 0; }
CPP

# The point of the whole exercise: a self-hoster can find_package, include the
# server header and construct a server, without a build tree. Until this target
# existed the package exported no server at all, and the README said so.
#
# It constructs but does not start: starting binds a port, and a merge gate that
# depends on a free port fails for reasons unrelated to the change. Construction
# is what proves the three installed headers are self-contained and the library
# links.
cat > "$WORK/server_consumer.cpp" <<'CPP'
#include <tessera/sync/server/server.hpp>
#include <tessera/util/logger.hpp>
#include <cstdio>
int main(int, char** argv)
{
    tessera::sync::Server::Config config;
    config.logger = tessera::util::Logger::get_default_logger();
    config.listen_address = "127.0.0.1";
    config.listen_port = "0";
    tessera::sync::Server server(argv[1], tessera::util::none, config);
    std::printf("server constructed\n");
    return 0;
}
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

SRVDIR="$WORK/server-root"
mkdir -p "$SRVDIR"
SRV_OUT=$("$WORK/build/server_consumer" "$SRVDIR" 2>&1 || true)
case "$SRV_OUT" in
  *"server constructed"*) ;;
  *) echo "FAIL: the installed server could not be constructed: $SRV_OUT"; exit 1 ;;
esac

# The installed server binary. A library you link is not a server you run, and
# the package shipped five inspector tools and no server until this existed.
#
# What is asserted is the refusal, not the start-up: a server given no public key
# verifies no signature, demands no token and applies no permissions, and a
# binary that entered that mode silently on a port would reintroduce at the
# command line exactly what #29-#35 removed from the server. Starting it here
# would also bind a port, and a merge gate that fails when a port is busy trains
# everyone to ignore it.
SERVER_BIN=$(find "$PREFIX/bin" -maxdepth 1 -name 'tessera-sync-server*' -type f | head -1)
[ -n "$SERVER_BIN" ] || { echo "FAIL: no tessera-sync-server in the installed package"; exit 1; }

set +e
"$SERVER_BIN" --root "$WORK/refuse" > "$WORK/refuse.log" 2>&1
REFUSE_STATUS=$?
set -e
[ "$REFUSE_STATUS" -eq 2 ] || {
    echo "FAIL: the server started without a public key (exit $REFUSE_STATUS)"
    cat "$WORK/refuse.log"
    exit 1
}
grep -q "no --public-key given" "$WORK/refuse.log" || {
    echo "FAIL: the server refused without saying why"; cat "$WORK/refuse.log"; exit 1
}

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
echo "      the README example works,"
echo "      and the installed server refuses to run unauthenticated"
