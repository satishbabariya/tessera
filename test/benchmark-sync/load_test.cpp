// A load test that compiles.
//
// The file that stood here did not. It opened with
//
//     #include "load_tester.hpp"
//
// and no such header exists anywhere in this repository, or anywhere in its
// history. Nothing built it -- test/benchmark-sync/CMakeLists.txt names only
// bench_transform.cpp -- so nothing ever discovered that. It came across with
// the fork as a fragment: a file shaped like a load test, referring to an
// implementation that was never here, beside a shell script driving a binary
// that was never produced. See docs/findings/0b-a-load-test-that-never-built.md.
//
// This is a replacement rather than a repair, because there was nothing to
// repair. It drives real sessions against a real server over a real socket,
// using only the installed public API, and reports what it measured.

#include <tessera/db.hpp>
#include <tessera/history.hpp>
#include <tessera/table.hpp>
#include <tessera/transaction.hpp>
#include <tessera/sync/client.hpp>
#include <tessera/sync/network/default_socket.hpp>
#include <tessera/sync/noinst/client_history_impl.hpp>
#include <tessera/util/logger.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

using namespace tessera;

namespace {

struct Options {
    std::string server_address = "localhost";
    sync::port_type server_port = 7800;
    std::string path = "/loadtest";
    std::string token;
    std::string root;
    int clients = 4;
    int transactions = 50;
    bool verbose = false;
    bool converge = false;
    std::int64_t key_base = 0;
    bool contend = false;
    bool tls = false;
    std::string tls_trust;
};

void usage(const char* argv0)
{
    std::fprintf(stderr,
                 "usage: %s --root DIR [options]\n"
                 "\n"
                 "Drives N sync sessions against a running server, each committing M write\n"
                 "transactions, and reports how long that took.\n"
                 "\n"
                 "  --root DIR         scratch directory for the client databases (required)\n"
                 "  --address ADDR     server address (default: localhost)\n"
                 "  --port PORT        server port (default: 7800)\n"
                 "  --path PATH        server-side database path (default: /loadtest)\n"
                 "  --token TOKEN      signed access token presented by every client\n"
                 "  --clients N        concurrent sessions (default: 4)\n"
                 "  --transactions N   write transactions per session (default: 50)\n"
                 "  --verbose          log at debug level\n"
                 "  --contend          every client writes the SAME keys, so their writes\n"
                 "                     conflict and the merge engine has to reconcile them\n"
                 "  --tls              connect over TLS (wss) rather than plain ws\n"
                 "  --tls-trust PATH   PEM trust anchor, for a self-signed server cert\n"
                 "  --key-base N       offset for the primary keys this run writes. Runs\n"
                 "                     against one server path collide without it, so a\n"
                 "                     repeat run updates rows instead of inserting them\n"
                 "  --converge         after uploading, wait for download and check that\n"
                 "                     every client can see every other client\'s rows\n",
                 argv0);
}

// One session: its own database, its own table, its own row per transaction.
// Returns the number of transactions that were committed and uploaded, so the
// caller can tell a partial run from a complete one rather than assuming.
// A pair of barriers, so that every client waits for every other client's
// uploads before checking what it can see. Without them the first client to
// finish would look for rows nobody has written yet and report a convergence
// failure that is really a race in the test.
class Latch {
public:
    explicit Latch(int n) : m_remaining(n) {}
    void count_down()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (--m_remaining == 0)
            m_cv.notify_all();
    }
    void wait()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait(lock, [&] { return m_remaining <= 0; });
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cv;
    int m_remaining;
};

