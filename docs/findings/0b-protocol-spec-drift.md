# Finding: the protocol spec documents version 1; the code implements version 14

Date: 2026-08-29
Task: Phase 1 de-risking, done during Phase 0b
Status: qualifies a Phase 0a claim

## The claim being corrected

`docs/findings/0a-existing-documentation.md` reported `doc/protocol.md` as "a
complete network protocol specification, 1,126 lines, message by message", and
concluded that Phase 1's work was closer to *verifying and publishing* an
existing spec than to writing one.

The first half is true. The conclusion needs qualifying.

## The drift

| | |
|---|---|
| `doc/protocol.md` title | **Network protocol (version 1)** |
| `sync/protocol.hpp:73` | `get_current_protocol_version()` returns **14** |

The header carries a changelog of all fourteen versions, so the drift is
documented in code even though the spec was never updated:

| Version | Change |
|---|---|
| 2 | Restored erase-always-wins OT behaviour |
| 3 | Mixed, TypeLinks, Set and Dictionary columns |
| 4 | Flexible JSON field in JSON_ERROR |
| 5 | Compensating write errors |
| 6 | Asymmetric tables |
| 7 | Client honours the `action` in JSON_ERROR |
| 8 | Websocket HTTP errors as websocket close codes |
| 9 | PBS→FLX client migration |
| 10 | BIND carries the reason for the session |
| 11 | FLX schema migrations |
| 12 | Estimated progress in DOWNLOAD for FLX |
| 13 | Syncing collections in Mixed columns |
| 14 | Server-initiated bootstraps for role/permission changes |

## Why this is smaller than it looks

At least four of the thirteen changes (9, 11, 12, and much of 14) are **FLX
features**, and Tessera is not carrying FLX forward. Several others are additive
error-reporting refinements rather than message-shape changes.

The message set itself is stable. Comparing the spec's documented messages
against what `protocol_codec` actually parses:

- Spec documents: ALLOC, BIND, CLIENT_VERSION, CLIENT_VERSION_REQUEST, DOWNLOAD,
  ERROR, HTTP, IDENT, JSON_ERROR, LOG_MESSAGE, MARK, PING, PONG, STATE,
  STATE_REQUEST, TRANSACT, UNBIND, UNBOUND, UPLOAD
- Codec parses: bind, download, error, ident, json_error, log_message, mark,
  ping, pong, query_error, test_command, unbind, unbound, upload

Every message the codec handles appears in the spec. The spec additionally
documents messages the current codec does not parse (ALLOC, CLIENT_VERSION,
STATE, TRANSACT), which are the ones dropped as the protocol evolved.

So the spec's *structure* is sound and its *message vocabulary* is a superset of
what is implemented. What it lacks is thirteen versions of refinement.

## The message-level reconciliation, computed precisely

Comparing the spec's documented messages against what `protocol_codec` parses:

| | Count | Messages |
|---|---|---|
| Documented **and** implemented | 12 | BIND, DOWNLOAD, ERROR, IDENT, JSON_ERROR, LOG_MESSAGE, MARK, PING, PONG, UNBIND, UNBOUND, UPLOAD |
| Documented, **not** implemented | 7 | ALLOC, CLIENT_VERSION, CLIENT_VERSION_REQUEST, HTTP, STATE, STATE_REQUEST, TRANSACT |
| Implemented, **not** documented | 2 | QUERY_ERROR, TEST_COMMAND |

Both undocumented messages fall outside a full-sync Tessera:

- **QUERY_ERROR** carries a `query_version` parameter. It is flexible-sync
  machinery and leaves with FLX.
- **TEST_COMMAND** is `Session::send_test_command`, a test facility rather than
  part of the protocol a peer must implement.

**So for a full-sync Tessera, the spec's twelve documented-and-implemented
messages are the complete message set, and nothing needs to be written from
scratch.** The reconciliation is deletion plus semantic refresh, not authorship.

That is a materially better position than the version number alone suggests, and
it is why this was worth measuring rather than assuming in either direction.

## What Phase 1 should actually do

Not "write a protocol specification" and not "publish the existing one as-is",
but a bounded reconciliation:

1. Pin the protocol version Tessera implements. With FLX removed, that is
   probably not 14 — several of 9–14 exist only for FLX. Choosing the number is a
   design decision, and starting Tessera's protocol at 1 has the same argument
   behind it as restarting the file format at 1.
2. Delete the spec's sections for the seven messages the codec no longer parses:
   ALLOC, CLIENT_VERSION, CLIENT_VERSION_REQUEST, HTTP, STATE, STATE_REQUEST,
   TRANSACT. Each was confirmed absent from the codec's dispatch, not merely
   assumed obsolete.
3. Fold versions 2–8 and 13 into the spec text, since those apply to full sync.
4. Skip 9, 11, 12 and the FLX parts of 14.
5. Re-title it for the version Tessera settles on.

That is a document-sized job with a clear definition of done, and the 1,126
existing lines carry most of the weight. It is materially less work than writing
a specification, which was the point of the original finding -- but it is not the
zero-effort "verify and publish" that finding implied.

## Related: a wire identifier corrupted by the rename

While checking this, the Phase 0b rename was found to have silently changed the
WebSocket subprotocol identifier from `com.mongodb.realm-sync#` to
`com.mongodb.tess-sync#`, via the `.realm` → `.tess` substitution.

Every test passed throughout, because client and server are built from the same
tree and both changed together. **A test suite where both peers come from one
source cannot detect a consistently wrong wire identifier**; only an external
peer would fail, and no test simulates one.

Now deliberately `io.tessera.sync#` and `io.tessera.query-sync#`, with the
server's URL routes matched. A clean-break fork with no deployed peers is exactly
the moment when a protocol break costs nothing, and a project that is not
MongoDB's should not advertise `com.mongodb` on the wire.
