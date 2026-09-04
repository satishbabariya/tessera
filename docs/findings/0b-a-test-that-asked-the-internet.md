# A merge-gate test that asked the internet, and asked it the wrong question

`test/object-store/sync/session/session.cpp`, section `reports DNS error`,
pointed the sync client at a hostname that should not resolve and asserted the
error says `Host not found`:

```cpp
tsm.sync_manager()->set_sync_route("ws://invalid.com:9090", true);
...
std::chrono::seconds(35)); // this sometimes needs to wait for a 30s dns timeout
```

`invalid.com` is a registered domain. Resolving it from this machine three times
in a row:

```
attempt 1: 9.98s -> Host invalid.com not found: 2(SERVFAIL)
attempt 2: 3.87s -> Host invalid.com not found: 2(SERVFAIL)
attempt 3: 0.21s -> Host invalid.com not found: 2(SERVFAIL)
```

SERVFAIL is a resolver failure, not a missing name, so the resolver retries
upstream -- hence the spread, and hence a recorded run of this binary taking
**680.6 seconds and failing** where another took 0.011s.

The delay was the visible problem. The wrong answer was the real one: the
section asserts the reason starts with `Host not found`, which is what NXDOMAIN
produces. SERVFAIL does not reliably produce it. The test was asserting a DNS
outcome it had not arranged to get, and passed only when the resolver happened
to be helpful.

## The fix is four characters

RFC 6761 reserves `.invalid` and guarantees names under it do not exist;
resolvers answer NXDOMAIN immediately without asking anyone:

```
attempt 1: 0.02s -> Host host.invalid not found: 3(NXDOMAIN)
attempt 2: 0.02s -> Host host.invalid not found: 3(NXDOMAIN)
```

Five consecutive runs of the section after the change: 0.08, 0.08, 0.06, 0.07,
0.07 seconds, three assertions each, all passing. Against a worst recorded case
of 680.6s and failing.

## The comment about it was wrong in three ways

A note in `.github/workflows/build.yml` read:

> Excluded from the merge gate: 'reports DNS error' resolves a bogus hostname
> and depends on the runner's resolver. [...] A merge gate that fails on DNS
> conditions trains everyone to ignore red builds.

The reasoning is right. The facts were not:

1. It sat on the **SyncTests** step. The test is in ObjectStoreTests.
2. Neither suite was excluded. Both steps gate -- no `if:`, no
   `continue-on-error`.
3. So the mitigation it describes did not exist anywhere, and the flaky test had
   been gating merges the whole time.

A comment claiming a mitigation that does not exist is worse than no comment,
because it answers the question somebody would otherwise ask. Anyone who
wondered "isn't that test flaky?" found a note saying it had been handled.

## What was not fixed

`UNITTEST_THREADS=1` on SyncTests, here and in `nightly.yml`. Nothing in the
repository records why; CoreTests runs with 2. It is left alone rather than
changed on a guess -- but it is a guess either way until somebody measures
whether that suite passes in parallel.