int run_one_client(const Options& opt, int index, std::atomic<int>& failures,
                   const std::shared_ptr<util::Logger>& logger, Latch& everyone_uploaded,
                   std::vector<std::size_t>& seen, std::vector<std::int64_t>& checksum)
{
    try {
        std::string path = opt.root + "/client" + std::to_string(index) + ".tess";
        auto history = sync::make_client_replication();
        DBOptions db_options;
        auto db = DB::create(std::move(history), path, db_options);

        sync::Client::Config client_config;
        client_config.logger = logger;
        // Required: ClientImpl asserts on a config without one, with
        // "Must provide socket provider in sync Client config". Each client
        // gets its own, so the sessions do not share an event loop and the
        // measurement is of the server rather than of one client's scheduler.
        client_config.socket_provider = std::make_shared<sync::websocket::DefaultSocketProvider>(
            logger, "tessera-load-test");
        sync::Client client{std::move(client_config)};

        sync::Session::Config session_config;
        session_config.server_address = opt.server_address;
        session_config.server_port = opt.server_port;
        session_config.realm_identifier = opt.path;
        session_config.service_identifier = "/tessera-sync";
        session_config.signed_user_token = opt.token;
        if (opt.tls) {
            // wss rather than ws. The server's --tls-cert/--tls-key put it on the
            // other end; this is the half that had never been exercised: an
            // openssl s_client handshake proves the server speaks TLS, not that
            // a sync client can complete a session over it.
            session_config.protocol_envelope = sync::ProtocolEnvelope::wss;
            if (!opt.tls_trust.empty())
                session_config.ssl_trust_certificate_path = opt.tls_trust;
        }

        // Constructing the session binds it: the config carries the path, the
        // endpoint and the token, and there is no separate bind() to call.
        sync::Session session{client, db, nullptr, nullptr, std::move(session_config)};

        int committed = 0;
        for (int i = 0; i < opt.transactions; ++i) {
            WriteTransaction wt{db};
            TableRef table = wt.get_group().get_table("class_load");
            if (!table)
                table = wt.get_group().add_table_with_primary_key("class_load", type_Int, "id");
            ColKey col = table->get_column_key("payload");
            if (!col)
                col = table->add_column(type_Int, "payload");
            // A primary key unique across clients, so concurrent sessions do not
            // collide on the same object and measure lock contention instead.
            // Unique per client and per run. Without key_base a second run
            // against the same server path rewrites the first run's keys, so it
            // measures updates while appearing to measure inserts.
            // With --contend the index term is dropped, so every client writes
            // the same keys and their writes collide. That is the case the
            // merge engine exists for, and the one a test where each client
            // owns a disjoint range never reaches.
            std::int64_t id = opt.contend ? opt.key_base + i
                                          : opt.key_base + std::int64_t(index) * 1000000 + i;
            // Under contention every client writes a different payload to the
            // same key, so agreement is not trivial: the clients have to end up
            // holding the same value, whichever write the engine kept.
            table->create_object_with_primary_key(id).set(col, opt.contend ? std::int64_t(index) : id);
            wt.commit();
            ++committed;
        }

        if (!session.wait_for_upload_complete_or_client_stopped()) {
            std::fprintf(stderr, "client %d: stopped before its uploads completed\n", index);
            ++failures;
        }

        // Uploading proves the server accepted the writes. It does not prove
        // anyone else will ever see them, which is the entire promise of a sync
        // engine and was, until this flag, never checked end to end against a
        // deployed server -- only in-process, by the test suite.
        if (opt.converge) {
            everyone_uploaded.count_down();
            everyone_uploaded.wait();
            if (!session.wait_for_download_complete_or_client_stopped()) {
                std::fprintf(stderr, "client %d: stopped before its downloads completed\n", index);
                ++failures;
            }
            auto rt = db->start_read();
            ConstTableRef table = rt->get_table("class_load");
            std::size_t rows = table ? table->size() : 0;
            seen[std::size_t(index)] = rows;

            // Row counts agreeing is not convergence. Two clients can hold the
            // same number of rows and disagree about every value in them, which
            // is exactly what a merge engine that did not converge would look
            // like. Summing the payloads is a cheap way to notice.
            std::int64_t sum = 0;
            if (table) {
                ColKey payload = table->get_column_key("payload");
                if (payload) {
                    for (auto& obj : *table)
                        sum += obj.get<std::int64_t>(payload);
                }
            }
            checksum[std::size_t(index)] = sum;

            // Only a lower bound is asserted here. An equality check against
            // clients * transactions holds solely on an empty server, and every
            // real one has history: pointed at a server that already held 200
            // rows, this reported "sees 200 rows, expected 1" and called a
            // successful restart a failure. What convergence means is that the
            // clients agree, and that is checked once, in main, where all the
            // counts are visible.
            // Under contention the clients share their keys, so the whole run
            // produces `transactions` rows rather than clients * transactions.
            // Using the disjoint figure here reported eight failures against a
            // run that had converged perfectly.
            std::size_t at_least = opt.contend
                                       ? std::size_t(opt.transactions)
                                       : std::size_t(opt.clients) * std::size_t(opt.transactions);
            if (rows < at_least) {
                std::fprintf(stderr, "client %d: sees %zu rows, expected at least %zu\n", index, rows,
                             at_least);
                ++failures;
            }
        }

        client.shutdown_and_wait();
        return committed;
    }
    catch (const std::exception& e) {
        std::fprintf(stderr, "client %d: %s\n", index, e.what());
        ++failures;
        return 0;
    }
}

} // unnamed namespace

