# What making the sync server installable costs

[0b-self-hostable.md](0b-self-hostable.md) records that the sync server is not in
the installed package, and names two obstacles: the headers live under a path
called `noinst`, and the server still expects an App Services access token. It
calls them "the substance of the next milestone" without saying how much
substance that is.

This measures it, because "the milestone" and "roughly a day's work" are
different plans and the difference was not known.

## The library

| | |
|---|---|
| Headers | 10 |
| Sources | 9 |
| Lines | 10,104 |
| Exported target | `SyncServer`, linking `Sync` and `QueryParser` -- both installed |

## Moving it to a public path

Thirty-six include sites reference `tessera/sync/noinst/server/`: 23 in `src`, 13
in `test`. All are `#include` lines. Nothing computes the path, and no build file
encodes it beyond the `add_subdirectory` and the source list.

The server's headers include 29 `tessera/` headers between them. Twenty-three are
already installed. Of the six that are not, five are the server's own -- they
would be installed alongside -- leaving exactly one genuine addition:

    tessera/util/time.hpp

So the dependency closure is essentially already public. The obstacle is the
directory's name, not its contents. This is the opposite of what `noinst`
implies, and the reason it is worth measuring rather than assuming: a name that
described a decision has been read as describing a difficulty.

## Replacing the authentication model

The App Services-shaped code is four files:

| | |
|---|---|
| `access_token.cpp/.hpp` | 413 lines -- JWT parsing and verification |
| `access_control.cpp/.hpp` | 152 lines -- the policy layer over it |
| `permissions.cpp/.hpp` | 121 lines -- the privilege model |
| `crypto_server*.cpp/.hpp` | 75 lines -- the signature backend, three implementations |

761 lines. `server.cpp` touches access control at **four** call sites, and the
protocol carries the token as one field on one message: `signed_user_token` on
BIND.

That is a contained replacement rather than a pervasive one. Whatever
authentication model Tessera adopts, it plugs in behind four calls and one wire
field.

## What this does not settle

The size of the work is not the difficulty of the decision. Choosing an
authentication model for a self-hostable sync server -- who issues credentials,
what a permission is scoped to, what happens when a token expires mid-session --
is a design question, and 761 lines is the cost of implementing an answer rather
than of finding one.

Nor does it settle the naming. `noinst/server` cannot become a public include
path unchanged, and whatever replaces it is an interface that consumers will
write into their source. That decision is cheap to implement and expensive to
revise.

What the measurement rules out is the reading these two obstacles invite: that
the server is deeply entangled with App Services and would have to be
substantially rewritten. It is 10,000 lines of sync server sitting behind a
directory name, with 761 lines of authentication attached at four points.
