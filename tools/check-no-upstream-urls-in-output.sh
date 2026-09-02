#!/usr/bin/env bash
# Fails if a URL pointing at an upstream Realm repository appears in a string
# the program can print.
#
# Tessera's terminate handler ended every crash with
#
#     !!! IMPORTANT: Please report this at
#     https://github.com/realm/realm-core/issues/new/choose
#
# which shipped in v0.3.0. Anyone whose Tessera process aborted was told, by
# Tessera, to report it to a different project -- one that cannot act on it and
# whose maintainers did not write the code that failed.
#
# tools/check-rename-residue.sh did not catch it. That check looks for
# pre-rename identifiers and identity strings; this is a URL, which is a
# different shape, and it survived a tree-wide rename precisely because the
# rename had no reason to touch it.
#
# Comments are exempt, deliberately. `// See https://github.com/realm/realm-core/issues/3005`
# beside a workaround is provenance: it says why the code is shaped as it is, and
# rewriting it would destroy the citation. What is refused is a Realm URL inside
# a string literal, because that is one the user can be shown.
set -euo pipefail
cd "$(dirname "$0")/.."

# Two earlier versions of this got the same thing wrong in two different ways,
# and the canary is the only reason either was noticed.
#
# The first stripped comments with `sed 's|//.*||'` before looking for the URL.
# That deletes the URL, because `https://` contains `//`.
#
# The second skipped lines matching `:[[:space:]]*//`, meaning "a colon, then a
# comment marker". `https://` matches that too: the colon of the scheme, then
# the slashes.
#
# A URL is very good at looking like a comment. So the prefix that grep -rn adds
# (path:line:) is removed first, and the decision is made on what the source line
# actually begins with.
hits=""
while IFS= read -r match; do
    [ -n "$match" ] || continue
    # grep -rn emits path:line:content -- drop exactly those two fields.
    content=${match#*:}
    content=${content#*:}
    # Leading whitespace off, then: a comment line is provenance, not output.
    trimmed=${content#"${content%%[! ]*}"}
    case "$trimmed" in
        //*|\**|/\**) continue ;;
    esac
    # What remains is code. A Realm URL in it is only reachable by a user if it
    # sits in a string.
    case "$content" in
        *'"'*) hits="$hits$match"$'\n' ;;
    esac
done <<<"$(grep -rn 'github\.com/realm' src/ --include='*.cpp' --include='*.hpp' --include='*.mm' 2>/dev/null || true)"
hits=${hits%$'\n'}

if [ -n "$hits" ]; then
    echo "FAIL: a URL pointing at an upstream Realm repository can be printed to a user:"
    while IFS= read -r line; do printf '    %s\n' "$line"; done <<<"$hits"
    echo
    echo "    A Tessera process should not tell anyone to report anything to realm/*."
    echo "    Comments citing upstream issues are fine and are ignored by this check;"
    echo "    string literals are not."
    exit 1
fi

echo "PASS: no upstream Realm URLs in strings the program can print"
