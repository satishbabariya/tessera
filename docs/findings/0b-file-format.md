# Finding: a file-format version is a position in a lineage, not a number

Date: 2026-08-29
Task: Phase 0b Task 2 (file-format identity)

## What changed

| | Before | After |
|---|---|---|
| Magic mnemonic | `T-DB` | **`TESS`** |
| Format version | 24 | **1** |
| Accepted versions | 10, 11, 20-24 (Phase 0a narrowed to 24) | **1 only** |

`T-DB` was a fossil of TightDB, Realm's original name, carried in every database
file for over a decade. A `.tess` file is now genuinely a different format, so a
corrupt-file report can never be confused between the two.

## Why this was fifteen edits, not two

The spec described this as "new 4-byte mnemonic, format version reset to 1." The
version turned out not to be a value but a **position in Realm's numbering
lineage** (formats ran 7 to 24), and the codebase encoded that history four
different ways -- each failing differently when the number moved:

| Site | Form | Failure mode |
|---|---|---|
| `group.hpp` | `g_current_file_format_version = 24` | new files written with the wrong version |
| `backup_restore.cpp` | `accepted_versions_` list | valid new files **rejected** |
| `transaction.cpp:536` | `TESSERA_ASSERT_EX(target == 24)` | **Debug-only runtime assertion**, path-dependent |
| `group.cpp:1378` | `TESSERA_ASSERT(m_file_format_version >= 7)` | **abort** -- this is what surfaced first |

Only the first two are findable by grepping the *named* constant. The other two
required searching for the literal values `24` and `7`.

`>= 7` is the instructive one: it looks like a version check but actually asserts
"the format has been decided". The `7` was incidental to Realm's history, not to
the invariant. It is now `> 0`, which states the real condition and survives any
future renumbering.

## The mistake worth recording

`DB::upgrade_file_format` did **two independent jobs** under one name: file-format
upgrade *and* history-schema upgrade. Only the first became obsolete.

The name was read, the whole function was deleted, and six tests failed across two
suites with "Database upgrade required but prohibited":

- `SharedRealm::convert` (4 assertions) -- converting a synced Realm to a local
  one changes history schema
- `LangBindHelper_InRealmHistory_Upgrade`, `Replication_HistorySchemaVersionUpgrade`

Compounding it, the guard `if (!allow_file_format_upgrade && (hist_needed ||
format_needed))` was collapsed to `if (hist_needed)`, converting "refuse when
disallowed" into "always refuse".

The surviving half is restored as **`DB::upgrade_history_schema`** -- a name that
describes what it does. The history schema versions independently of the file
format and remains a live upgrade path.

**Rule: when deleting a function, read its body rather than trusting its name.**
In a decade-old codebase a name is the original author's summary, and summaries
drift. This is the fourth instance in this project of a label not matching the
code: the `REALM_APP_SERVICES` guard contained core sync types, `audit.hpp` was a
cross-platform interface with one platform-specific implementation,
`src/external/s2` contains first-party patches, and `upgrade_file_format` also
upgraded history schemas.

## Why the change could not be split

The plan intended to separate the version reset from the machinery removal so the
two would be independently revertible. That is not possible: a tree with format
version 1 and Realm-era upgrade logic **aborts on its own assertions**. Splitting
a change is only safe when each half leaves the system in a consistent state, and
here they do not.

## What was deliberately kept

- The version **check**. Deleting the upgrade must not delete the rejection, or a
  foreign file would be opened as though valid -- a data-loss bug no test would
  catch, since no test supplies a foreign file.
- `Group::validate_top_array` structural validation.
- `Replication::is_upgradable_history_schema` validation.

## Public API impact

`RealmConfig::disable_format_upgrade` and `RealmConfig::backup_at_file_format_change`
are removed rather than left as no-ops. In a clean-break phase a compile error is
a feature: it points a downstream consumer at the exact line. A silently ignored
option is the worse failure -- someone sets `backup_at_file_format_change = true`,
believes backups are happening, and finds out otherwise when they need one.
