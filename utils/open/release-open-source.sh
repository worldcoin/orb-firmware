#!/bin/bash

# exit on error
set -e

COLOR_RED='\033[0;31m'
COLOR_GREEN='\033[0;32m'
COLOR_BLUE='\033[0;34m'
COLOR_END='\033[0m'

# take one argument to allow dry-run
if [ "$#" -ne 1 ]; then
    echo -e "Usage: $0 < run | dry-run >"
    exit 1
fi

# check if dry-run
if [ "$1" == "dry-run" ]; then
    echo -e "${COLOR_BLUE}Dry run${COLOR_END}"
    DRY_RUN=true
else
    DRY_RUN=false
fi

# get current branch
BASE_BRANCH_NAME=$(git branch --show-current)

# check if `open` branch exists
if ! git show-ref --verify --quiet refs/heads/open; then
    # creating a new open branch without any history
    FIRST_COMMIT=true
    TARGET_BRANCH_NAME=open
    git checkout --orphan "$TARGET_BRANCH_NAME"
    echo -e "${COLOR_BLUE}Creating new orphan 'open' branch${COLOR_END}"
else
    # checkout open branch
    git checkout open
fi

clean_branch() {
  echo -e "${COLOR_RED}Cleaning up${COLOR_END}"
  git checkout "$BASE_BRANCH_NAME"
  git branch -D "$TARGET_BRANCH_NAME" || true
}

# move to git root
GIT_ROOT=$(git rev-parse --show-toplevel)
cd "$GIT_ROOT" || exit 1
source VERSION

if [ "$FIRST_COMMIT" = true ]; then
     TARGET_BRANCH_NAME=open
else
  if [ "$DRY_RUN" = true ]; then
      TARGET_BRANCH_NAME=open-dry-run
  else
      TARGET_BRANCH_NAME=open-v"$VERSION_MAJOR"."$VERSION_MINOR"."$VERSION_PATCH"
  fi
  # delete in case it exists
  git branch -D "$TARGET_BRANCH_NAME" || true
  git checkout -b "$TARGET_BRANCH_NAME"
  echo -e "${COLOR_BLUE}Creating new '$TARGET_BRANCH_NAME' branch, based on 'open'${COLOR_END}"
fi

trap 'clean_branch' ERR

echo -e "${COLOR_BLUE}Fetching allowed files from '$BASE_BRANCH_NAME'${COLOR_END}"
# clean up private/internal files
git checkout "$BASE_BRANCH_NAME" -- "$GIT_ROOT"/utils/open/allowlist.txt
git checkout "$BASE_BRANCH_NAME" -- "$GIT_ROOT"/utils/open/blocklist.txt
git checkout "$BASE_BRANCH_NAME" --pathspec-from-file="$GIT_ROOT"/utils/open/allowlist.txt
git rm --cached -r --pathspec-from-file="$GIT_ROOT"/utils/open/blocklist.txt

# append to gitignore
cat "$GIT_ROOT"/utils/open/blocklist.txt >> "$GIT_ROOT"/.gitignore
git add "$GIT_ROOT"/.gitignore

# use public facing repositories
echo -e "${COLOR_BLUE}Enabling compilation for public-facing repo by default${COLOR_END}"
cp "$GIT_ROOT"/utils/open/public.yml.template "$GIT_ROOT"/utils/open/groups.yml
git add "$GIT_ROOT"/utils/open/groups.yml

# Check if "License" is in README.md
echo -e "${COLOR_BLUE}Checking 'License' is in README.md${COLOR_END}"
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
read -e -p "${COLOR_BLUE}Continue with commit? (y/n)${COLOR_END}" -n 1 -r
if [[ $REPLY =~ ^[Yy]$ ]]
then
  git commit -am "Orb Microcontroller Firmware - Release v$VERSION_MAJOR.$VERSION_MINOR.$VERSION_PATCH"
fi
