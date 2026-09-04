# The Apple package's extra surface came from one internal header

`tools/check-install-surface.sh` failed on its first CI run: recorded on macOS,
asserted on Linux, two headers apart. The conclusion written into the script at
the time was that this was a genuine platform difference and not removable:

> both are included by an installed header, so they are not removable: the Apple
> package genuinely has a surface the Linux one does not. Code that compiles
> against the macOS package may not compile against the Linux one.

The first half was true. The second did not follow from it, and nobody checked
which installed header did the including.

It was one: `sync/impl/sync_client.hpp`. Nothing installed includes *that* --
its includers are four `.cpp` files in this library and one test helper, all of
which compile against the source tree, not the package. So the header that made
the surface platform-dependent had no business being in the surface at all.

Removing it takes four more with it:

```
sync/impl/sync_client.hpp                       nothing installed includes it
sync/impl/sync_file.hpp                         nothing installed includes it
sync/impl/network_reachability.hpp              reached only via the other four
sync/impl/apple/network_reachability_observer.hpp   reached only via sync_client
sync/impl/apple/system_configuration.hpp        reached only via the observer
```

229 headers install, on macOS and on Linux, and no line in the manifest carries
the `apple:` prefix any more. The divergence was never a property of the
platforms; it was one internal header dragging its platform's implementation
details into the public package behind it.

## And the package could not be built for Emscripten

`sync_client.hpp` includes, under `#ifdef __EMSCRIPTEN__`:

```cpp
#include <tessera/object-store/sync/impl/emscripten/socket_provider.hpp>
```

That header installs only under `elseif(EMSCRIPTEN)`, and an installed
`sync_client.hpp` on any other platform's package refers to a file the package
does not contain. Anyone building the package for Emscripten got a missing
header, and no CI job builds that target, so the package has been unbuildable
there for as long as it has existed -- silently, because the failure needs both a
platform nobody tests and a header nobody was supposed to include.

Both Emscripten `impl/` headers are now internal too, on the same grounds as the
rest.

## What found it

Not the count. The count had already failed on exactly this and its diagnosis
went into a code comment as a fact about platforms. What found it was asking
which header, once the manifest made that question cheap enough to ask: five
lines naming five paths, and the closure fell out of grepping their includers.

The check reporting a subtraction cost five CI runs to diagnose once. The
comment it left behind cost longer, because a wrong answer written down as a
justification stops the question being asked again.
