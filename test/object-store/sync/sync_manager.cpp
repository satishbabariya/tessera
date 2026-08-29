////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#include <util/event_loop.hpp>
#include <util/test_path.hpp>
#include <util/test_utils.hpp>
#include <util/sync/session_util.hpp>
#include <util/sync/sync_test_utils.hpp>

#include <tessera/object-store/property.hpp>
#include <tessera/object-store/sync/sync_manager.hpp>
#include <tessera/object-store/sync/sync_user.hpp>

#include <tessera/sync/config.hpp>

#include <tessera/util/logger.hpp>
#include <tessera/util/optional.hpp>
#include <tessera/util/scope_exit.hpp>

using namespace tessera;
using namespace tessera::util;
using File = tessera::util::File;

static const auto base_path =
    fs::path{util::make_temp_dir()}.make_preferred() / "realm_objectstore_sync_manager.test-dir";
static const std::string dummy_device_id = "123400000000000000000000";


TEST_CASE("SyncManager: set_session_multiplexing", "[sync][sync manager]") {
    TestSyncManager::Config tsm_config;
    tsm_config.start_sync_client = false;
    TestSyncManager tsm(tsm_config);
    bool sync_multiplexing_allowed = GENERATE(true, false);
    auto sync_manager = tsm.sync_manager();
    sync_manager->set_session_multiplexing(sync_multiplexing_allowed);

    auto user_1 = tsm.fake_user("user-name-1");
    auto user_2 = tsm.fake_user("user-name-2");

    SyncTestFile file_1(user_1, "partition1", util::none);
    SyncTestFile file_2(user_1, "partition2", util::none);
    SyncTestFile file_3(user_2, "partition3", util::none);

    auto realm_1 = Realm::get_shared_realm(file_1);
    auto realm_2 = Realm::get_shared_realm(file_2);
    auto realm_3 = Realm::get_shared_realm(file_3);

    wait_for_download(*realm_1);
    wait_for_download(*realm_2);
    wait_for_download(*realm_3);

    if (sync_multiplexing_allowed) {
        REQUIRE(conn_id_for_realm(realm_1) == conn_id_for_realm(realm_2));
        REQUIRE(conn_id_for_realm(realm_2) != conn_id_for_realm(realm_3));
    }
    else {
        REQUIRE(conn_id_for_realm(realm_1) != conn_id_for_realm(realm_2));
        REQUIRE(conn_id_for_realm(realm_2) != conn_id_for_realm(realm_3));
        REQUIRE(conn_id_for_realm(realm_1) != conn_id_for_realm(realm_3));
    }
}

TEST_CASE("SyncManager: has_existing_sessions", "[sync][sync manager][active sessions]") {
    TestSyncManager tsm({}, {false});
    auto sync_manager = tsm.sync_manager();

    SECTION("no active sessions") {
        REQUIRE(!sync_manager->has_existing_sessions());
    }

    auto schema = Schema{
        {"object",
         {
             {"_id", PropertyType::Int, Property::IsPrimary{true}},
             {"value", PropertyType::Int},
         }},
    };

    std::atomic<bool> error_handler_invoked(false);
    Realm::Config config;
    auto user = tsm.fake_user("user-name");
    auto create_session = [&](SyncSessionStopPolicy stop_policy) {
        std::shared_ptr<SyncSession> session = sync_session(
            user, "/test-dying-state",
            [&](auto, auto) {
                error_handler_invoked = true;
            },
            stop_policy, nullptr, schema, &config);
        EventLoop::main().run_until([&] {
            return sessions_are_active(*session);
        });
        return session;
    };

    SECTION("active sessions") {
        {
            auto session = create_session(SyncSessionStopPolicy::Immediately);
            REQUIRE(sync_manager->has_existing_sessions());
            session->close();
        }
        EventLoop::main().run_until([&] {
            return !sync_manager->has_existing_sessions();
        });
        REQUIRE(!sync_manager->has_existing_sessions());
    }
}
