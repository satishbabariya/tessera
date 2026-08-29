#ifndef TESSERA_TEST_UTIL_COMPARE_GROUPS_HPP
#define TESSERA_TEST_UTIL_COMPARE_GROUPS_HPP

#include <tessera/util/functional.hpp>
#include <tessera/util/logger.hpp>
#include <tessera/transaction.hpp>
#include <tessera/table.hpp>

namespace tessera::test_util {

bool compare_tables(const Table& table_1, const Table& table_2, util::Logger&);

bool compare_tables(const Table& table_1, const Table& table_2);

bool compare_groups(const Transaction& group_1, const Transaction& group_2);

bool compare_groups(const Transaction& group_1, const Transaction& group_2, util::Logger&);

bool compare_groups(const Transaction& group_1, const Transaction& group_2,
                    util::FunctionRef<bool(StringData table_name)> filter_func, util::Logger&);


// Implementation

inline bool compare_groups(const Transaction& group_1, const Transaction& group_2, util::Logger& logger)
{
    return compare_groups(
        group_1, group_2,
        [](StringData) {
            return true;
        },
        logger);
}

} // namespace tessera::test_util

#endif // TESSERA_TEST_UTIL_COMPARE_GROUPS_HPP
