# AGENT.md

This file provides guidance to AI coding assistants when working with the orb-firmware repository.

## Overview

This repository contains the MCU firmware for the Worldcoin Orb, built with Zephyr RTOS.

## Prime Directive

- Keep changes small and compiling.
- Prefer existing patterns; do not introduce new dependencies without asking.
- If unsure, propose 2 options with tradeoffs (don't guess).

## Path Discovery

This document uses semantic path references. Resolve them as follows:

| Reference         | How to Find                                           | Contains              |
| ----------------- | ----------------------------------------------------- | --------------------- |
| `<WORKSPACE>`     | Run `west topdir`                                     | West workspace root   |
| `<PUBLIC_REPO>`   | Find directory containing `main_board/` and `VERSION` | Public firmware repo  |
| `<PRIVATE_REPO>`  | Find directory containing `sec_board/` (if available) | Private firmware repo |
| `<MANIFEST_REPO>` | Run `west manifest --path` and get parent directory   | Active west manifest  |

**Quick discovery commands:**

```bash
# Workspace root
west topdir

# Find public repo (contains main_board/)
find "$(west topdir)" -type d -name "main_board" -path "*/orb/*" 2>/dev/null | head -1 | xargs dirname

# Check if private repo exists (contains sec_board/)
find "$(west topdir)" -type d -name "sec_board" 2>/dev/null | head -1 | xargs dirname
```

## Build System

- **RTOS**: Zephyr
- **Build Tool**: West (CMake under the hood)
- **Language**: C
- **Structure**: West workspace with multiple repositories managed by west

## Key Folders

Relative to `<WORKSPACE>`:

- `<PUBLIC_REPO>/main_board/` - Main MCU firmware source
- `<PUBLIC_REPO>/VERSION` - Firmware version file
- `<PRIVATE_REPO>/sec_board/` - Security MCU firmware (internal only)
- `boards/` - Board overlays / definitions (if present)
- `dts/` - Devicetree overlays (if present)
- `tests/` - Tests (if present)

## Environment Assumptions

- Commands are run from `<WORKSPACE>` (where `west topdir` works).
- Toolchain is installed and Zephyr env is set up.

## Build & Flash Commands

All commands are run from `<WORKSPACE>` root.

**Available boards:** `pearl_main`, `diamond_main` (main MCU), `pearl_security`, `diamond_security` (security MCU, internal only)

### Build (first time / new board)

```bash
# Main board (public)
west build -b <BOARD> -d build/<BOARD> <PUBLIC_REPO>/main_board

# Example for Pearl main board
west build -b pearl_main -d build/pearl_main orb/public/main_board
```

### Incremental build

```bash
west build -d build/<BOARD>
```

### Clean rebuild (use when build system is "stuck")

```bash
west build -p always -d build/<BOARD>
```

### Release build (smaller binary, optimized)

```bash
west build -b <BOARD> -d build/<BOARD> <PUBLIC_REPO>/main_board -- -DCMAKE_BUILD_TYPE=Release
```

### Flash

```bash
west flash -d build/<BOARD>
```

### Debug

```bash
west debug -d build/<BOARD>
```

## Testing

Preferred: Twister for hardware-in-the-loop (HIL) tests.

> **Important:** AI agents should **propose** the twister command but **NOT run it automatically**. The user must review and execute it manually.

### Prerequisites

Twister requires `ZEPHYR_BASE` to be set and Zephyr scripts in `PATH`.

**Nix environment (recommended):** If using nix/direnv (`direnv allow`), this is handled automatically.

**Manual setup:** If not using nix, source from `<WORKSPACE>`:

```bash
source zephyr/zephyr-env.sh
```

### Discover Required Parameters

Before running twister, find the following:

**1. Serial port** (for test output):

```bash
# macOS
ls /dev/cu.usbserial-*

# Linux
ls /dev/ttyUSB* /dev/ttyACM*
```

**2. Debugger unique ID** (for flashing):

```bash
pyocd list
# or for ST-Link
st-info --probe
```

### Twister Command Template

Run from `<PUBLIC_REPO>/main_board/` directory:

```bash
twister -vv -ll DEBUG \
  -T . \
  -A ./../ \
  -p <BOARD>/<QUALIFIER> \
  --device-serial <SERIAL_PORT> \
  -c \
  --device-testing \
  --west-flash="-i=<DEBUGGER_ID>"
```

**Arguments explained:**

| Argument                 | Description                                         | Example                                                  |
| ------------------------ | --------------------------------------------------- | -------------------------------------------------------- |
| `-vv`                    | Very verbose output                                 |                                                          |
| `-ll DEBUG`              | Log level debug                                     |                                                          |
| `-T .`                   | Test suite root (current dir)                       |                                                          |
| `-A ./../`               | Board root (parent directory for board definitions) |                                                          |
| `-p <BOARD>/<QUALIFIER>` | Platform with qualifiers                            | `pearl_main/stm32g474xx`, `diamond_security/stm32g474xx` |
| `--device-serial`        | Serial port for device output                       | `/dev/cu.usbserial-xxx` (macOS), `/dev/ttyUSB0` (Linux)  |
| `-c`                     | Clobber (clean) previous output                     |                                                          |
| `--device-testing`       | Run tests on actual hardware                        |                                                          |
| `--west-flash="-i=<ID>"` | Debugger unique ID for flashing                     | `-i=851007786`                                           |

### Example (AI should propose, not run)

```bash
# From <PUBLIC_REPO>/main_board/
source ../../../zephyr/zephyr-env.sh

twister -vv -ll DEBUG \
  -T . \
  -A ./../ \
  -p pearl_main/stm32g474xx \
  --device-serial /dev/cu.usbserial-bfd00a013 \
  -c \
  --device-testing \
  --west-flash="-i=851007786"
```

See [Twister documentation](https://docs.zephyrproject.org/latest/develop/test/twister.html) for more options.

## Style & Quality Gates

- If the repo defines formatting/lint scripts, run them.
- Don't reformat unrelated code.

## Output Expectations

- **Before coding**: A short plan (3–6 steps).
- **After coding**: What changed + where, and the exact build/test commands you ran + results.

---

## Version Bump Process (Public Repository)

When asked to "bump the version", "create a new version", or "prepare a release" in `<PUBLIC_REPO>`, follow these steps:

### 1. Ensure on Latest Main Branch

```bash
cd <PUBLIC_REPO>
git fetch origin
git checkout main
git pull
```

### 2. Find the Last Version Bump Commit

```bash
git log --oneline --grep="build(release): mcu fw version" -1
```

This identifies the commit hash of the previous version bump.

### 3. List Changes Since Last Version Bump

```bash
git log --oneline <last-version-commit>..HEAD
```

Review these commits to understand what changed. Categorize by component (main board, etc.) and type (feat, fix, chore).

### 4. Update the VERSION File

The `VERSION` file in `<PUBLIC_REPO>` contains:

```
VERSION_MAJOR=X
VERSION_MINOR=Y
VERSION_PATCH=Z
```

**Bump rules:**

- **PATCH**: Bug fixes, small improvements (most common)
- **MINOR**: New features, non-breaking changes
- **MAJOR**: Breaking changes, major new functionality

Typically, increment `VERSION_PATCH` by 1.

### 5. Create a New Branch

Branch follows format `release/vX.Y.Z`

### 6. Create the Version Bump Commit

**Commit message format:**

```
build(release): mcu fw version X.Y.Z

<component>:

- <type>: <description>
- <type>: <description>

Signed-off-by: <author name> <author email>
```

**Example:**

```
build(release): mcu fw version 4.0.4

main board:

- feat: improve liquid lens quantization and control loop
- chore: cap face cam IR duration to 2000us

Signed-off-by: Cyril Fougeray <cyril.fougeray@toolsforhumanity.com>
```

### 7. Create the Commit

```bash
git add VERSION
git commit -m "$(cat <<'EOF'
build(release): mcu fw version X.Y.Z

main board:

- <changes listed here>

Signed-off-by: <name> <email>
EOF
)"
```

### Complete Example Workflow

```bash
# 1. Get latest main
cd <PUBLIC_REPO>
git fetch origin && git checkout main && git pull origin main

# 2. Find last version bump
git log --oneline --grep="build(release): mcu fw version" -1
# Output: 00ec1145 build(release): mcu fw version 4.0.3 (#440)

# 3. List changes since then
git log --oneline 00ec1145..HEAD
# Output shows commits to summarize

# 4. Edit VERSION file (bump PATCH from 3 to 4)

# 5. Commit with changelog
git add VERSION
git commit  # with proper message format
```

---

## Commit Message Conventions

- Follow Conventional Commits format
- Common prefixes: `feat`, `fix`, `chore`, `build`, `docs`, `refactor`, `test`
- Version bump commits use: `build(release): mcu fw version X.Y.Z`
- Include `Signed-off-by:` line

## Directory Structure

```
<WORKSPACE>/
├── AGENT.md             # AI assistant instructions (this file)
├── <PUBLIC_REPO>/
│   ├── VERSION          # Firmware version (MAJOR.MINOR.PATCH)
│   ├── main_board/      # Main MCU firmware source
│   │   └── src/
│   │       └── optics/  # Optics-related modules
│   ├── west.yml         # West manifest
│   └── zephyr/          # Zephyr module configuration
├── <PRIVATE_REPO>/      # Internal only (if available)
│   ├── west.yml         # Private west manifest
│   └── sec_board/       # Security MCU firmware
├── bootloader/          # Bootloader code
├── modules/             # Additional modules
└── zephyr/              # Zephyr RTOS
```

---

## Isolated Workspace (Git Worktrees)

Use git worktrees to work on a feature in a fully isolated copy of the workspace without affecting your main checkout. This is useful for parallel development, experiments, or vibe coding sessions where you want a throwaway environment.

### Creating an Isolated Workspace

Run from `<WORKSPACE>` root:

```bash
# 1. Define variables
WORKSPACE=$(west topdir)
BRANCH_NAME="feature/my-feature"
WT_NAME="my-feature"
NEW_WS="${WORKSPACE}-wt-${WT_NAME}"

# 2. Create worktrees for both repos
mkdir -p "${NEW_WS}/orb"

cd "${WORKSPACE}/orb/public"
git worktree add "${NEW_WS}/orb/public" -b "${BRANCH_NAME}"

cd "${WORKSPACE}/orb/private"
git worktree add "${NEW_WS}/orb/private" -b "${BRANCH_NAME}"

# 3. Set up .west configuration
mkdir -p "${NEW_WS}/.west"
cp "${WORKSPACE}/.west/config" "${NEW_WS}/.west/config"

# 4. Fetch dependencies (zephyr, modules, etc.)
# IMPORTANT: Use --narrow and --depth=1 to avoid cloning full history
cd "${NEW_WS}"
west update --narrow --fetch-opt=--depth=1
```

### Pitfalls & Lessons Learned

1. **`west update` must use `--narrow --fetch-opt=--depth=1`**: Without these flags, `west update` fetches the full history of every dependency (Zephyr, HAL, etc.) which takes a very long time and wastes disk space. Always use the shallow fetch.

2. **Do NOT change `west.yml` revision to your branch name before pushing**: The private manifest pins `orb-firmware` to a commit hash. If you change this to a branch name that doesn't exist on the remote yet, `west update` will fail with `fatal: couldn't find remote ref`. Keep the original commit hash for `west update`, then change it only if needed after the branch is pushed.

3. **Avoid `git stash` in worktrees**: Using `git stash` + `git stash pop` in a worktree can leave HEAD detached. If you need to temporarily set aside changes, prefer committing to the branch and amending later.

4. **The public repo remote is named `worldcoin`**, not `origin`. Use `git push -u worldcoin <branch>`.

### Cleaning Up

```bash
# Remove worktrees (from the ORIGINAL workspace repos)
cd "${WORKSPACE}/orb/public"
git worktree remove "${NEW_WS}/orb/public"

cd "${WORKSPACE}/orb/private"
git worktree remove "${NEW_WS}/orb/private"

# Remove remaining files (west-managed deps, build artifacts)
rm -rf "${NEW_WS}"
```

---

## Linear Ticket Workflow

When working on a fix or feature linked to a Linear ticket:

1. **Create a worktree** using the Linear-suggested branch name (from the ticket details).
2. **Implement the changes**, build, and verify.
3. **Commit directly** with a proper Conventional Commits message and `Signed-off-by:` line. Do not ask for permission to commit - commit as soon as the change is verified.
4. **Ask the user** whether the branch should be pushed to the remote. Do not push without confirmation.

---

## Private Repository Release Process (Internal Only)

> **Note:** This section only applies when `<PRIVATE_REPO>` exists (contains `sec_board/`).

When creating a new version in `<PRIVATE_REPO>`, follow these steps:

### 1. Update Public Repository Reference

First, pull the latest `main` branch in `<PUBLIC_REPO>`:

```bash
cd <PUBLIC_REPO>
git checkout main
git pull
```

### 2. Get the Git Revision

Get the current git revision hash from `<PUBLIC_REPO>`:

```bash
cd <PUBLIC_REPO>
git rev-parse HEAD
```

### 3. Checkout the Private Repository

Before modifying the repo, ensure you're on the latest `main` branch:

```bash
cd <PRIVATE_REPO>
git fetch origin
git checkout main
git pull origin main
```

### 4. Update west.yml

Update the `revision` field for `orb-firmware` in `<PRIVATE_REPO>/west.yml` with the git revision obtained in step 2:

```yaml
projects:
  - name: orb-firmware
    revision: <NEW_GIT_REVISION_HERE> # Update this line
    import: ...
```

### 5. Determine Version Number

Read the version from `<PUBLIC_REPO>/VERSION`:

```bash
cat <PUBLIC_REPO>/VERSION
```

### 6. Verify Version Was Bumped

**IMPORTANT:** Before proceeding, verify that the version has actually been bumped since the last release.

Get the last release tag:

```bash
cd <PRIVATE_REPO>
git describe --tags --abbrev=0
```

Compare the tag version (e.g., `v4.0.3`) with the VERSION file (e.g., `4.0.4`).

- **If they match, STOP.** The VERSION file has not been bumped. Do not create a release without a version bump.
- **If they differ, proceed** with the release process.

### 7. Create Release Branch

Create a new branch named `release/vX.Y.Z` (using the version from step 4):

```bash
cd <PRIVATE_REPO>
git checkout -b release/vX.Y.Z
```

### 8. Generate Commit Message with Changelog

The commit message must enumerate all changes since the last version tag. Find the previous tag:

```bash
cd <PRIVATE_REPO>
git describe --tags --abbrev=0
```

Generate the changelog for each board:

#### main_board changes (in public repo):

```bash
cd <PUBLIC_REPO>
git log <PREVIOUS_TAG>..HEAD --oneline -- main_board/
```

#### sec_board changes (in private repo):

```bash
cd <PRIVATE_REPO>
git log <PREVIOUS_TAG>..HEAD --oneline -- sec_board/
```

### 9. Create the Commit

Stage and commit the changes with a detailed message. Do not include `#xxx` PR numbers, or replace with full URL (markdown) based on remote within each repository.

```bash
cd <PRIVATE_REPO>
git add west.yml
git commit -m "$(cat <<'EOF'
release: bump to vX.Y.Z

Update orb-firmware revision to <GIT_REV>.

## main_board changes

- <list changes from main_board>

## sec_board changes

- <list changes from sec_board>

EOF
)"
```

### Private Release File Locations

| File                  | Location                    | Purpose                                              |
| --------------------- | --------------------------- | ---------------------------------------------------- |
| Public manifest       | `<PUBLIC_REPO>/west.yml`    | West manifest for public builds                      |
| Private manifest      | `<PRIVATE_REPO>/west.yml`   | West manifest with orb-firmware revision             |
| Version file          | `<PUBLIC_REPO>/VERSION`     | Contains VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH |
| Main board source     | `<PUBLIC_REPO>/main_board/` | Main MCU firmware source code                        |
| Security board source | `<PRIVATE_REPO>/sec_board/` | Security MCU firmware source code                    |

### Private Release Example

For version 4.0.4:

1. Pull main in `<PUBLIC_REPO>`
2. Get revision: `00ec11452548d00189317d1df85e023d23c9c4e5`
3. Checkout and pull main in `<PRIVATE_REPO>`
4. Update `<PRIVATE_REPO>/west.yml` revision field
5. Read `<PUBLIC_REPO>/VERSION`: 4.0.4
6. Verify version bumped: last tag is `v4.0.3`, VERSION is `4.0.4` → proceed
7. Create branch: `release/v4.0.4`
8. Generate changelog for `main_board` and `sec_board`
9. Commit with changelog
