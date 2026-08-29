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

This *is* tested. `Shared_RobustAgainstDeathDuringWrite` in `test_shared.cpp`
forks a hundred processes, each of which calls `_Exit(42)` while holding an open
write transaction, and after every one of them checks that the database still
verifies and can still be written.

It is guarded `#if TESSERA_PLATFORM_APPLE`. Its comment says the test "has issues
that has not been fully understood, but could be related to interaction between
posix robust mutexes and the fork() system call. it has so far only been seen
failing on Linux".

So the strongest claim the project makes about durability is verified on macOS
and iOS, and not on Linux -- which is the platform a self-hosted server would run
on. Whether the exclusion reflects a flaw in the test or a real difference in how
robust mutexes recover after `fork()` on Linux is not known here, and inherited
comments have been wrong often enough in this project that the question deserves
its own investigation rather than an assumption.

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
