// Mints the tokens the server verifies.
//
// v0.3.0 shipped a server that refuses to start without a public key, verifies
// every connection against it, and applies per-path permissions. It shipped
// nothing that can produce a token. A self-hoster could stand up an
// authenticating server and then had no way to let anybody in, which makes the
// authentication work unusable rather than merely incomplete.
//
// The format is the one the server already parses (see AccessToken::parse):
//
//     base64(payload JSON) ":" base64(RSA signature over the payload JSON)
//
// The signature covers the *decoded* JSON, not its base64 -- checked against
// access_token.cpp rather than assumed, because a signer that signs the wrong
// bytes produces tokens that are rejected only at runtime.

#include <tessera/sync/server/access_control.hpp>
#include <tessera/sync/server/access_token.hpp>
#include <tessera/sync/server/crypto_server.hpp>
#include <tessera/sync/server/permissions.hpp>
#include <tessera/util/base64.hpp>
#include <tessera/util/buffer.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <sstream>
#include <string>
#include <vector>

using namespace tessera;

namespace {

void usage(const char* argv0)
{
    std::fprintf(stderr,
                 "usage: %s --key PRIVATE.pem --identity NAME [options]\n"
                 "\n"
                 "Mints an access token for a Tessera sync server. The server verifies it\n"
                 "against the matching public key, the one given to --public-key.\n"
                 "\n"
                 "  --key PATH         PEM private key that signs the token (required)\n"
                 "  --identity NAME    who the token is for (required)\n"
                 "  --access LIST      comma-separated: download, upload, delete, query,\n"
                 "                     create, set-permissions (default: download,upload)\n"
                 "  --path PATH        restrict the token to one database path.\n"
                 "                     Without it the token is valid for every path.\n"
                 "  --expires-in SECS  seconds until expiry. Without it the token never\n"
                 "                     expires, which is rarely what you want.\n"
                 "  --verify PUB.pem   after signing, verify against this public key and\n"
                 "                     print what the server would see\n"
                 "\n"
                 "Generate a key pair with openssl:\n"
                 "  openssl genrsa -out private.pem 2048\n"
                 "  openssl rsa -in private.pem -pubout -out public.pem\n",
                 argv0);
}

// The names the server's own parser accepts, so that a token this tool emits
// and a token the server understands cannot drift apart.
bool known_access(const std::string& name)
{
    return name == "download" || name == "upload" || name == "delete" || name == "query" ||
           name == "create" || name == "set-permissions" || name == "modify-schema";
}

std::string json_escape(const std::string& in)
{
    std::string out;
    for (char c : in) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string to_base64(const char* data, std::size_t size)
{
    std::string out;
    out.resize(util::base64_encoded_size(size));
    util::base64_encode(StringData{data, size}, out);
    return out;
}

} // unnamed namespace

int main(int argc, char** argv)
{
    std::string key_path, identity, path, verify_path;
    std::string access = "download,upload";
    std::int64_t expires_in = -1;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        auto value = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s: %s needs a value\n", argv[0], name);
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--key")             key_path = value("--key");
        else if (arg == "--identity")   identity = value("--identity");
        else if (arg == "--access")     access = value("--access");
        else if (arg == "--path")       path = value("--path");
        else if (arg == "--expires-in") expires_in = std::stoll(value("--expires-in"));
        else if (arg == "--verify")     verify_path = value("--verify");
        else if (arg == "-h" || arg == "--help") { usage(argv[0]); return 0; }
        else {
            std::fprintf(stderr, "%s: unrecognised argument '%s'\n", argv[0], arg.c_str());
            usage(argv[0]);
            return 2;
        }
    }
    if (key_path.empty() || identity.empty()) {
        std::fprintf(stderr, "%s: --key and --identity are required\n", argv[0]);
        usage(argv[0]);
        return 2;
    }

    // Refuse a privilege the server will not recognise, rather than emitting a
    // token that silently grants less than it appears to.
    std::vector<std::string> privileges;
    {
        std::stringstream ss{access};
        std::string item;
        while (std::getline(ss, item, ',')) {
            if (item.empty())
                continue;
            if (!known_access(item)) {
                std::fprintf(stderr, "%s: unknown access '%s'\n", argv[0], item.c_str());
                return 2;
            }
            privileges.push_back(item);
        }
    }
    if (privileges.empty()) {
        std::fprintf(stderr, "%s: --access listed no privileges\n", argv[0]);
        return 2;
    }

    try {
        std::int64_t now = std::int64_t(std::chrono::duration_cast<std::chrono::seconds>(
                                            std::chrono::system_clock::now().time_since_epoch())
                                            .count());

        std::string payload = "{\"identity\":\"" + json_escape(identity) + "\",\"access\":[";
        for (std::size_t i = 0; i < privileges.size(); ++i) {
            if (i)
                payload += ',';
            payload += '"' + privileges[i] + '"';
        }
        payload += "],\"timestamp\":" + std::to_string(now);
        payload += ",\"expires\":";
        payload += (expires_in >= 0) ? std::to_string(now + expires_in) : "null";
        if (!path.empty())
            payload += ",\"path\":\"" + json_escape(path) + "\"";
        payload += "}";

        sync::PKey key = sync::PKey::load_private(key_path);
        if (!key.can_sign()) {
            std::fprintf(stderr, "%s: %s cannot sign; is it a private key?\n", argv[0], key_path.c_str());
            return 1;
        }

        util::Buffer<unsigned char> signature;
        key.sign(BinaryData{payload.data(), payload.size()}, signature);

        std::string token = to_base64(payload.data(), payload.size()) + ":" +
                            to_base64(reinterpret_cast<const char*>(signature.data()), signature.size());

        // Verifying what was just produced, with the server's own parser, is the
        // difference between "a token was printed" and "a token the server
        // accepts was printed".
        if (!verify_path.empty()) {
            sync::AccessControl control{sync::PKey::load_public(verify_path)};
            sync::AccessToken::ParseError error = sync::AccessToken::ParseError::none;
            util::Optional<sync::AccessToken> parsed = control.verify_access_token(token, &error);
            if (!parsed) {
                std::fprintf(stderr, "%s: the token this tool just signed does not verify (error %d)\n",
                             argv[0], int(error));
                return 1;
            }
            std::fprintf(stderr, "verified: identity=%s path=%s expires=%lld\n", parsed->identity.c_str(),
                         parsed->path ? parsed->path->c_str() : "<any>", (long long)parsed->expires);
        }

        std::printf("%s\n", token.c_str());
        return 0;
    }
    catch (const std::exception& e) {
        std::fprintf(stderr, "%s: %s\n", argv[0], e.what());
        return 1;
    }
}
