#include <string>
#include <sstream>

#include <tessera/util/file.hpp>
#include <tessera/db.hpp>
#include <tessera/sync/history.hpp>
#include <tessera/sync/noinst/client_history_impl.hpp>
#include <tessera/sync/noinst/server/server_history.hpp>

#include "test.hpp"
#include "util/compare_groups.hpp"
#include "sync_fixtures.hpp"

using namespace tessera;
using namespace tessera::test_util;


// Test independence and thread-safety
// -----------------------------------
//
// All tests must be thread safe and independent of each other. This
// is required because it allows for both shuffling of the execution
// order and for parallelized testing.
//
// In particular, avoid using std::rand() since it is not guaranteed
// to be thread safe. Instead use the API offered in
// `test/util/random.hpp`.
//
// All files created in tests must use the TEST_PATH macro (or one of
// its friends) to obtain a suitable file system path. See
// `test/util/test_path.hpp`.
//
//
// Debugging and the ONLY() macro
// ------------------------------
//
// A simple way of disabling all tests except one called `Foo`, is to
// replace TEST(Foo) with ONLY(Foo) and then recompile and rerun the
// test suite. Note that you can also use filtering by setting the
// environment variable `UNITTEST_FILTER`. See `README.md` for more on
// this.
//
// Another way to debug a particular test, is to copy that test into
// `experiments/testcase.cpp` and then run `sh build.sh
// check-testcase` (or one of its friends) from the command line.


namespace {

#if !TESSERA_MOBILE

#endif // !TESSERA_MOBILE

TEST(Sync_HistoryCompression)
{
    SHARED_GROUP_TEST_PATH(path);
    DBRef db = DB::create(sync::make_client_replication(), path);

    {
        WriteTransaction wt(db);
        auto table = wt.get_group().add_table_with_primary_key("class_table", type_Int, "id");
        table->add_column(type_Binary, "data");
        wt.commit();
    }

    { // Create a changeset which should be highly compressible
        WriteTransaction wt(db);
        auto table = wt.get_table("class_table");
        auto data = std::make_unique<char[]>(100'000);
        table->create_object_with_primary_key(1).set("data", BinaryData{data.get(), 100'000});
        wt.commit();
    }

    // Inspect the history compartment directly to verify that compression happened
    ReadTransaction rt(db);
    using gf = _impl::GroupFriend;
    Allocator& alloc = gf::get_alloc(rt.get_group());
    auto ref = gf::get_history_ref(rt.get_group());

    Array history_root(alloc);
    history_root.init_from_ref(ref);

    BinaryColumn changesets(alloc);
    changesets.set_parent(&history_root, 13); // s_changesets_iip
    changesets.init_from_parent();

    // Both changesets should be small: the first because it's just creating the
    // schema, and the second because the 100k binary data is all zeroes and
    // can be compressed to <1% of its source size.
    CHECK_EQUAL(changesets.size(), 2);
    CHECK_LESS(changesets.get(0).size(), 256);
    CHECK_LESS(changesets.get(1).size(), 1024);
}
} // unnamed namespace
