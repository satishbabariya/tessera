# Vendored: SHA-1

Upstream: https://github.com/clibs/sha1
Commit:   d9ae30f34095107ece9dceb224839f0dc2f9c1c7 (tag 0.1.0~3)
Vendored: 2026-08-28

Was a git submodule. Vendored because it is a two-file dependency hosted in an
individual's repository -- the same class of single-point-of-failure the Phase 0a
supply-chain work exists to remove. See docs/findings/0a-supply-chain.md.

Only the files the build uses are kept: sha1.c and sha1.h (referenced from
src/realm/CMakeLists.txt). The upstream Makefile, package.json, and .travis.yml
were not carried over.

License: public domain. Per sha1.c: "SHA-1 in C, By Steve Reid
<steve@edmweb.com>, 100% Public Domain". The attribution header is preserved
verbatim in the source file.

No local modifications. If this ever needs patching, mark it with a
`// Tessera:` comment stating what changed and why.
