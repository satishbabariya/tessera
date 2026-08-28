# Vendored: SHA-2

Upstream: https://github.com/kalven/sha-2
Commit:   0e9aebf34101c6aa89355fd76ac9cd886735dee1 (heads/master)
Vendored: 2026-08-28

Was a git submodule. Vendored for the same reason as sha-1: a small dependency
hosted in an individual's repository. See docs/findings/0a-supply-chain.md.

Only the files the build uses are kept: sha224.{cpp,hpp} and sha256.{cpp,hpp}
(referenced from src/realm/CMakeLists.txt). sha384, sha512, and sum.cpp were
verified unused anywhere in src/realm and were not carried over. The upstream
CMakeLists.txt is unused -- realm's build compiles these sources directly.

License: public domain. Per upstream README: "Sha-2 is public domain." Adapted
from LibTomCrypt.

No local modifications. If this ever needs patching, mark it with a
`// Tessera:` comment stating what changed and why.
