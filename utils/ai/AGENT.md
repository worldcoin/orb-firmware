# AGENT.md

This file provides guidance to AI coding assistants when working with the orb-firmware repository.

## Overview

This repository contains the public MCU firmware for the Worldcoin Orb, built with Zephyr RTOS.

## Prime Directive

- Keep changes small and compiling.
- Prefer existing patterns; do not introduce new dependencies without asking.
- If unsure, propose 2 options with tradeoffs (don't guess).

## Build System

- **RTOS**: Zephyr
- **Build Tool**: West (CMake under the hood)
- **Language**: C
- **Structure**: West workspace with root in a parent directory (use `west topdir` to obtain path)

## Key Folders

- `orb/public/main_board/` - Main MCU firmware source
- `boards/` - Board overlays / definitions (if present)
- `dts/` - Devicetree overlays (if present)
- `tests/` - Tests (if present)

## Environment Assumptions

- Commands are run from the west workspace (where `west topdir` works).
- Toolchain is installed and Zephyr env is set up.

## Build & Flash Commands

### Build (first time / new board)

```bash
west build -b <BOARD> -d build/<BOARD> app
```

### Incremental build

```bash
west build -d build/<BOARD>
```

### Clean rebuild (use when build system is "stuck")

```bash
west build -p always -d build/<BOARD>
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

Preferred: Twister for repo tests

```bash
west twister -T tests -p <PLATFORM> --inline-logs -v
```

Twister is Zephyr's test runner. See [Twister documentation](https://docs.zephyrproject.org/latest/develop/test/twister.html).

## Style & Quality Gates

- If the repo defines formatting/lint scripts, run them.
- Don't reformat unrelated code.

## Output Expectations

- **Before coding**: A short plan (3–6 steps).
- **After coding**: What changed + where, and the exact build/test commands you ran + results.

---

## Version Bump Process

When asked to "bump the version", "create a new version", or "prepare a release", follow these steps:

### 1. Ensure on Latest Main Branch

```bash
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

The `VERSION` file contains:

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
orb-firmware/
├── AGENT.md             # AI assistant instructions (this file)
├── orb/
│   ├── public/
│   │   ├── VERSION      # Firmware version (MAJOR.MINOR.PATCH)
│   │   ├── main_board/  # Main MCU firmware source
│   │   │   └── src/
│   │   │       └── optics/  # Optics-related modules
│   │   ├── west.yml     # West manifest
│   │   └── zephyr/      # Zephyr module configuration
│   └── private/         # Internal only (if available)
│       ├── west.yml     # Private west manifest
│       └── sec_board/   # Security MCU firmware
├── bootloader/          # Bootloader code
├── modules/             # Additional modules
└── zephyr/              # Zephyr RTOS
```

---

## Private Repository Release Process (Internal Only)

> **Note:** This section only applies when `orb/private/` is available.

When creating a new version in `orb/private/`, follow these steps:

### 1. Update Public Repository Reference

First, pull the latest `main` branch in `orb/public/`:

```bash
cd orb/public
git checkout main
git pull
```

### 2. Get the Git Revision

Get the current git revision hash from `orb/public/`:

```bash
cd orb/public
git rev-parse HEAD
```

### 3. Update west.yml

Update the `revision` field for `orb-firmware` in `orb/private/west.yml` with the git revision obtained in step 2:

```yaml
projects:
  - name: orb-firmware
    revision: <NEW_GIT_REVISION_HERE> # Update this line
    import: ...
```

### 4. Determine Version Number

Read the version from `orb/public/VERSION`:

```bash
cat orb/public/VERSION
```

### 5. Verify Version Was Bumped

**IMPORTANT:** Before proceeding, verify that the version has actually been bumped since the last release.

Get the last release tag:

```bash
cd orb/private
git describe --tags --abbrev=0
```

Compare the tag version (e.g., `v4.0.3`) with the VERSION file (e.g., `4.0.4`).

- **If they match, STOP.** The VERSION file has not been bumped. Do not create a release without a version bump.
- **If they differ, proceed** with the release process.

### 6. Create Release Branch

Create a new branch named `release/vX.Y.Z` (using the version from step 4):

```bash
cd orb/private
git checkout -b release/vX.Y.Z
```

### 7. Generate Commit Message with Changelog

The commit message must enumerate all changes since the last version tag. Find the previous tag:

```bash
cd orb/private
git describe --tags --abbrev=0
```

Generate the changelog for each board:

#### main_board changes (in orb/public):

```bash
cd orb/public
git log <PREVIOUS_TAG>..HEAD --oneline -- main_board/
```

#### sec_board changes (in orb/private):

```bash
cd orb/private
git log <PREVIOUS_TAG>..HEAD --oneline -- sec_board/
```

### 8. Create the Commit

Stage and commit the changes with a detailed message. Do not include #xxx PR numbers, or replace with full URL (markdown) based
on remote within each repositories.

```bash
cd orb/private
git add west.yml
git commit -m "$(cat <<'EOF'
release: bump to vX.Y.Z

Update orb-firmware revision to <GIT_REV>.

## main_board changes

- <list changes from orb/public/main_board>

## sec_board changes

- <list changes from orb/private/sec_board>

EOF
)"
```

### Private Release File Locations

| File                  | Path                     | Purpose                                              |
| --------------------- | ------------------------ | ---------------------------------------------------- |
| Public manifest       | `orb/public/west.yml`    | West manifest for public builds                      |
| Private manifest      | `orb/private/west.yml`   | West manifest with orb-firmware revision             |
| Version file          | `orb/public/VERSION`     | Contains VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH |
| Main board source     | `orb/public/main_board/` | Main MCU firmware source code                        |
| Security board source | `orb/private/sec_board/` | Security MCU firmware source code                    |

### Private Release Example

For version 4.0.4:

1. Pull main in `orb/public/`
2. Get revision: `00ec11452548d00189317d1df85e023d23c9c4e5`
3. Update `orb/private/west.yml` revision field
4. Read `orb/public/VERSION`: 4.0.4
5. Verify version bumped: last tag is `v4.0.3`, VERSION is `4.0.4` → proceed
6. Create branch: `release/v4.0.4`
7. Generate changelog for `main_board` and `sec_board`
8. Commit with changelog
