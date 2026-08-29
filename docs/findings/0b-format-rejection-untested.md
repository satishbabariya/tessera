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

They write a real database, close it, patch the 24-byte header on disk, and
reopen. The header is duplicated in the test because `SlabAlloc::Header` is
private; `test_transactions.cpp` already carried its own copy for the same
reason.

The last test earns its place separately. A file refused with a message that does
not say what was wrong is a support request rather than an error.

## Canary-tested, in both directions

Five passing tests prove nothing on their own. Each was confirmed to fail against
a deliberately broken engine:

    file_format_ok = true;              -> RejectsOtherVersions fails 4x,
                                           RejectionNamesTheProblem fails
    if (false) { /* mnemonic check */ } -> RejectsRealmMnemonic and
                                           RejectsForeignMnemonic both fail

This is the same discipline the `tools/check-*.sh` scripts are held to, applied
to a test rather than a check. The reason is identical: a gate that cannot fail
is not a gate, and the only way to know is to break the thing it guards.
