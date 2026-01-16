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
│   └── public/
│       ├── VERSION      # Firmware version (MAJOR.MINOR.PATCH)
│       ├── main_board/  # Main MCU firmware source
│       │   └── src/
│       │       └── optics/  # Optics-related modules
│       ├── west.yml     # West manifest
│       └── zephyr/      # Zephyr module configuration
├── bootloader/          # Bootloader code
├── modules/             # Additional modules
└── zephyr/              # Zephyr RTOS
```