int main(int argc, char** argv)
{
    Options opt;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto value = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s: %s needs a value\n", argv[0], name);
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--root")             opt.root = value("--root");
        else if (arg == "--address")     opt.server_address = value("--address");
        else if (arg == "--port")        opt.server_port = sync::port_type(std::stoi(value("--port")));
        else if (arg == "--path")        opt.path = value("--path");
        else if (arg == "--token")       opt.token = value("--token");
        else if (arg == "--clients")     opt.clients = std::stoi(value("--clients"));
        else if (arg == "--transactions") opt.transactions = std::stoi(value("--transactions"));
        else if (arg == "--verbose")     opt.verbose = true;
        else if (arg == "--converge")    opt.converge = true;
        else if (arg == "--key-base")    opt.key_base = std::stoll(value("--key-base"));
        else if (arg == "--contend")     opt.contend = true;
        else if (arg == "--tls")         opt.tls = true;
        else if (arg == "--tls-trust")   opt.tls_trust = value("--tls-trust");
        else if (arg == "-h" || arg == "--help") { usage(argv[0]); return 0; }
        else {
            std::fprintf(stderr, "%s: unrecognised argument '%s'\n", argv[0], arg.c_str());
            usage(argv[0]);
            return 2;
        }
    }
    if (opt.root.empty()) {
        std::fprintf(stderr, "%s: --root is required\n", argv[0]);
        usage(argv[0]);
        return 2;
    }
    if (opt.clients < 1 || opt.transactions < 1) {
        std::fprintf(stderr, "%s: --clients and --transactions must be at least 1\n", argv[0]);
        return 2;
    }

    auto logger = std::make_shared<util::StderrLogger>();
    logger->set_level_threshold(opt.verbose ? util::Logger::Level::debug : util::Logger::Level::error);

    std::fprintf(stderr, "%d clients x %d transactions against %s:%d%s\n", opt.clients,
                 opt.transactions, opt.server_address.c_str(), int(opt.server_port), opt.path.c_str());

    std::atomic<int> failures{0};
    std::atomic<int> committed{0};
    std::vector<std::thread> threads;
    auto started = std::chrono::steady_clock::now();

    Latch everyone_uploaded{opt.clients};
    std::vector<std::size_t> seen(std::size_t(opt.clients), 0);
    std::vector<std::int64_t> checksum(std::size_t(opt.clients), 0);
    for (int i = 0; i < opt.clients; ++i)
        threads.emplace_back([&, i] {
            committed += run_one_client(opt, i, failures, logger, everyone_uploaded, seen, checksum);
        });
    for (auto& t : threads)
        t.join();

    // Convergence is agreement. Whatever the server already held, every client
    // that finished should end up seeing the same thing.
    if (opt.converge) {
        std::size_t first = seen.empty() ? 0 : seen[0];
        for (std::size_t i = 1; i < seen.size(); ++i) {
            if (seen[i] != first) {
                std::fprintf(stderr, "clients disagree: client 0 sees %zu rows, client %zu sees %zu\n",
                             first, i, seen[i]);
                ++failures;
                break;
            }
        }
        std::int64_t first_sum = checksum.empty() ? 0 : checksum[0];
        for (std::size_t i = 1; i < checksum.size(); ++i) {
            if (checksum[i] != first_sum) {
                std::fprintf(stderr,
                             "clients disagree on values: client 0 sums %lld, client %zu sums %lld\n",
                             (long long)first_sum, i, (long long)checksum[i]);
                ++failures;
                break;
            }
        }
        std::fprintf(stderr, "every client sees %zu rows summing %lld\n", first, (long long)first_sum);
    }

    auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    int expected = opt.clients * opt.transactions;

    // Report what happened, including the denominator. A rate printed without
    // the count it was computed from cannot be checked.
    std::printf("committed %d of %d transactions in %.2fs (%.0f/s), %d client failures\n",
                committed.load(), expected, elapsed,
                elapsed > 0 ? committed.load() / elapsed : 0.0, failures.load());

    return (failures.load() == 0 && committed.load() == expected) ? 0 : 1;
}
