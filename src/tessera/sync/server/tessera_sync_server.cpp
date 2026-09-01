// A server you can run.
//
// Until this file existed, "self-hostable" meant linking Tessera::SyncServer
// and writing the program below yourself. The library was the hard part and was
// finished long before; what was missing was twenty lines and a decision about
// what to do when nobody supplies a public key.
//
// That decision is the only interesting thing here. A Server built with no
// public key verifies no signature, so it demands no token, stores none, and
// skips authorization entirely -- see docs/findings/0b-keyless-still-demanded-a-token.md.
// That mode exists for tests. A binary that entered it silently, on a port, in
// production, would be the exact failure this project spent a day removing from
// the server itself, reintroduced at the command line. So it must be asked for
// by name.

#include <tessera/sync/server/server.hpp>
#include <tessera/sync/server/crypto_server.hpp>
#include <tessera/util/logger.hpp>
#include <tessera/util/optional.hpp>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <thread>

namespace {

using namespace tessera;

void usage(const char* argv0)
{
    std::fprintf(stderr,
                 "usage: %s --root DIR [options]\n"
                 "\n"
                 "  --root DIR             where the server keeps its databases (required)\n"
                 "  --public-key PATH      PEM public key used to verify access tokens\n"
                 "  --listen ADDR          address to bind (default: localhost)\n"
                 "  --tls-cert PATH        PEM certificate chain; enables TLS\n"
                 "  --tls-key PATH         PEM private key for --tls-cert\n"
                 "  --port PORT            port to bind, empty for a system-assigned one\n"
                 "                         (default: 7800)\n"
                 "  --id ID                server id, reported in the backup protocol\n"
                 "  --log-level LEVEL      all, trace, debug, detail, info, warn, error,\n"
                 "                         fatal, off (default: info)\n"
                 "  --authenticate-nobody  run without a public key. The server will then\n"
                 "                         verify nothing, require no token from anyone and\n"
                 "                         apply no permissions. For tests only.\n"
                 "  --allow-cleartext      bind a non-loopback address without TLS. Clients\n"
                 "                         send their token in the WebSocket URL, so this\n"
                 "                         puts credentials on the wire in the clear.\n"
                 "  -h, --help             this text\n",
                 argv0);
}

bool parse_log_level(const std::string& s, util::Logger::Level& out)
{
    using L = util::Logger::Level;
    if (s == "all")    { out = L::all;    return true; }
    if (s == "trace")  { out = L::trace;  return true; }
    if (s == "debug")  { out = L::debug;  return true; }
    if (s == "detail") { out = L::detail; return true; }
    if (s == "info")   { out = L::info;   return true; }
    if (s == "warn")   { out = L::warn;   return true; }
    if (s == "error")  { out = L::error;  return true; }
    if (s == "fatal")  { out = L::fatal;  return true; }
    if (s == "off")    { out = L::off;    return true; }
    return false;
}

sync::Server* g_server = nullptr;

// Signals are handled on a thread of their own rather than in a handler.
// Server::stop() takes locks, and calling it from a signal handler would be
// undefined however reliably it appears to work.
void wait_for_signal_then_stop()
{
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    int sig = 0;
    if (sigwait(&set, &sig) == 0 && g_server)
        g_server->stop();
}

} // unnamed namespace

