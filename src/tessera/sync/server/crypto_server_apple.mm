#include <tessera/sync/server/crypto_server.hpp>

#include <tessera/util/cf_ptr.hpp>
#include <tessera/util/scope_exit.hpp>
#include <tessera/util/backtrace.hpp>

#define __ASSERT_MACROS_DEFINE_VERSIONS_WITHOUT_UNDERSCORES 0

#include <CommonCrypto/CommonDigest.h>
#include <Foundation/Foundation.h>
#include <Security/Security.h>
#include <TargetConditionals.h>

using namespace tessera;
using namespace tessera::sync;

using util::CFPtr;
using util::adoptCF;

struct PKey::Impl {
  CFPtr<SecKeyRef> public_key;
  CFPtr<SecKeyRef> private_key;
};

PKey::PKey() : m_impl(std::make_unique<Impl>()) {}

PKey::PKey(PKey &&) = default;
PKey &PKey::operator=(PKey &&) = default;

PKey::~PKey() = default;

// Mirrors load_public_from_data; SecItemImport is told to expect a private key
// rather than a public one. The Impl already carried a `private_key` member that
// nothing ever assigned, because signing was declared in crypto_server.hpp and
// implemented in no shipping backend -- see
// docs/findings/0b-a-signature-nothing-could-produce.md.
static CFPtr<SecKeyRef> load_private_from_data(CFDataRef pem_data) {
#if TESSERA_MOBILE
    static_cast<void>(pem_data);
    TESSERA_UNREACHABLE();
#else
    CFArrayRef itemsCF = nullptr;
    auto scope_exit = util::make_scope_exit([&]() noexcept {
        if (itemsCF)
            CFRelease(itemsCF);
    });

    // These are in/out hints, not assertions. Naming the format exactly --
    // kSecFormatPEMSequence with kSecItemTypePrivateKey, mirroring the public
    // loader above -- rejects the PKCS#1 "BEGIN RSA PRIVATE KEY" that
    // `openssl genrsa` writes by default, with errSecUnknownFormat (-25257).
    // Leaving them unknown lets SecItemImport identify what it was handed, which
    // covers PKCS#1 and PKCS#8 alike.
    SecExternalFormat format = kSecFormatUnknown;
    SecExternalItemType itemType = kSecItemTypeUnknown;
    OSStatus status = SecItemImport(pem_data, CFSTR(".pem"), &format, &itemType, 0, nullptr, nullptr, &itemsCF);
    if (status != errSecSuccess) {
        NSError* error = [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil];
        throw CryptoError(std::string("Could not import PEM private key: ") + error.localizedDescription.UTF8String);
    }
    if (itemType != kSecItemTypePrivateKey) {
        throw CryptoError(std::string("That PEM file does not hold a private key."));
    }
    if (CFArrayGetCount(itemsCF) != 1) {
        throw CryptoError(std::string("Loading PEM private key produced unexpected number of keys."));
    }
    SecKeyRef key = static_cast<SecKeyRef>(const_cast<void*>(CFArrayGetValueAtIndex(itemsCF, 0)));
    if (CFGetTypeID(key) != SecKeyGetTypeID()) {
        throw CryptoError(std::string("Loading PEM private key produced a key of unexpected type."));
    }

    return util::retainCF(key);
#endif
}

static CFPtr<SecKeyRef> load_public_from_data(CFDataRef pem_data) {
#if TESSERA_MOBILE
    static_cast<void>(pem_data);
    TESSERA_UNREACHABLE();
#else
    CFArrayRef itemsCF = nullptr;
    auto scope_exit = util::make_scope_exit([&]() noexcept {
        if (itemsCF)
            CFRelease(itemsCF);
    });

    SecExternalFormat format = kSecFormatPEMSequence;
    SecExternalItemType itemType = kSecItemTypePublicKey;
    OSStatus status = SecItemImport(pem_data, CFSTR(".pem"), &format, &itemType, 0, nullptr, nullptr, &itemsCF);
    if (status != errSecSuccess) {
        NSError* error = [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:nil];
        throw CryptoError(std::string("Could not import PEM data: ") + error.localizedDescription.UTF8String);
    }
    if (CFArrayGetCount(itemsCF) != 1) {
        throw CryptoError(std::string("Loading PEM file produced unexpected number of keys."));
    }
    SecKeyRef key = static_cast<SecKeyRef>(const_cast<void*>(CFArrayGetValueAtIndex(itemsCF, 0)));
    if (CFGetTypeID(key) != SecKeyGetTypeID()) {
        throw CryptoError(std::string("Loading PEM file produced a key of unexpected type."));
    }

    return util::retainCF(key);
#endif
}

