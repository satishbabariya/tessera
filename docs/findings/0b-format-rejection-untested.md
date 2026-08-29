# The clean break was asserted, not tested

README.md and ARCHITECTURE.md both say the same thing:

> Tessera rejects any file whose format is not its own. It does not upgrade
> older formats, including Realm's.

This is the fork's central promise. There is no migration path, no compatibility
mode and no upgrade code, and that is deliberate: it is what buys the freedom to
change the format. `docs/findings/0b-file-format.md` records the four constants
that had to agree for it to hold.

Nothing tested it.

## What was and was not covered

`tools/verify/consumer-smoke-test.sh` reads the magic mnemonic at offset 16 of a
file Tessera has just written and checks it says `TESS`. That is the writing
half, verified from outside the project against an installed package, which is
the right place for it.

The reading half had nothing. No test opened a file claiming to be something
else and confirmed it was refused. The behaviour was correct -- `validate_header`
throws `InvalidDatabase` on a foreign mnemonic, and `DB::do_open` throws
`UnsupportedFileFormatVersion` on a version that is not 1 -- but correctness that
nothing exercises is a property of the current commit rather than of the project.

The gap is easy to miss because it inverts the usual shape. Most tests establish
that something works. This one has to establish that something *fails*, and a
missing test that something fails looks exactly like a passing test suite.

## The tests

`test/test_file_format.cpp`, five tests:

| | |
|---|---|
| `WritesItsOwnIdentity` | the mnemonic is `TESS` and the format version is 1 |
| `RejectsRealmMnemonic` | a file beginning `T-DB` is refused -- the case the fork exists for |
| `RejectsForeignMnemonic` | so is any other file that is not a database |
| `RejectsOtherVersions` | versions 2, 10, 24 and 255 are all refused, including versions Realm reached and versions Tessera has not defined |
| `RejectionNamesTheProblem` | the error message contains the offending version number |

And four for the other claim about the bytes on disk, README's "Encryption at
rest. Optional AES-256, applied per page below the engine":

| | |
|---|---|
| `UnencryptedFileContainsThePlaintext` | the control: without a key the string *is* on the disk in the clear |
| `EncryptedFileDoesNotContainThePlaintext` | with a key it is not, and neither is the table name -- "below the engine" means the engine's own structures too |
| `OpensWithCorrectKey` | the positive control |
| `RejectsOpenWithoutKey` / `RejectsOpenWithWrongKey` | an encrypted file is refused without the key and with a key that is one byte wrong |

`test_encrypted_file_mapping.cpp` already covers the cryptor, page IVs,
interrupted writes and concurrent mappings thoroughly -- thirteen tests, all
about the mapping machinery being correct. None of them answers the question a
reader of the claim actually asks, which is whether their data is on the disk in
the clear. The control test is what makes the answer mean anything: it fails if
the search is broken, and it demonstrates that the same string written without a
key is trivially findable.

They write a real database, close it, patch the 24-byte header on disk, and
reopen. The header is duplicated in the test because `SlabAlloc::Header` is
private; `test_transactions.cpp` already carried its own copy for the same
reason.

The last test earns its place separately. A file refused with a message that does
not say what was wrong is a support request rather than an error.

## What is still asserted rather than measured

Auditing the headline claims against the tests turned up one more gap, recorded
here rather than closed.

README: "Crash safety by construction. Commits write new nodes and swap a single
root pointer. There is no write-ahead log to replay, because there is nothing to
replay."

A test for it exists. `Shared_RobustAgainstDeathDuringWrite` in `test_shared.cpp`
forks a hundred processes, each of which calls `_Exit(42)` while holding an open
write transaction, and after every one checks that the database still verifies
and can still be written. It is exactly the right test.

It has never run. Not on Linux, not on macOS, not anywhere.

The inner guard is `#if TESSERA_PLATFORM_APPLE`, with a comment saying the test
"has so far only been seen failing on Linux, so we enable it on ios". Reading
that alone -- which is what the first version of this section did -- gives
"verified on Apple, not on Linux". The outer guard is the one that matters:

    #if !TESSERA_ENABLE_ENCRYPTION && defined(ENABLE_ROBUST_AGAINST_DEATH_DURING_WRITE)

The macro is defined thirty lines earlier in the same file, so it reads as
satisfied. `TESSERA_ENABLE_ENCRYPTION` defaults to `ON` and is on in every
configuration this project builds and tests. The one configuration that turns it
off is the Android nightly, which passes `-DTESSERA_NO_TESTS=ON` and builds only
the `Storage` target.

Measured rather than reasoned:

    $ nm tessera-tests | grep -ci RobustAgainstDeath
    0
    $ UNITTEST_FILTER='Shared_RobustAgainstDeathDuringWrite' ./tessera-tests
    Success: All 0 tests passed. Note: 1662 tests were excluded!

So the strongest claim the project makes about durability -- crash safety by
construction, no log to replay -- is verified nowhere, and has not been for the
life of the fork or, on this evidence, for some time before it.

## It could not have run

