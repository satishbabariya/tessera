# Eighty-three installed headers no consumer was told about

The package shipped 243 headers. Compiling the two documented tier entry points
-- `<tessera/api.hpp>` and `<tessera/engine.hpp>` -- plus every other header a
consumer of the six exported targets would plausibly include, reaches 160 of
them.

The other 83 installed as public API with no public header mentioning them.
A consumer could only have arrived at any of them by listing the include
directory, which is the definition of an accidental promise.

They came off in four groups, each found by a different question:

| Question | Headers |
|---|---|
| Which are internal by directory name? | 13 `object-store/impl/*` |
| Which installed header includes the Apple-only ones? | 7 `sync/impl/*` |
| Which installed headers does nothing include? | `audit_serializer.hpp`, `external/json/json.hpp`, 2 `object-store/util/*` |
| Which are reachable from no entry point at all? | 22 -- `util/base64.hpp`, `util/uri.hpp`, `util/compression.hpp`, `util/sha_crypto.hpp`, `group_writer.hpp`, `merge/integer_codec.hpp`, `sync/network/websocket.hpp`, and fifteen more |

201 headers install now, and all 201 are reachable.

## The last group is the interesting one

The first three groups were found by asking about a specific suspicious thing.
The fourth was found by asking the general question, and it is only askable once
the entry points are written down.

They were never written down. The project documented two tiers and exported six
targets, and nothing said what a consumer of `Merge`, `Sync`, `QueryParser` or
`SyncServer` should include. So "is this header reachable?" had no answer, and
`util/base64.hpp` shipping as API was not a mistake anyone could point at -- it
was a question nobody could phrase.

`tools/api-entry-points.txt` phrases it: 36 headers, grouped by target, with the
extension points marked as such. `tools/check-surface-is-reachable.sh` compiles
each one alone and fails if any installed header is neither declared nor
reached.

## Compiling each entry point alone also proved something

All 36 are self-contained. The consumer smoke test had been checking that for
two of them; the other 34 held, but nothing had asked.

That is worth separating from the good news. An entry point that needs another
header included before it works is not an entry point, and this property held by
luck rather than by enforcement for as long as those headers have existed.

## Measuring on one platform is not measuring

`-H` reports the include tree the preprocessor actually walked, so it says
nothing about branches not taken. `sync_client.hpp`'s include of
`emscripten/socket_provider.hpp` sits under `#ifdef __EMSCRIPTEN__` and was
invisible to every measurement taken on this laptop.

So before removing the 22, the installed tree was grepped textually for includes
of each one, guard or no guard. Four turned up -- `array_with_find.hpp`,
`tessera_nmmintrin.h`, `util/cf_ptr.hpp`, `util/misc_errors.hpp` -- and every
one of their includers was itself in the removal set. A closed cluster, which is
why they could go together and why checking mattered: `tessera_nmmintrin.h` is
reached only on x86, and nothing in CI's matrix would have caught it.

## What this does not settle

Reachability says a consumer *can* get to a header. It does not say the header
is fit to be public, that its contents are stable, or that the 36 entry points
are the right 36. `tessera/util/*` still contributes headers to the reachable
set because public headers include them; that they are reachable is not a claim
that `util::Buffer` is good API.

The check makes the surface answerable, not correct.
