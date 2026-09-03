# Security

## Reporting a vulnerability

Use GitHub's private vulnerability reporting: **Security → Advisories → Report a
vulnerability** on this repository. That opens a channel visible only to the
maintainers, and it works without an email address anyone has to publish or
monitor.

Please do not open a public issue for a vulnerability. Please do not include a
working exploit in the initial report -- describe the class of problem, the
component, and what an attacker gains. A reproduction as a failing test in
`test/` is the most useful thing you can attach, and it can come later.

There is no bounty programme.

## What version is supported

Tessera is pre-1.0. Only the latest release gets fixes, and there are no
backports. Before 1.0 the fix for a security problem may be a breaking change,
and it will be released as one rather than deferred.

## What is in scope

Tessera is a library and a sync server. The parts where a vulnerability is
meaningful:

- **The sync server's authorization model.** Token verification, path scoping,
  upload privileges, expiry. Anything that lets a client read or write a path its
  token does not permit.
- **Token handling.** `AccessToken` parsing and signature verification, and the
  server's key handling.
- **The file format.** A database file is untrusted input if it came from
  somewhere else. A malformed file should be rejected, not treated as a way to
  execute code or read out of bounds.
- **The sync protocol.** Changesets arriving over the network are untrusted
  input, and so is anything the merge engine is asked to transform.
- **TLS.** Certificate verification in the client and server.

## What is not

- **A server started with `--authenticate-nobody` authenticates nobody.** That
  is what the flag says and what it logs on every connection. Reaching data
  through such a server is not a vulnerability.
- **A server bound to a public address with `--allow-cleartext` speaks
  cleartext.** Likewise.
- **Denial of service by a client the server has authorized.** An authorized
  writer can fill a disk. Rate limiting and quotas are not implemented and are
  not claimed.
- **The inherited upstream surface outside the documented API.** Tessera
  documents two entry points, `<tessera/api.hpp>` and `<tessera/engine.hpp>`,
  and installs a good deal more than they reach. Behaviour of headers outside
  the documented surface carries no promise, including no security promise.
- **Anything requiring an attacker who can already write the database file or the
  process's memory.** The engine trusts its own storage.

## What has already been fixed

Because it says something about where to look. Each of these shipped, and each
is written up in `docs/findings/`:

| Problem | Finding |
|---|---|
| The sync server accepted every connection without verifying a token | `0b-server-has-no-auth.md` |
| A keyless server still demanded a token, and hung the suite | `0b-keyless-still-demanded-a-token.md` |
| Token expiry was checked at the handshake and never again | `0b-expiry-stops-at-the-handshake.md` |
| An `is_admin` check was inverted | `0b-is-admin-was-inverted.md` |
| `PKey::can_sign()` returned true for a key that could not sign | `0b-a-signature-nothing-could-produce.md` |
| Test certificates had expired, disabling the SSL tests | `0b-certificate-expiry.md` |

The pattern in most of them is not a subtle flaw. It is a check that was
declared and never executed, or executed in a configuration nobody deployed.
Reports of that shape are the most valuable ones.
