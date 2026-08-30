#include <thread>
#include <mutex>
#include <condition_variable>

#include <tessera/binary_data.hpp>
#include <tessera/sync/noinst/server/crypto_server.hpp>
#include <tessera/sync/client.hpp>
#include <tessera/sync/noinst/server/access_token.hpp>
#include <tessera/sync/noinst/server/access_control.hpp>

#include "test.hpp"
#include "sync_fixtures.hpp"

using namespace tessera;
using namespace tessera::util;
using namespace tessera::sync;

namespace {

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

#endif // !TESSERA_MOBILE

} // unnamed namespace
