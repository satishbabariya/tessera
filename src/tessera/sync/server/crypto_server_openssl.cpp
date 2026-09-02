#include <tessera/sync/server/crypto_server.hpp>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/evp.h>

#if OPENSSL_VERSION_MAJOR >= 3
#include <openssl/decoder.h>
#else
#include <openssl/rsa.h>
#endif

using namespace tessera;
using namespace tessera::sync;

namespace {

template <class T, class D>
std::unique_ptr<T, D> as_unique_ptr(T* ptr, D&& deleter)
{
    return std::unique_ptr<T, D>{ptr, std::forward<D>(deleter)};
}

} // namespace

using key_type = std::unique_ptr<EVP_PKEY, void (*)(EVP_PKEY*)>;

struct PKey::Impl {
    key_type key;
    bool both_parts; // true if both public and private key are loaded

    Impl()
        : key(nullptr, nullptr)
    {
    }
};

PKey::PKey()
    : m_impl(new Impl)
{
    m_impl->key = nullptr;
}

PKey::PKey(PKey&&) = default;
PKey& PKey::operator=(PKey&&) = default;

PKey::~PKey() {}

static key_type load_public_from_bio(BIO* bio)
{
#if OPENSSL_VERSION_MAJOR >= 3
    EVP_PKEY* pkey = nullptr;
    const char* format = "PEM";      /* NULL for any format */
    const char* structure = nullptr; /* any structure */
    const char* key_type = "RSA";    /* NULL for any key */
    auto ctx = as_unique_ptr(OSSL_DECODER_CTX_new_for_pkey(&pkey, format, structure, key_type,
                                                           OSSL_KEYMGMT_SELECT_PUBLIC_KEY, nullptr, nullptr),
                             OSSL_DECODER_CTX_free);
    if (!OSSL_DECODER_from_bio(ctx.get(), bio)) {
        throw CryptoError{"Error reading RSA key."};
    }
    return as_unique_ptr(pkey, EVP_PKEY_free);
#else
    pem_password_cb* password_cb = nullptr; // OpenSSL will display a prompt if necessary
    void* password_cb_userdata = nullptr;

    void (*rsa_free)(RSA*) = RSA_free; // silences a warning on VS2017
    auto rsa = as_unique_ptr(PEM_read_bio_RSA_PUBKEY(bio, nullptr, password_cb, password_cb_userdata), rsa_free);
    if (rsa == nullptr)
        throw CryptoError{"Not a valid RSA public key."};

    void (*evp_pkey_free)(EVP_PKEY*) = EVP_PKEY_free; // silences a warning on VS2017
    key_type key = as_unique_ptr(EVP_PKEY_new(), evp_pkey_free);
    if (EVP_PKEY_assign_RSA(key.get(), rsa.get()) == 0)
        throw CryptoError{"Error assigning RSA key."};
    rsa.release();
    return key;
#endif
}

// The private-key half of PKey was declared in crypto_server.hpp and implemented
// by no shipping backend: load_private, sign and an honest can_sign existed only
// in the stub. So the library could verify a token and never produce one, and
// v0.3.0 shipped an authenticating server with no way to issue anyone a
// credential. See docs/findings/0b-a-signature-nothing-could-produce.md.
//
// This mirrors load_public_from_bio, selecting the keypair rather than the
// public half.
static key_type load_private_from_bio(BIO* bio)
{
#if OPENSSL_VERSION_MAJOR >= 3
    EVP_PKEY* pkey = nullptr;
    const char* format = "PEM";
    const char* structure = nullptr;
    const char* key_type = "RSA";
    auto ctx = as_unique_ptr(OSSL_DECODER_CTX_new_for_pkey(&pkey, format, structure, key_type,
                                                           OSSL_KEYMGMT_SELECT_KEYPAIR, nullptr, nullptr),
                             OSSL_DECODER_CTX_free);
    if (!OSSL_DECODER_from_bio(ctx.get(), bio)) {
        throw CryptoError{"Error reading RSA private key."};
    }
    return as_unique_ptr(pkey, EVP_PKEY_free);
#else
    pem_password_cb* password_cb = nullptr;
    void* password_cb_userdata = nullptr;

    void (*rsa_free)(RSA*) = RSA_free;
    auto rsa = as_unique_ptr(PEM_read_bio_RSAPrivateKey(bio, nullptr, password_cb, password_cb_userdata), rsa_free);
    if (rsa == nullptr)
        throw CryptoError{"Not a valid RSA private key."};

    void (*evp_pkey_free)(EVP_PKEY*) = EVP_PKEY_free;
    key_type key = as_unique_ptr(EVP_PKEY_new(), evp_pkey_free);
    if (EVP_PKEY_assign_RSA(key.get(), rsa.get()) == 0)
        throw CryptoError{"Error assigning RSA key."};
    rsa.release();
    return key;
#endif
}

