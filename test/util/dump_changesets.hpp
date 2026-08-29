#ifndef TESSERA_TEST_UTIL_DUMP_CHANGESETS_HPP
#define TESSERA_TEST_UTIL_DUMP_CHANGESETS_HPP

#include <memory>

#include "test_path.hpp"

namespace tessera {
namespace test_util {
namespace unit_test {

class TestContext;

} // namespace unit_test

std::unique_ptr<TestDirNameGenerator>
get_changeset_dump_dir_generator(const unit_test::TestContext& test_context,
                                 const char* env_var = "UNITTEST_DUMP_TRANSFORM");

} // namespace test_util
} // namespace tessera

#endif // TESSERA_TEST_UTIL_DUMP_CHANGESETS_HPP
