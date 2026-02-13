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

Let's build the service image by default with warnings treated as errors to maintain code quality.

```bash
# Main board (public)
west build -b <BOARD> -d build/<BOARD> <PUBLIC_REPO>/main_board -- -DCMAKE_BUILD_TYPE="Service" -DEXTRA_COMPILE_FLAGS="-Werror"

# Example for Pearl main board
west build -b pearl_main -d build/pearl_main orb/public/main_board -- -DCMAKE_BUILD_TYPE="Service" -DEXTRA_COMPILE_FLAGS="-Werror"
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

## Hardware Lock (Mutex)

Multiple agents may work in parallel across git worktrees, but the physical Orb hardware is a **shared, exclusive resource**. Before flashing or opening a serial connection, the agent **must** acquire a lock. This prevents two agents from flashing simultaneously or reading garbled serial output.

### Lock File Location

The lock lives in the **shared git directory**, which is the same for every worktree:

```bash
LOCK_FILE="$(git rev-parse --git-common-dir)/orb-hardware.lock"
```

### Lock File Format

The lock file is a simple key=value text file:

```
AGENT=<identifier>
WORKTREE=<path to the worktree>
OPERATION=<flash|serial|flash+serial>
TIMESTAMP=<unix epoch seconds>
```

### Protocol

#### Acquiring the lock

1. Read `$LOCK_FILE`. If it does **not** exist → create it and proceed.
2. If it **does** exist, read `TIMESTAMP` and compute the age:
   ```bash
   age=$(( $(date +%s) - TIMESTAMP ))
   ```
3. If `age > 600` (10 minutes) → the lock is **stale**. Log a warning (`"Breaking stale lock held by <AGENT> since <TIMESTAMP>"`), remove the file, and create a new one.
4. If the lock is **not stale** → **do not proceed**. Inform the user that the hardware is in use by another agent and which operation it is performing.

#### Creating the lock

```bash
LOCK_FILE="$(git rev-parse --git-common-dir)/orb-hardware.lock"
cat > "$LOCK_FILE" <<EOF
AGENT=$(whoami)@$$
WORKTREE=$(pwd)
OPERATION=flash+serial
TIMESTAMP=$(date +%s)
EOF
```

#### Releasing the lock

Remove the lock file as soon as the flash + serial verification is complete:

```bash
rm -f "$LOCK_FILE"
```

> **Always release the lock**, even if an error occurs during flashing or verification. Wrap the flash/verify sequence so that the lock is released in all exit paths.

### Staleness Timeout

The timeout is **10 minutes (600 seconds)**. This is generous enough for a full build-flash-verify cycle. If an agent crashes or the user kills a session, the lock will automatically be considered stale by the next agent after 10 minutes.

### Rules

- **Acquire before**: `west flash`, opening any serial port via the MCP server.
- **Release after**: serial connection is closed and verification is complete.
- **Never** hold the lock during long operations that don't touch hardware (building, code edits, git operations).
- If the lock is held by someone else and is not stale, **wait and inform the user** — do not force-break a fresh lock.

---

## Linear Ticket Workflow

When working on a fix or feature linked to a Linear ticket:

1. **Create a worktree** using the Linear-suggested branch name (from the ticket details).
2. **Implement the changes**, build, and verify.
3. **Commit directly** with a proper Conventional Commits message and `Signed-off-by:` line. Do not ask for permission to commit - commit as soon as the change is verified.
4. **Acquire the hardware lock** before touching the Orb (see [Hardware Lock](#hardware-lock-mutex)). If the lock is held by another agent, inform the user and wait.
5. **Build and flash the target** to verify the firmware compiles cleanly and runs correctly on hardware. By default, build a Service image for `diamond_main` with warnings treated as errors. When the agent will verify autonomously via serial (step 6), add `-DCONFIG_INSTA_BOOT=y` so the MCU boots immediately after flash without waiting for a physical button press. Propose the commands for the user to review and execute:
   ```bash
   west build -b diamond_main -d build/diamond_main <PUBLIC_REPO>/main_board -- -DCMAKE_BUILD_TYPE="Service" -DEXTRA_COMPILE_FLAGS="-Werror" -DCONFIG_INSTA_BOOT=y
   west flash -d build/diamond_main
   ```
   > **Note:** `CONFIG_INSTA_BOOT=y` skips the power-button wait so the agent can read boot logs immediately. Omit it if the user will test manually or if the build is intended for production.
6. **Verify via serial** after flashing. If the Serial MCP server is available (see [Serial Communication](#serial-communication-mcp-server) below), open the main MCU serial port, read boot logs, and check for errors. Use CLI commands (e.g., `orb version`, `orb state`) to verify the new firmware is running correctly.
7. **Release the hardware lock** once serial verification is complete (or if any step fails).
8. **Ask the user** whether the branch should be pushed to the remote. Do not push without confirmation.

---

## Serial Communication (MCP Server)

If the **Serial MCP server** (`serial-mcp`) is available, use it to communicate with MCUs and the Jetson over serial after flashing. This enables reading boot logs, checking for errors, and sending CLI commands to verify firmware behavior on hardware.

### Serial Port Discovery

Use `list_ports` to discover available serial ports. When the **debug board** is plugged in, 4 serial devices appear as `/dev/tty[name][x]` with `x` from 0 to 3:

| Port Index | Device            | Purpose                 |
| ---------- | ----------------- | ----------------------- |
| 0          | `/dev/tty[name]0` | (reserved)              |
| 1          | `/dev/tty[name]1` | Jetson CLI              |
| 2          | `/dev/tty[name]2` | Main MCU CLI & logs     |
| 3          | `/dev/tty[name]3` | Security MCU CLI & logs |

> The exact device name prefix varies by host OS and debug board version (e.g., `/dev/cu.usbserial-xxx2` on macOS, `/dev/ttyUSB2` on Linux).

### Connection Settings

All serial ports use: **115200 baud, 8 data bits, no parity, 1 stop bit (8N1)**.

```
baud_rate: 115200
data_bits: 8
parity: none
stop_bits: 1
```

### Post-Flash Verification Workflow

After flashing new firmware, use the serial connection to verify it boots correctly:

1. **List ports** with `list_ports` to find available serial devices.
2. **Open** the main MCU serial port (index 2) with the settings above.
3. **Read** boot logs — look for error messages (`<err>` log level), assertion failures, or crash dumps.
4. **Send CLI commands** to verify the firmware is functional:
   - Press Enter first to get a shell prompt (`uart:~$`).
   - `orb version` — confirm the expected firmware version is running.
   - `orb state` — check hardware component states for anomalies.
   - `orb battery` — verify battery reporting is functional.
   - `orb stats` — check runner job statistics.
   - `orb ping_sec` — verify communication with the security MCU.
5. **Close** the connection when done.

If any errors are observed in the logs or commands fail unexpectedly, report the findings to the user before proceeding.

### Available CLI Commands

Commands are registered in source via `SHELL_CMD` / `SHELL_STATIC_SUBCMD_SET_CREATE` macros. To discover the latest commands and their usage, **read the CLI source files**:

- **Main MCU**: `<PUBLIC_REPO>/main_board/src/cli/cli.c` — all commands are under the `orb` shell prefix.
- **Security MCU**: `<PRIVATE_REPO>/sec_board/src/cli/cli.c` (if available).

Look for `SHELL_CMD(...)` entries and the corresponding `execute_*` functions for argument details and validation.

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
