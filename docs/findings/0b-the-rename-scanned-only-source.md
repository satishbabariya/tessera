# The rename check scanned source files, and binaries are named in scripts

`tools/check-rename-residue.sh` has two scans and both are good ones: code
identifiers, and identity-bearing string literals. The second is what caught
`realm.io` being sent as a negotiated WebSocket subprotocol eight months after
the rename was declared complete.

Both scan the same file set:

```sh
SOURCES=(--include='*.cpp' --include='*.hpp' --include='*.h'
         --include='*.mm' --include='*.c'
         --include='CMakeLists.txt' --include='*.cmake')
```

A binary is not named in a `.cpp` file. It is named in shell scripts,
workflows, manifests and ignore files, and none of those match those globs.
Eighteen occurrences across eight files survived the rename:

**`tools/run-tests-on-exfat.sh` could not run at all.** It located its binary by
testing three hardcoded paths, all naming the pre-rename core-test executable.
All three failed, so the script exited with

```
Run this script from the build directory after building tests
```

wherever it was run from and whatever had been built. The advice could not be
followed. Nothing in CI runs this script -- it mounts an exFAT image to test a
filesystem without proper locking -- so nothing reported it.

**`test/.gitignore` stopped ignoring the test binaries.** It listed
`/realm-tests`, `/realm-tests-dbg`, `/realm-tests-cov` and no current suite. A
built test binary now shows up as untracked, in a repository whose hygiene check
exists because twenty-one runtime artifacts once got committed. The prevention
layer was broken by the rename and the detection layer never noticed, because
they guard different things.

**`test/Package.appxmanifest`** declared its entry point and executable under
the old name -- a Windows packaging manifest pointing at a binary that is not
built.

The rest were comments in four test sources telling a developer to run a
`valgrind --tool=callgrind` command against a binary that does not exist.

## Then the new scan did not work

A third scan was added for pre-rename binary names, over the file types that
name binaries. Three canaries -- a shell script, a `.gitignore` entry, a
workflow comment, each naming an old binary -- all reported:

```
PASS: no residual pre-rename identifiers, identity strings or binary names
```

The pattern is an alternation, `realm-(tests|sync-tests|...)`, and `grep`
without `-E` matches parentheses and pipes literally. The scan written to catch
eighteen occurrences found none of them, and would have been committed looking
like it worked, because the eighteen had already been fixed by the time it ran.

That is the trap in adding a check and its fix together: the check passes, and
the pass proves nothing. Only the canary distinguishes "nothing is wrong" from
"this cannot detect anything".

## And then it flagged itself

With `-E`, it fired -- on its own explanatory comment, which spelled the old
names while describing them, and on a neighbouring comment mentioning
`realm-trawler`, a foreign tool the pattern should never have included.

Both were fixed at the source rather than with an exclusion list: the pattern
covers only this project's test and benchmark binaries, and the comments
describe the old names without spelling them. A check whose own documentation
trips it teaches people to add exclusions, and an exclusion list is where a
check goes to die.
