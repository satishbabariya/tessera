# A copy of a live data directory opens

Someone running `tessera-sync-server` will ask how to back it up, and until this
was measured the honest answer was that nobody knew. The engine is
copy-on-write and multi-version, which *suggests* that copying the files under a
running server yields a consistent older version rather than a torn one, but
suggestion is not evidence.

## What was done

A server under continuous write load -- four clients, two hundred transactions a
round, six rounds -- with `cp -R` of its data directory taken five times, one
second apart, while the writing continued. Then each copy was opened by a fresh
server and queried.

| snapshot | rows | errors |
|---|---|---|
| 1 | 1933 | 0 |
| 2 | 2628 | 0 |
| 3 | 3801 | 0 |
| 4 | 3801 | 0 |
| 5 | 3801 | 0 |

Every copy opened. None reported corruption. Each holds a prefix of the writes:
the counts rise while writing continues and settle once it stops.

A copy taken while the server is running but idle is complete -- 501 rows in the
original, 501 in the copy, plus the row the probe itself wrote.

## What this does and does not license

It licenses `cp -R` as a backup: the result is an openable database holding
committed writes up to some point. It is a snapshot, not a fence -- writes in
flight when the copy began may or may not be in it, and nothing here says which.
If that matters, stop the server first, which is cheap: it stops on `SIGTERM` by
waiting in `sigwait` and calling `Server::stop()`.

It says nothing about copying with tools that do not read a consistent view of
each file, about network filesystems, or about snapshots taken by a volume
manager underneath the process. Those were not tested.

## Why this is written down

Five copies opening is weak evidence that a sixth will. It is much stronger
evidence than the alternative, which was a plausible argument from the engine's
design and no measurement at all -- and this project has now found eight things
that were plausible, documented, and false.
