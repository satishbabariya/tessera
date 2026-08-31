#include <thread>
#include <mutex>
#include <condition_variable>

#include <tessera/binary_data.hpp>
#include <tessera/sync/noinst/server/crypto_server.hpp>
#include <tessera/sync/client.hpp>
#include <tessera/sync/noinst/server/access_token.hpp>
#include <tessera/sync/noinst/server/access_control.hpp>

#include <tessera/db.hpp>
#include <tessera/sync/noinst/client_history_impl.hpp>
#include "test.hpp"
#include "sync_fixtures.hpp"

using namespace tessera;
using namespace tessera::util;
using namespace tessera::sync;

namespace {

// As in test_sync.cpp: a client-side database with replication, on a test path.
#define TEST_CLIENT_DB(name)                                                                                         \
    SHARED_GROUP_TEST_PATH(name##_path);                                                                             \
    auto name = DB::create(make_client_replication(), name##_path);


#if !TESSERA_MOBILE

TEST(Sync_Auth_JWTAccessToken)
{
    AccessToken tok;
    AccessToken::ParseError error = AccessToken::ParseError::none;

    PKey pk1 = PKey::load_public(test_util::get_test_resource_path() + "test_pubkey2.pem");
    AccessControl ctrl(std::move(pk1));

    AccessToken::Verifier& verifier = ctrl.verifier();
    auto exampleJWT =
        "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9."
        "eyJhcHBJZCI6ImlvLnJlYWxtLkF1dGgiLCJhY2Nlc3MiOlsiZG93bmxvYWQiLCJ1cGxvYWQiXSwic3ViIjoiZGYyZjE4NjBjMTk1MjFiYjk0"
        "NjM0OTRjOTI1MTYyZjciLCJwYXRoIjoiL2RlZmF1bHQvX19wYXJ0aWFsL2RmMmYxODYwYzE5NTIxYmI5NDYzNDk0YzkyNTE2MmY3LzBlYzNj"
        "NjdlMTFjNzFkYmU1ZTgzYmZiNDE3MTViZmJlMGQ5ODNmODYiLCJzeW5jX2xhYmVsIjoiZGVmYXVsdCIsInNhbHQiOiIyY2FmZjhlMCIsImlh"
        "dCI6MTU2NDczNzY1NiwiZXhwIjo0NzIwNDExNjE1LCJhdWQiOiJyZWFsbSIsImlzcyI6InJlYWxtIiwianRpIjoiYmM3MTlkY2ItOTA2Ny00"
        "ZTQ4LWI1NmItYTQ3MzMxZDNmZDgxIn0.SGFUR8A-"
        "XXn2i7LFGcWuUlrfcPgUYRj58ZClZrjsW7NSiE1tI5zZSbrEL7vyTPtwbMbMe1qMgdoB1ZdSzt-HAB9RCIrRk40XlHw7flb8jk_"
        "q0hdqPnKbxEMz9wWzzUGOshXj2Yso1NVEX0q04k-ndpAODtuMDiU5T_3vF1czUFA-WXOMDr9dpX_Wn8KeEO0uOvb4_1AvDM_"
        "wK3RF5D9IsJGuvE2Sqbq5j2DPGCgTkBsTcKJPQPcgEDC270nSb9SfitzLEzxoQbhF9M82MQJqhfj4ZThImG6ed7hjUIqdgBFuyBQ4WaMQgPD"
        "vA5KRPYymC5owAHBmGht9wpUFzAbnBg";
    auto result = AccessToken::parseJWT(StringData(exampleJWT), tok, error, &verifier);

    CHECK(result);
    CHECK(error == AccessToken::ParseError::none);
    CHECK_EQUAL(tok.expires, 4720411615);
    CHECK_EQUAL(tok.identity, "df2f1860c19521bb9463494c925162f7");
    CHECK_EQUAL(tok.sync_label, "default");
}


// The token every sync test presents, and the key every sync test configures the
// server with. Nothing has ever checked that the one verifies against the other,
// because the server never verifies anything -- it logs the token and discards
// it. See docs/findings/0b-server-has-no-auth.md.
//
// This has to hold before the server can be made to verify: if the suite's own
// token does not check out against the suite's own key, turning verification on
// fails 461 tests for a reason unrelated to the change.
TEST(Sync_Auth_TheSuitesOwnTokenVerifiesAgainstTheSuitesOwnKey)
{
    PKey key = PKey::load_public(fixtures::test_server_key_path());
    AccessControl control(std::move(key));

    AccessToken token;
    AccessToken::ParseError error = AccessToken::ParseError::none;
    bool ok = AccessToken::parse(StringData(fixtures::g_signed_test_user_token), token, error, &control.verifier());

    CHECK(ok);
    CHECK(error == AccessToken::ParseError::none);
    CHECK_EQUAL(token.identity, "test");

    // Download and upload, on any path, forever: the token carries no path and
    // no expiry. That is the right shape for a test fixture and the wrong shape
    // for anything else.
    CHECK_NOT(bool(token.path));
    CHECK_EQUAL(token.expires, 0);
    CHECK(control.can(token, Privilege::Download, "/any/path"));
    CHECK(control.can(token, Privilege::Upload, "/any/path"));
}


// The same token against a key that did not sign it.
TEST(Sync_Auth_AWrongKeyRejectsTheToken)
{
    PKey wrong = PKey::load_public(test_util::get_test_resource_path() + "test_pubkey2.pem");
    AccessControl control(std::move(wrong));

    AccessToken token;
    AccessToken::ParseError error = AccessToken::ParseError::none;
    bool ok = AccessToken::parse(StringData(fixtures::g_signed_test_user_token), token, error, &control.verifier());

    CHECK_NOT(ok);
    CHECK(error == AccessToken::ParseError::invalid_signature);
}


// AccessToken::parse must reject malformed input rather than abort. An empty
// token is what a client that has not been given one sends, and what anyone
// probing an exposed port sends first.
//
// This was found by making the server verify tokens: base64_decode calls
// Span::back() on the input, which asserts on an empty span, so parse("")
// terminated the process. A server that authenticates is a server that parses
// hostile input, and the parser had never been given any.
TEST(Sync_Auth_MalformedTokensAreRejectedNotFatal)
{
    PKey key = PKey::load_public(fixtures::test_server_key_path());
    AccessControl control(std::move(key));

    const char* malformed[] = {
        "",                       // no token at all
        ":",                      // separator only
        ":abc",                   // empty payload
        "abc:",                   // empty signature
        "not-base64!:also-not",   // invalid alphabet
        "eyJ9",                   // one part, no signature
    };

    for (const char* t : malformed) {
        AccessToken token;
        AccessToken::ParseError error = AccessToken::ParseError::none;
        bool ok = AccessToken::parse(StringData(t), token, error, &control.verifier());
        CHECK_NOT(ok);
        CHECK(error != AccessToken::ParseError::none);
    }
}


// The server verifies the credential on the WebSocket handshake. Until that
// existed nothing checked a token at all, and the fixtures below -- a signature
// that does not verify, and a token past its expiry -- had no test to belong to.
// See docs/findings/0b-tokens-without-tests.md.
//
// A suite that only asserts acceptance cannot tell a system that accepts the
// right things from one that accepts everything. These are the complement.

TEST(Sync_Auth_HandshakeRejectsABadSignature)
{
    TEST_DIR(dir);
    TEST_CLIENT_DB(db);
    bool did_fail = false;
    {
        fixtures::ClientServerFixture fixture(dir, test_context);
        fixture.set_client_side_error_handler([&](Status, bool) {
            did_fail = true;
            fixture.stop();
        });
        fixture.start();

        // The suite's own token with one character of its signature changed.
        // Everything else about it is valid, so a server that rejects this is
        // checking the signature rather than the shape.
        std::string corrupted = fixtures::g_signed_test_user_token;
        corrupted[corrupted.size() - 1] = (corrupted[corrupted.size() - 1] == 'A' ? 'B' : 'A');

        Session session = fixture.make_bound_session(db, "/test", corrupted);
        session.wait_for_download_complete_or_client_stopped();
    }
    CHECK(did_fail);
}


TEST(Sync_Auth_HandshakeRejectsAnExpiredToken)
{
    TEST_DIR(dir);
    TEST_CLIENT_DB(db);
    bool did_fail = false;
    {
        fixtures::ClientServerFixture fixture(dir, test_context);
        fixture.set_client_side_error_handler([&](Status, bool) {
            did_fail = true;
            fixture.stop();
        });
        fixture.start();

        // g_signed_test_user_token_expiration_specified expires at 3000000000.
        // The server reads the clock the fixture gives it, so move that past the
        // expiry rather than waiting until 2065.
        fixture.set_fake_token_expiration_time(3000000001);

        Session session =
            fixture.make_bound_session(db, "/test", fixtures::g_signed_test_user_token_expiration_specified);
        session.wait_for_download_complete_or_client_stopped();
    }
    CHECK(did_fail);
}


// The complement of the two above: the same fixture, an untouched token, and no
// clock movement must still connect. Without this, both tests above would pass
// against a server that refused everything.
TEST(Sync_Auth_HandshakeAcceptsAValidToken)
{
    TEST_DIR(dir);
    TEST_CLIENT_DB(db);
    bool did_fail = false;
    {
        fixtures::ClientServerFixture fixture(dir, test_context);
        fixture.set_client_side_error_handler([&](Status, bool) {
            did_fail = true;
            fixture.stop();
        });
        fixture.start();
        Session session = fixture.make_bound_session(db, "/test");
        session.wait_for_download_complete_or_client_stopped();
    }
    CHECK_NOT(did_fail);
}

#endif // !TESSERA_MOBILE

} // unnamed namespace


// A server given no public key can verify nothing, so it authenticates nobody.
// It must therefore not demand a token either -- and the case that matters is
// not an unsigned token but *no* token, because a client whose access token is
// being refreshed sends `?baas_at=` with an empty value.
//
// This is the shape that hung ObjectStoreTests. The keyless branch used to be
// reached by way of the invalid_signature that verify_access_token reports when
// it holds no key, which only happens for a token that parses. An empty string
// does not parse, so the branch was skipped and the server answered 401 to a
// connection it was never meant to authenticate. The client retried, forever:
// every job in the CI matrix died at the 60-minute timeout with no output.
// Sync_RunServerWithoutPublicKey did not catch it because it presents an
// unsigned token, which parses.
TEST(Sync_Auth_AKeylessServerAcceptsAClientWithNoToken)
{
    TEST_DIR(dir);
    TEST_CLIENT_DB(db);
    bool did_fail = false;
    {
        fixtures::ClientServerFixture::Config config;
        config.server_public_key_path = {}; // keyless
        fixtures::ClientServerFixture fixture(dir, test_context, std::move(config));
        fixture.default_the_user_token = false; // send `?baas_at=` with nothing after it
        fixture.set_client_side_error_handler([&](Status, bool) {
            did_fail = true;
            fixture.stop();
        });
        fixture.start();
        Session session = fixture.make_bound_session(db, "/test", "");
        session.wait_for_download_complete_or_client_stopped();
    }
    CHECK_NOT(did_fail);
}


// The complement, and the reason the branch above is conditional rather than
// unconditional: a server that *was* given a public key must refuse a client
// presenting nothing. Without this, the test above would pass against a server
// that had simply stopped authenticating.
TEST(Sync_Auth_AVerifyingServerRejectsAClientWithNoToken)
{
    TEST_DIR(dir);
    TEST_CLIENT_DB(db);
    bool did_fail = false;
    {
        fixtures::ClientServerFixture fixture(dir, test_context); // default: keyed
        fixture.default_the_user_token = false;
        fixture.set_client_side_error_handler([&](Status, bool) {
            did_fail = true;
            fixture.stop();
        });
        fixture.start();
        Session session = fixture.make_bound_session(db, "/test", "");
        session.wait_for_download_complete_or_client_stopped();
    }
    CHECK(did_fail);
}
