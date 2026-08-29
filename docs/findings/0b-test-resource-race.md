# The test resource copy raced, and only when a resource changed

Re-issuing the test certificates turned one Linux CI job red with a build
failure, not a test failure:

    Error copying file (if different) from
      ".../test/expect_xjson.json" to ".../build/test/resources".

The same commit built and tested cleanly on the four other jobs, including
`ubuntu-latest gcc Debug` -- same compiler family, same platform, same changed
files. A failure that appears on one of five jobs and not the others is not a
disagreement about the code.

## The mechanism

`set_target_resources` in `tools/cmake/Utilities.cmake` gives every test target a
POST_BUILD step:

    cmake -E copy_if_different ${_resources} $<TARGET_FILE_DIR:${_target}>/resources

Outside Apple bundles, `CoreTests`, `SyncTests` and `CombinedTests` are all built
into `build/test/`, so `$<TARGET_FILE_DIR>` is the same directory for all three
and their resource lists overlap heavily. Under `cmake --build --parallel` the
three POST_BUILD steps run concurrently, and two processes writing the same
destination file at the same moment is a race one of them loses.

## Why it had never been seen

`copy_if_different` compares before it writes. When no resource has changed all
three copies find every destination identical, write nothing, and there is
nothing to race on. The race needs a changed resource file to become reachable at
all.

Test resources are certificates, JSON fixtures and tokens -- files that change
almost never. This one had been latent since the resource copy was written, and
it took re-issuing the certificates, which changes seven files at once, to reach
it. Then it presented as a single red Linux job among five green ones, which is
indistinguishable from infrastructure noise.

## The fix

One `TestResources` custom target copies the union once, and the three
executables depend on it instead of each copying for itself. `ObjectStoreTests`
builds into `build/test/object-store/` and has the directory to itself, so it
keeps the per-target copy.

`set_target_resources` became a function rather than a macro, because it now
takes an optional third argument. A macro's `ARGV2` is textual substitution: when
the argument is absent it resolves to whatever `ARGV2` means in an enclosing
scope. Demonstrated rather than assumed --

    macro:    outer(x y NO_COPY) -> nested call with two arguments -> no_copy=ON
    function: outer(x y NO_COPY) -> nested call with two arguments -> no_copy=OFF

The macro version silently skips the copy for a target that never asked to.

## What this one adds

Every other finding here is about a check that inspected the wrong thing. This
one is about a defect whose *trigger* is rare rather than whose detection is
weak. Nothing was mis-verified: the code was correct on every build that did not
change a resource, which was every build for the life of the project.

The practical form: a conditional write is also a conditional race, and a build
step that usually does nothing is a build step that is usually not tested.
