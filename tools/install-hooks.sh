#!/usr/bin/env bash
# Installs the git hooks in this directory.
#
# They had no installer. tools/pre-commit and tools/pre-push sat here unlisted
# by tools/README.md, unreferenced by any workflow, and uninstalled -- which is
# why nobody noticed that pre-push had rotted into a hook that rejected this
# repository's own remote. See docs/findings/0b-hooks-nobody-runs.md.
set -euo pipefail
cd "$(dirname "$0")/.."

hooks_dir=$(git rev-parse --git-path hooks)
mkdir -p "$hooks_dir"

for hook in pre-commit pre-push; do
    target="$hooks_dir/$hook"
    if [[ -e "$target" && ! -L "$target" ]]; then
        echo "skipping $hook: $target exists and is not a symlink" >&2
        continue
    fi
    ln -sf "$(pwd)/tools/$hook" "$target"
    echo "installed $hook -> tools/$hook"
done

# pre-commit shells out to git-clang-format and does nothing useful without it.
# Saying so here is cheaper than a commit that silently skips the format check.
if ! command -v git-clang-format >/dev/null 2>&1; then
    echo
    echo "note: git-clang-format is not on PATH, so pre-commit will not check" >&2
    echo "      formatting. Install clang-format to make it effective." >&2
fi
