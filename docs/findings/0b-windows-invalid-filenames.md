# Twenty-one files made the repository uncloneable on Windows

The nightly Windows job failed at `actions/checkout@v4`, before it could
configure anything:

    error: invalid path '--helpCompaction_Large<std::__1::integral_constant<bool,+true>>.1.path.realm'
    The process 'git.exe' failed with exit code 128

Windows forbids `< > : " | ? *` in filenames. A single tracked path containing
one of them makes `git checkout` fail outright, so the repository could not be
cloned on Windows at all. Not built -- cloned.

## Where they came from

From running the test binary to see its options:

    $ ./tessera-tests --help

The framework does not have a `--help`. It treats an unrecognised argument as
the test path prefix, and wrote twenty-one temporary databases into the
repository root named after it and the tests that created them, angle brackets
and all. `git add -A` on a later commit swept them in.

## Why the hygiene check passed over every one

`tools/check-repo-hygiene.sh` exists precisely to catch committed runtime
artifacts. Its rule read:

    \.tess(\.lock|\.management)?$

`.tess` is the file format's extension, so that is the pattern a reasonable
person writes. The test suite does not use it. `test/util/test_path.hpp` still
names its files `.realm`:

    #define GROUP_TEST_PATH(var_name) TEST_PATH_HELPER(..., "realm");

So the check was looking for the artifacts the *engine* produces and the tests
produce different ones. It has been passing over this class since it was
written, and it passed over these twenty-one.

That is this project's second recurring pattern in its purest form: the property
inspected -- files ending `.tess` -- was adjacent to the property claimed, which
is that no runtime artifact is committed.

## What changed

The artifact rule now matches `.realm` and `.mx` as well as `.tess`.

A new rule rejects any tracked path that is not a legal Windows filename. That
is a different and stronger guarantee than tidiness: the previous rule protects
the repository from looking careless, this one protects it from being unusable.

Both are canary-tested -- a `weird<name>.txt` and a stray `.realm` file each fail
the check, and neither did before.

## The part worth keeping

The Windows job is compile-only and was added because nobody develops on Windows
here. It has earned its place twice now for reasons unrelated to compiling: it
found an exported linker flag that broke consumers, and it found this, which no
amount of testing on macOS or Linux could have surfaced because both accept the
filenames without complaint.

A platform nobody uses is where the assumptions nobody states get checked.