Building with `-DTESSERA_ENABLE_ENCRYPTION=OFF` compiles the test in. It then
reports:

    Success: All 1 tests passed (0 checks). Test time: 20us

Twenty microseconds and no checks. Its first statement is

    if (!RobustMutex::is_robust_on_this_platform)
        return;

and on macOS that constant is `false`. Measured from the built library rather
than inferred from the preprocessor:

    is_robust_on_this_platform = false
    _POSIX_THREADS = 200112L      (thread.cpp requires >= 200809L)
    __GNU_LIBRARY__ not defined   (so the glibc branch does not apply either)

So the two guards are not merely restrictive, they are mutually exclusive with
the test doing any work. The compile-time guard admits Apple platforms only. The
runtime guard requires robust POSIX mutexes, which Apple platforms do not have.
Linux has them, and the compile-time guard excludes Linux.

**The test cannot execute its body on any platform.** Satisfying the encryption
guard, as done above, does not help: it merely promotes the test from
uncompiled to compiled-and-immediately-returning. Both states report success.

## How this one hid

Three conditions, each individually plausible and none of them alarming.

A macro guard naming a feature toggle, where the macro is defined thirty lines
above and reads as satisfied. A platform guard naming Apple, which is an
ordinary thing for a test to have. And a runtime capability check that returns
early, which is the correct and conventional way to write a test that needs a
platform feature.

Each is defensible. The conjunction is a test that can never fail, and every
report of it is the word "passed".

Nothing in a test report distinguishes a test that passed, a test that returned
before checking anything, and a test that was never compiled. `1662 tests
passed` is the same sentence in all three cases. A zero-check test is visible
in the count, but only if someone reads per-test check counts, and no gate here
does.

## How many others

Rather than assume the answer, every test name in `test/*.cpp` was run in its own
process and its check count recorded. `tools/analyse-zero-check-tests.sh` does
this; it takes minutes, because the check count is only reported per run and a
run of the whole suite reports one total.

Of 1,784 names: 512 are not in `CoreTests` at all -- they belong to the sync and
object-store suites -- 1,166 ran and checked something, and **106 ran and checked
nothing**.

The 106 are almost entirely legitimate. `Query_DeepCopyLeak1`,
`Table_DeleteCrash`, `Shared_StringIndexBug2`, `Compare_Core_utf8_invalid_crash`
and about a hundred others are regression tests for sequences that once corrupted
memory or leaked. Their assertion is that the process survives. There is nothing
to count, and a check count of zero is the correct outcome.

**Exactly one of the 106 returns early on a guard**, and it is the same guard:

    test_thread.cpp  Thread_RobustMutexTryLock
        if (!RobustMutex::is_robust_on_this_platform) return;

So both of the project's tests of robust-mutex behaviour silently do nothing on
macOS. The difference is that `Thread_RobustMutexTryLock` carries no compile-time
guard, so it compiles and runs on Linux, where robust mutexes exist and CI is
green. Robust mutexes are therefore known to work on the CI runners -- which is
the strongest available prior for the experiment in the crash-safety change.

The useful result here is the size of the class. One. The pattern that hid the
crash-safety test is real and worth a tool, and it is not endemic.

## Canary-tested, in both directions

Five passing tests prove nothing on their own. Each was confirmed to fail against
a deliberately broken engine:

    file_format_ok = true;              -> RejectsOtherVersions fails 4x,
                                           RejectionNamesTheProblem fails
    if (false) { /* mnemonic check */ } -> RejectsRealmMnemonic and
                                           RejectsForeignMnemonic both fail

    DB::create(path)          -> EncryptedFileDoesNotContainThePlaintext fails
      instead of DB::create(path,      on both the value and the table name
      DBOptions(crypt_key(true)))
    key left unmodified       -> RejectsOpenWithWrongKey fails, "Did not throw"

This is the same discipline the `tools/check-*.sh` scripts are held to, applied
to a test rather than a check. The reason is identical: a gate that cannot fail
is not a gate, and the only way to know is to break the thing it guards.

## The canary that lied

The wrong-key canary reported success on its first run: the test passed even with
the key left correct, which would have meant it was worthless.

It was the canary that was wrong. The edit and the preceding file restore landed
in the same second, and `make` treats a source file that is not *newer* than its
object as up to date, so nothing recompiled and the previous binary ran. Forcing
the rebuild produced the expected failure.

This is reproducible in eight lines:

    cmake -S . -B b -G "Unix Makefiles" && cmake --build b   # main returns 1
    python3 -c "open('src/main.cpp','w').write('int main(){return 7;}')"
    cmake --build b && ./b/app                               # still returns 1

CI configures with `-G Ninja`, and its checkouts are fresh, so it has never shown
this. Locally the generator defaults to `Unix Makefiles` on macOS. Whether Ninja
avoids the same one-second window was not tested, because Homebrew on this
machine is broken and ninja could not be installed -- so it is not claimed here.

What is claimed: after an edit-build-test cycle fast enough to land inside one
second, confirm the build actually compiled the file you changed. Four separate
results today were measured against binaries that predated the change being
measured.
