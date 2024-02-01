#!/bin/bash

# exit on error
set -e

# take one argument to allow dry-run
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 < run | dry-run >"
    exit 1
fi

# check if dry-run
if [ "$1" == "dry-run" ]; then
    echo "Dry run"
    DRY_RUN=true
else
    DRY_RUN=false
fi

git checkout open
# move to git root
GIT_ROOT=$(git rev-parse --show-toplevel)
cd "$GIT_ROOT" || exit 1
source VERSION

if [ "$DRY_RUN" = true ]; then
    BRANCH_NAME=open-dry-run
else
    BRANCH_NAME=open-v"$VERSION_MAJOR"."$VERSION_MINOR"."$VERSION_PATCH"
fi

clean_branch() {
  git checkout open
  git branch -D "$BRANCH_NAME" || true
}

clean_branch
git checkout -b "$BRANCH_NAME"
trap 'clean_branch' ERR

# clean up private/internal files
git checkout main -- "$GIT_ROOT"/utils/open/allowlist.txt
git checkout main -- "$GIT_ROOT"/utils/open/blocklist.txt
git checkout main --pathspec-from-file="$GIT_ROOT"/utils/open/allowlist.txt
git rm --cached -r --pathspec-from-file="$GIT_ROOT"/utils/open/blocklist.txt

# append to gitignore
cat "$GIT_ROOT"/utils/open/blocklist.txt >> "$GIT_ROOT"/.gitignore
git add "$GIT_ROOT"/.gitignore

# use public facing repositories
cp "$GIT_ROOT"/utils/open/public.yml.template "$GIT_ROOT"/utils/open/groups.yml
git add "$GIT_ROOT"/utils/open/groups.yml

# Check if "License" is in README.md
if ! grep -q "## License" "$GIT_ROOT"/README.md; then
    echo "

## License

Unless otherwise specified, all code in this repository is dual-licensed under either:

MIT License (LICENSE-MIT)
Apache License, Version 2.0, with LLVM Exceptions (LICENSE-APACHE)
at your option. This means you may select the license you prefer to use.

Unless you explicitly state otherwise, any contribution intentionally submitted for inclusion in the work by you, as
defined in the Apache-2.0 license, shall be dual licensed as above, without any additional terms or conditions.
" >> "$GIT_ROOT"/README.md
fi
git add "$GIT_ROOT"/README.md

git status

# stop here if dry-run
if [ "$DRY_RUN" = true ]; then
    clean_branch
    exit 0
fi

# ask if we should continue with commit
read -p "Continue with commit? (y/n) " -n 1 -r
if [[ $REPLY =~ ^[Yy]$ ]]
then
  git commit -am "Orb Microcontroller Firmware - Release v$VERSION_MAJOR.$VERSION_MINOR.$VERSION_PATCH"
fi
