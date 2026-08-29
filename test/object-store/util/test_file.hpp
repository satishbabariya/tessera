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

#ifndef TESSERA_TEST_UTIL_TEST_FILE_HPP
#define TESSERA_TEST_UTIL_TEST_FILE_HPP

#include <tessera/object-store/shared_db.hpp>
#include <tessera/util/logger.hpp>
#include <tessera/util/tagged_bool.hpp>

#if TESSERA_ENABLE_SYNC
#include "test_utils.hpp"

#include <tessera/object-store/sync/sync_manager.hpp>
#include <tessera/sync/client.hpp>
#include <tessera/sync/config.hpp>
#include <tessera/sync/noinst/server/server.hpp>
#endif // TESSERA_ENABLE_SYNC

#include <thread>

#ifndef TEST_TIMEOUT_EXTRA
#define TEST_TIMEOUT_EXTRA 0
#endif

namespace tessera {
class Schema;
enum class SyncSessionStopPolicy;
struct DBOptions;
struct SyncConfig;
} // namespace tessera

class JoiningThread {
public:
    template <typename... Args>
    JoiningThread(Args&&... args)
        : m_thread(std::forward<Args>(args)...)
    {
    }
    ~JoiningThread()
    {
        if (m_thread.joinable())
            m_thread.join();
    }
    void join()
    {
        m_thread.join();
    }

private:
    std::thread m_thread;
};


struct TestFile : tessera::Realm::Config {
    TestFile();
    ~TestFile();

    TestFile(const TestFile&) = delete;
    TestFile& operator=(const TestFile&) = delete;
    TestFile(TestFile&&) = default;
    TestFile& operator=(TestFile&&) = default;

    // The file should outlive the object, ie. should not be deleted in destructor
    void persist()
    {
        m_persist = true;
    }

    tessera::DBOptions options() const;

private:
    bool m_persist = false;
    std::string m_temp_dir;
};

struct InMemoryTestFile : tessera::Realm::Config {
    InMemoryTestFile();
    tessera::DBOptions options() const;
};

void advance_and_notify(tessera::Realm& realm);
void on_change_but_no_notify(tessera::Realm& realm);

#if TESSERA_ENABLE_SYNC

using StartImmediately = tessera::util::TaggedBool<class StartImmediatelyTag>;
using EnableSSL = tessera::util::TaggedBool<class EnableSSLTag>;

class SyncServer : private tessera::sync::Clock {
public:
    struct Config {
        StartImmediately start_immediately = true;
        EnableSSL ssl = false;
        std::string local_dir;
    };

    SyncServer(const Config& config);
    ~SyncServer();

    void start();
    void stop();

    std::string url_for_realm(tessera::StringData realm_name) const;
    std::string base_url() const
    {
        return m_url;
    }
    std::string local_root_dir() const
    {
        return m_local_root_dir;
    }
    int port() const;

    template <class R, class P>
    void advance_clock(std::chrono::duration<R, P> duration = std::chrono::seconds(1)) noexcept
    {
        m_now += std::chrono::duration_cast<time_point::duration>(duration).count();
    }

private:
    std::string m_local_root_dir;
    std::shared_ptr<tessera::util::Logger> m_logger;
    tessera::sync::Server m_server;
    std::thread m_thread;
    std::string m_url;
    std::atomic<time_point::rep> m_now{0};

    time_point now() const noexcept override
    {
        return time_point{time_point::duration{m_now}};
    }
};

struct TestUser : tessera::SyncUser {
    const std::string m_user_id;
    std::string m_access_token;
    std::string m_refresh_token;
    std::shared_ptr<tessera::SyncManager> m_sync_manager;
    tessera::SyncUser::State m_state = tessera::SyncUser::State::LoggedIn;

    TestUser(std::string user_id, std::shared_ptr<tessera::SyncManager> sync_manager)
        : m_user_id(std::move(user_id))
        , m_sync_manager(std::move(sync_manager))
    {
    }

    void log_out()
    {
        auto old_state = m_state;
        m_state = tessera::SyncUser::State::LoggedOut;
        m_sync_manager->update_sessions_for(*this, old_state, m_state, {});
    }

    void log_in()
    {
        auto old_state = m_state;
        m_state = tessera::SyncUser::State::LoggedIn;
        m_sync_manager->update_sessions_for(*this, old_state, m_state, m_access_token);
    }

    std::string user_id() const noexcept override
    {
        return m_user_id;
    }
    std::string app_id() const noexcept override
    {
        return "app id";
    }

    std::string access_token() const override
    {
        return m_access_token;
    }
    std::string refresh_token() const override
    {
        return m_access_token;
    }
    tessera::SyncUser::State state() const override
    {
        return m_state;
    }
    bool access_token_refresh_required() const override
    {
        return false;
    }
    tessera::SyncManager* sync_manager() override
    {
        return m_sync_manager.get();
    }

    void request_log_out() override {}
    void request_refresh_location(CompletionHandler&&) override {}
    void request_access_token(CompletionHandler&&) override {}

    void track_realm(std::string_view) override {}
    std::string create_file_action(tessera::SyncFileAction, std::string_view, std::optional<std::string>) override
    {
        return "";
    }
};

struct SyncTestFile : TestFile {
    template <typename ErrorHandler>
    SyncTestFile(const tessera::SyncConfig& sync_config, tessera::SyncSessionStopPolicy stop_policy,
                 ErrorHandler&& error_handler)
    {
        this->sync_config = std::make_shared<tessera::SyncConfig>(sync_config);
        this->sync_config->stop_policy = stop_policy;
        this->sync_config->error_handler = std::forward<ErrorHandler>(error_handler);
        schema_mode = tessera::SchemaMode::AdditiveExplicit;
    }

    SyncTestFile(TestSyncManager&, std::string name = "", std::string user_name = "test");
    SyncTestFile(std::shared_ptr<tessera::SyncUser> user, tessera::bson::Bson partition,
                 tessera::util::Optional<tessera::Schema> schema = tessera::util::none);
    SyncTestFile(std::shared_ptr<tessera::SyncUser> user, tessera::bson::Bson partition,
                 tessera::util::Optional<tessera::Schema> schema,
                 std::function<tessera::SyncSessionErrorHandler>&& error_handler);
    SyncTestFile(TestSyncManager&, tessera::bson::Bson partition, tessera::Schema schema);
    SyncTestFile(std::shared_ptr<tessera::SyncUser> user, tessera::Schema schema, tessera::SyncConfig::FLXSyncEnabled);
};

class TestSyncManager {
public:
    struct Config {
        Config();
        std::string base_path;
        bool should_teardown_test_directory = true;
        bool start_sync_client = true;
    };

    TestSyncManager(const Config& = Config(), const SyncServer::Config& = {});
    ~TestSyncManager();

    std::string base_file_path() const
    {
        return m_base_file_path;
    }
    SyncServer& sync_server()
    {
        return m_sync_server;
    }
    const std::shared_ptr<tessera::SyncManager>& sync_manager()
    {
        return m_sync_manager;
    }

    std::shared_ptr<TestUser> fake_user(const std::string& name = "test");

private:
    std::shared_ptr<tessera::SyncManager> m_sync_manager;
    SyncServer m_sync_server;
    const std::string m_base_file_path;
    const bool m_should_teardown_test_directory = true;
};


bool wait_for_upload(tessera::Realm& realm, std::chrono::seconds timeout = std::chrono::seconds(60));
bool wait_for_download(tessera::Realm& realm, std::chrono::seconds timeout = std::chrono::seconds(60));

#endif // TESSERA_ENABLE_SYNC

#endif