PKey PKey::load_public(const std::string &pemfile) {
  NSData *pem_data = [NSData dataWithContentsOfFile:@(pemfile.c_str())];
  if (!pem_data) {
    throw CryptoError(std::string("Could not load PEM file: " + pemfile));
  }

  PKey pkey;
  pkey.m_impl->public_key = load_public_from_data((__bridge CFDataRef)pem_data);
  return pkey;
}

PKey PKey::load_public(BinaryData pem_buffer) {
  CFPtr<CFDataRef> pem_data = adoptCF(CFDataCreateWithBytesNoCopy(
      kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(pem_buffer.data()),
      pem_buffer.size(), kCFAllocatorNull));

  PKey pkey;
  pkey.m_impl->public_key =
      load_public_from_data(static_cast<CFDataRef>(pem_data.get()));
  return pkey;
}

PKey PKey::load_private(const std::string &pemfile) {
  NSData *pem_data = [NSData dataWithContentsOfFile:@(pemfile.c_str())];
  if (!pem_data) {
    throw CryptoError(std::string("Could not load PEM file: " + pemfile));
  }

  PKey pkey;
  pkey.m_impl->private_key = load_private_from_data((__bridge CFDataRef)pem_data);
  return pkey;
}

PKey PKey::load_private(BinaryData pem_buffer) {
  CFPtr<CFDataRef> pem_data = adoptCF(CFDataCreateWithBytesNoCopy(
      kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(pem_buffer.data()),
      pem_buffer.size(), kCFAllocatorNull));

  PKey pkey;
  pkey.m_impl->private_key =
      load_private_from_data(static_cast<CFDataRef>(pem_data.get()));
  return pkey;
}

bool PKey::can_sign() const noexcept { return bool(m_impl->private_key); }

void PKey::sign(BinaryData message, util::Buffer<unsigned char> &signature) const {
  if (!can_sign()) {
    throw CryptoError{"Cannot sign (no private key)."};
  }

  CFPtr<CFDataRef> messageCF = adoptCF(CFDataCreateWithBytesNoCopy(
      kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(message.data()),
      message.size(), kCFAllocatorNull));
  if (!messageCF) {
    throw util::bad_alloc();
  }

  if (@available(macOS 10.12, *)) {
    CFErrorRef error = nullptr;
    // The same algorithm verify() checks with, directly below. A signature this
    // produces has to be one that function accepts, and the OpenSSL backend
    // signs SHA-256 over the message too, so a token minted on either platform
    // verifies on both.
    CFPtr<CFDataRef> signatureCF = adoptCF(SecKeyCreateSignature(
        m_impl->private_key.get(),
        kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256, messageCF.get(),
        &error));
    if (!signatureCF) {
      auto errorCF = adoptCF(error);
      auto errorNS = (__bridge NSError *)error;
      throw CryptoError(std::string("Could not sign message: ") +
                        errorNS.localizedDescription.UTF8String);
    }
    CFIndex length = CFDataGetLength(signatureCF.get());
    signature.set_size(std::size_t(length));
    CFDataGetBytes(signatureCF.get(), CFRangeMake(0, length), signature.data());
  } else {
    TESSERA_TERMINATE("Sync server requires macOS 10.12 or later");
  }
}

bool PKey::can_verify() const noexcept { return bool(m_impl->public_key); }

bool PKey::verify(BinaryData message, BinaryData signature) const {
  if (!can_verify()) {
    throw CryptoError{"Cannot verify (no public key)."};
  }

  CFPtr<CFDataRef> signatureCF = adoptCF(CFDataCreateWithBytesNoCopy(
      kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(signature.data()),
      signature.size(), kCFAllocatorNull));
  CFPtr<CFDataRef> messageCF = adoptCF(CFDataCreateWithBytesNoCopy(
      kCFAllocatorDefault, reinterpret_cast<const UInt8 *>(message.data()),
      message.size(), kCFAllocatorNull));
  if (!signatureCF || !messageCF) {
    throw util::bad_alloc();
  }

  CFErrorRef error = nullptr;
  if (@available(macOS 10.12, *)) {
    bool result =
        SecKeyVerifySignature(m_impl->public_key.get(),
                              kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256,
                              messageCF.get(), signatureCF.get(), &error);
    if (result) {
      return true;
    }
  } else {
    // This is now only used in tests, so no need for a fallback for older
    // macOS versions.
    TESSERA_TERMINATE("Sync server requires macOS 10.12 or later");
  }

  auto errorCF = adoptCF(error);
  auto errorNS = (__bridge NSError *)error;
  if ([errorNS.domain isEqualToString:NSOSStatusErrorDomain] &&
      errorNS.code == errSecVerifyFailed) {
    // Valid input, but the signature doesn't match
    return false;
  }

  std::string description;
  @autoreleasepool {
    description = util::format("Error verifying message: %1",
                               errorNS.description.UTF8String);
  }
  throw CryptoError(description);
}
