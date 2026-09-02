# `can_sign()` returned true for a key that could not sign

v0.3.0 shipped a sync server that refuses to start without a public key,
verifies every connection against it, and applies per-path permissions. It
shipped nothing that could produce a token. Standing up an authenticating
server left you unable to let anybody in -- which makes the authentication
unusable rather than merely incomplete.

Writing the tool that mints them found out why nobody had.

## Three backends, one missing function

`crypto_server.hpp` declares:

```cpp
bool can_sign() const noexcept;
void sign(BinaryData message, util::Buffer<unsigned char>& signature) const;
```

What implements them:

| backend | `can_sign()` | `sign()` |
|---|---|---|
| `crypto_server_apple.mm` | `false`, commented "Signing is not yet implemented" | absent |
| `crypto_server_openssl.cpp` | `m_impl->both_parts` -- true whenever a private key is loaded | **absent** |
| `crypto_server_stub.cpp` | present | a no-op producing an empty signature |

The Apple backend is honest: it says it cannot sign, and it cannot. The OpenSSL
backend says it *can* whenever it holds a private key, and there is no `sign` to
call. A caller who checked `can_sign()` before signing -- exactly what a careful
caller does -- got

```
Undefined symbols for architecture arm64:
  "tessera::sync::PKey::sign(tessera::BinaryData, tessera::util::Buffer<unsigned char>&) const"
```

a **link** error, not a runtime one. Nothing in the tree called it, so nothing
ever linked it, so the missing definition sat behind a declaration that promised
otherwise.

This is the fifth thing found in that state, and the first where the absence was
guarded by an accessor actively claiming the opposite.

## What was implemented

`PKey::sign` for OpenSSL, using SHA-256 to match `PKey::verify` directly below
it -- a signature this produces has to be one that function accepts.

One detail nearly shipped wrong. `EVP_PKEY_size` gives an upper bound, and the
signature actually written is shorter, so the buffer has to be trimmed. The
obvious call is:

```cpp
signature.set_size(len);
```

and `util::Buffer::set_size` is documented, three lines above its declaration, as
"Discards the original contents." It would have zeroed the signature it was
meant to trim. `resize(len, 0, len, 0)` retains the range. Reading the header
rather than assuming the name is the only reason that was caught.

The Apple backend still cannot sign. `can_sign()` there returns false and says
so, which is the correct behaviour for an unimplemented function; the token tool
reports it plainly rather than failing obscurely. Building with
`-DTESSERA_FORCE_OPENSSL=ON` gives a signing build on macOS.

## The general form

An accessor that reports a capability is a claim, and a claim needs the same
evidence as any other. `can_sign()` was written to describe the key material --
"a private key is present" -- and read as describing the object -- "this can
sign". Those coincided everywhere it was tested, because it was tested nowhere.
