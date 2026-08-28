# Finding: the sync protocol and merge algebra are already specified in-repo

Date: 2026-08-28
Task: Phase 0a Task 7 (surfaced while surveying `doc/` for deletion)

## What was found

`doc/` was on no deletion list, but was surveyed before touching it. It is not
cruft. It contains 2,037 lines of substantive design documentation, including two
documents that change what Phase 1 has to build:

| Document | Lines | What it is |
|---|---|---|
| **`doc/protocol.md`** | **1,126** | **A complete network protocol specification.** Message-by-message: BIND, IDENT, UPLOAD, DOWNLOAD, with session-establishment sequence diagrams for both the "need client file ident" and "have client file ident" cases. Titled "Network protocol (version 1)" |
| **`doc/algebra_of_changesets.md`** | 98 | The formal theory behind the merge engine: changesets as state transformers, concatenation, and the algebraic identities the OT implementation relies on |
| `doc/changeset.md` | 263 | Changeset wire format |
| `doc/permissions.md` | 206 | Permission model |
| `doc/primer/primer_architecture.md` | 239 | Architecture primer |
| `doc/server_path.md` | 37 | Server path conventions |
| `doc/primer/primer_files.md` | 47 | File layout primer |
| `doc/development/sanitizers.md` | 21 | Sanitizer usage |

Plus `design.pdf`, `design.docx`, `query_engine.pdf`, `query_engine.docx` under
`doc/development/` — unexamined binary design documents worth reading before
Phase 2 touches the query engine.

## Why this matters

The Phase 1 scope in the spec reads "open protocol spec; reference server." Both
halves are further along than assumed:

- The **reference server** builds and passes 173 integration tests
  (`0a-thesis-validation.md`).
- The **protocol specification** exists, at 1,126 lines, written by the people
  who implemented it.

What Phase 1 actually needs is therefore closer to *verifying that
`doc/protocol.md` still matches `sync/protocol.cpp`, publishing it under a name
Tessera controls, and versioning it* than to writing a specification from
scratch. Verification is real work — a decade-old document may have drifted from
the implementation — but it is a fundamentally smaller and lower-risk job than
authoring one.

`algebra_of_changesets.md` is the more unusual asset. Formal reasoning about
merge correctness is exactly what a local-first database needs to be trustworthy,
and almost nobody in this space has written it down.

## Action taken

Nothing deleted from `doc/` except two files documenting services that no longer
exist: `how-to-use-remote-baas-host.md` and `how-to-release.md` (the Evergreen
release flow; Phase 0b writes a replacement).

## Follow-ups

1. **Phase 1:** diff `doc/protocol.md` against `sync/protocol.cpp` and
   `sync/noinst/protocol_codec.cpp` to find drift. Do this before publishing it
   as Tessera's protocol.
2. **Phase 0b:** `doc/protocol.md` and `algebra_of_changesets.md` should be
   linked prominently from the new README. They are among the strongest evidence
   that this is a serious engine and not an abandoned toy.
3. **Phase 2:** read `doc/development/query_engine.pdf` before redesigning the
   query layer.