int main(int argc, char** argv)
{
    std::string root, public_key_path, listen_address = "localhost", listen_port = "7800", id = "tessera";
    std::string tls_cert_path, tls_key_path;
    util::Logger::Level level = util::Logger::Level::info;
    bool authenticate_nobody = false, allow_cleartext = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto value = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s: %s needs a value\n", argv[0], name);
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--root")                     root = value("--root");
        else if (arg == "--public-key")          public_key_path = value("--public-key");
        else if (arg == "--listen")              listen_address = value("--listen");
        else if (arg == "--port")                listen_port = value("--port");
        else if (arg == "--id")                  id = value("--id");
        else if (arg == "--tls-cert")            tls_cert_path = value("--tls-cert");
        else if (arg == "--tls-key")             tls_key_path = value("--tls-key");
        else if (arg == "--log-level") {
            std::string l = value("--log-level");
            if (!parse_log_level(l, level)) {
                std::fprintf(stderr, "%s: unknown log level '%s'\n", argv[0], l.c_str());
                return 2;
            }
        }
        else if (arg == "--authenticate-nobody") authenticate_nobody = true;
        else if (arg == "--allow-cleartext")     allow_cleartext = true;
        else if (arg == "-h" || arg == "--help") { usage(argv[0]); return 0; }
        else {
            std::fprintf(stderr, "%s: unrecognised argument '%s'\n", argv[0], arg.c_str());
            usage(argv[0]);
            return 2;
        }
    }

    if (root.empty()) {
        std::fprintf(stderr, "%s: --root is required\n", argv[0]);
        usage(argv[0]);
        return 2;
    }

    // The decision this program exists to make.
    if (public_key_path.empty() && !authenticate_nobody) {
        std::fprintf(stderr,
                     "%s: no --public-key given.\n"
                     "\n"
                     "Without one this server cannot verify a signature, so it would accept\n"
                     "every connection, require no token from anyone, and apply no\n"
                     "permissions to any database it serves.\n"
                     "\n"
                     "Pass --public-key PATH to authenticate clients, or\n"
                     "--authenticate-nobody if that is genuinely what you want.\n",
                     argv[0]);
        return 2;
    }
    if (!public_key_path.empty() && authenticate_nobody) {
        std::fprintf(stderr, "%s: --public-key and --authenticate-nobody contradict each other\n", argv[0]);
        return 2;
    }

    // TLS takes a certificate and its key or neither.
    if (tls_cert_path.empty() != tls_key_path.empty()) {
        std::fprintf(stderr, "%s: --tls-cert and --tls-key must be given together\n", argv[0]);
        return 2;
    }
    const bool tls = !tls_cert_path.empty();

    // The second decision, and the same shape as the first. A client sends its
    // access token in the WebSocket URL -- `?baas_at=<token>`, which is how the
    // server authenticates it at all -- so a connection without TLS carries the
    // credential across the network in the clear. On loopback that is between a
    // process and itself; on any other interface it is on the wire.
    //
    // So binding a non-loopback address without TLS has to be asked for, in the
    // same way that running without a public key does.
    const bool loopback = listen_address == "localhost" || listen_address == "127.0.0.1" ||
                          listen_address == "::1" || listen_address == "ip6-localhost";
    if (!tls && !loopback && !allow_cleartext) {
        std::fprintf(stderr,
                     "%s: refusing to bind %s without TLS.\n"
                     "\n"
                     "Clients send their access token in the WebSocket URL, so a connection\n"
                     "without TLS puts credentials on the wire in the clear. On loopback that\n"
                     "is a process talking to itself; on %s it is not.\n"
                     "\n"
                     "Pass --tls-cert PATH --tls-key PATH to serve over TLS, or\n"
                     "--allow-cleartext if that is genuinely what you want.\n",
                     argv[0], listen_address.c_str(), listen_address.c_str());
        return 2;
    }

    // Block the signals before any thread exists, so every thread inherits the
    // mask and only the waiter below receives them.
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    if (pthread_sigmask(SIG_BLOCK, &set, nullptr) != 0) {
        std::fprintf(stderr, "%s: could not block signals\n", argv[0]);
        return 1;
    }

    try {
        auto logger = std::make_shared<util::StderrLogger>();
        logger->set_level_threshold(level);

        sync::Server::Config config;
        config.logger = logger;
        config.listen_address = listen_address;
        config.listen_port = listen_port;
        config.id = id;
        config.ssl = tls;
        if (tls) {
            config.ssl_certificate_path = tls_cert_path;
            config.ssl_certificate_key_path = tls_key_path;
        }

        util::Optional<sync::PKey> public_key;
        if (!public_key_path.empty())
            public_key = sync::PKey::load_public(public_key_path);
        else
            logger->warn("Running with no public key: this server authenticates nobody "
                         "and applies no permissions");

        sync::Server server(root, std::move(public_key), std::move(config));
        g_server = &server;
        server.start();
        logger->info("Listening on %1:%2 (%3)", listen_address, server.listen_endpoint().port(),
                     tls ? "TLS" : "no TLS");

        std::thread signals(wait_for_signal_then_stop);
        server.run();
        signals.join();
        logger->info("Stopped");
        return 0;
    }
    catch (const std::exception& e) {
        std::fprintf(stderr, "%s: %s\n", argv[0], e.what());
        return 1;
    }
}