PKey PKey::load_private(const std::string& pemfile)
{
    int (*bio_free)(BIO*) = BIO_free;
    auto bio = as_unique_ptr(BIO_new_file(pemfile.c_str(), "r"), bio_free);
    if (bio == nullptr)
        throw CryptoError{std::string("Could not read PEM file: ") + pemfile};

    PKey result;
    result.m_impl->key = load_private_from_bio(bio.get());
    result.m_impl->both_parts = true;

    return result;
}

PKey PKey::load_private(BinaryData pem_buffer)
{
    std::size_t size = pem_buffer.size();
    int (*bio_free)(BIO*) = BIO_free;
    TESSERA_ASSERT_RELEASE(int(size) <= std::numeric_limits<int>::max());
    auto bio = as_unique_ptr(BIO_new_mem_buf(const_cast<char*>(pem_buffer.data()), int(size)), bio_free);

    PKey result;
    result.m_impl->key = load_private_from_bio(bio.get());
    result.m_impl->both_parts = true;

    return result;
}

PKey PKey::load_public(const std::string& pemfile)
{
    int (*bio_free)(BIO*) = BIO_free; // silences warning on VS2017
    auto bio = as_unique_ptr(BIO_new_file(pemfile.c_str(), "r"), bio_free);
    if (bio == nullptr)
        throw CryptoError{std::string("Could not read PEM file: ") + pemfile};

    PKey result;
    result.m_impl->key = load_public_from_bio(bio.get());
    result.m_impl->both_parts = false;

    return result;
}

PKey PKey::load_public(BinaryData pem_buffer)
{
    std::size_t size = pem_buffer.size();
    int (*bio_free)(BIO*) = BIO_free; // silences a warning on VS2017
    TESSERA_ASSERT_RELEASE(int(size) <= std::numeric_limits<int>::max());
    auto bio = as_unique_ptr(BIO_new_mem_buf(const_cast<char*>(pem_buffer.data()), int(size)), bio_free);

    PKey result;
    result.m_impl->key = load_public_from_bio(bio.get());
    result.m_impl->both_parts = false;

    return result;
}

bool PKey::can_sign() const noexcept
{
    return m_impl->both_parts;
}

bool PKey::can_verify() const noexcept
{
    return true;
}

void PKey::sign(BinaryData message, util::Buffer<unsigned char>& signature) const
{
    // can_sign() has returned true here since the fork -- it reports whether a
    // private key was loaded -- while this function did not exist. Any caller
    // who believed can_sign() got a link error rather than a runtime failure,
    // and nothing called it, so nothing found out. See
    // docs/findings/0b-a-signature-nothing-could-produce.md.
    //
    // SHA-256, matching verify() below, because a signature this produces has to
    // be one that verify() accepts.
    if (!can_sign()) {
        throw CryptoError{"Cannot sign (no private key)."};
    }
    const EVP_MD* digest = EVP_sha256();

    EVP_MD_CTX* ctx = EVP_MD_CTX_create();
    if (!ctx)
        throw CryptoError{"Could not allocate a digest context."};

    unsigned int len = 0;
    signature.set_size(std::size_t(EVP_PKEY_size(m_impl->key.get())));

    bool ok = EVP_SignInit(ctx, digest) == 1 &&
              EVP_SignUpdate(ctx, message.data(), message.size()) == 1 &&
              EVP_SignFinal(ctx, signature.data(), &len, m_impl->key.get()) == 1;

    EVP_MD_CTX_destroy(ctx);

    if (!ok)
        throw CryptoError{"Error signing message."};

    // EVP_PKEY_size is an upper bound; len is what was actually written.
    // Returning the padded buffer would produce a signature that verifies
    // nowhere -- and set_size() cannot be used to trim it, because it is
    // documented to discard the contents. resize() retains the range.
    signature.resize(len, 0, len, 0);
}

bool PKey::verify(BinaryData message, BinaryData signature) const
{
    if (!can_verify()) {
        throw CryptoError{"Cannot verify (no public key)."};
    }
    const EVP_MD* digest = EVP_sha256();

    EVP_MD_CTX* ctx = EVP_MD_CTX_create();

    EVP_VerifyInit(ctx, digest);
    EVP_VerifyUpdate(ctx, message.data(), message.size());

    const unsigned char* sig = reinterpret_cast<const unsigned char*>(signature.data());
    std::size_t size = signature.size();
    TESSERA_ASSERT_RELEASE(int(size) <= std::numeric_limits<int>::max());
    int ret = EVP_VerifyFinal(ctx, sig, int(size), m_impl->key.get());

    EVP_MD_CTX_destroy(ctx);

    if (ret < 0)
        throw CryptoError{"Error verifying message."};
    return ret == 1;
}
