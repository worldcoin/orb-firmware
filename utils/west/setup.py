# Copyright (c) 2026 Tools For Humanity
# SPDX-License-Identifier: Apache-2.0

"""West extension command for setting up the workspace with common files."""

import shutil
import subprocess
import sys
from pathlib import Path

from west import log
from west.commands import WestCommand


class Setup(WestCommand):
    """West command to set up workspace with AI instructions and environment files."""

    def __init__(self):
        super().__init__(
            "setup",
            "set up workspace with common files",
            "Set up the west workspace with AI coding assistant instruction files "
            "(AGENT.md, CLAUDE.md, etc.) and environment configuration (.envrc).",
            accepts_unknown_args=False,
        )

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(
            self.name,
            help=self.help,
            description=self.description,
        )

        # Subcommands
        subparsers = parser.add_subparsers(
            dest="subcommand",
            help="sub-command help",
        )

        # Install subcommand (default behavior)
        install_parser = subparsers.add_parser(
            "install",
            help="Install setup files to workspace root (default)",
        )
        install_parser.add_argument(
            "--dry-run",
            action="store_true",
            help="Show what would be copied without actually copying files.",
        )
        install_parser.add_argument(
            "--force",
            "-f",
            action="store_true",
            help="Overwrite existing files without prompting.",
        )

        # Clean subcommand
        clean_parser = subparsers.add_parser(
            "clean",
            help="Remove installed setup files from workspace root",
        )
        clean_parser.add_argument(
            "--dry-run",
            action="store_true",
            help="Show what would be removed without actually removing files.",
        )

        # Also add flags to main parser for default behavior (no subcommand)
        parser.add_argument(
            "--dry-run",
            action="store_true",
            help="Show what would be copied without actually copying files.",
        )
        parser.add_argument(
            "--force",
            "-f",
            action="store_true",
            help="Overwrite existing files without prompting.",
        )

        return parser

    def do_run(self, args, unknown_args):
        if args.subcommand == "clean":
            self._do_clean(args)
        else:
            # Default to install (covers both 'install' subcommand and no subcommand)
            self._do_install(args)

    def _get_installed_files(self):
        """Return list of files that get installed to workspace root."""
        return [
            "AGENT.md",
            "CLAUDE.md",
            ".github/copilot-instructions.md",
            ".envrc",
        ]

    def _check_direnv(self):
        """Check if direnv is installed and provide installation instructions if not."""
        try:
            result = subprocess.run(
                ["direnv", "--version"],
                capture_output=True,
                text=True,
                check=False,
            )
            if result.returncode == 0:
                version = result.stdout.strip()
                log.dbg(f"direnv found: {version}")
                return True
        except FileNotFoundError:
            pass

        # direnv not found - show installation instructions
        log.wrn("direnv is not installed.")
        log.wrn("")
        log.wrn("direnv is recommended for automatic environment setup.")
        log.wrn("The .envrc file will be installed, but won't work without direnv.")
        log.wrn("")
        log.wrn("To install direnv:")
        log.wrn("")

        # Detect platform and show appropriate instructions
        if sys.platform == "darwin":
            log.wrn("  macOS (Homebrew):")
            log.wrn("    brew install direnv")
            log.wrn("")
            log.wrn("  macOS (Nix):")
            log.wrn("    nix-env -i direnv")
        elif sys.platform.startswith("linux"):
            log.wrn("  Ubuntu/Debian:")
            log.wrn("    sudo apt install direnv")
            log.wrn("")
            log.wrn("  Fedora:")
            log.wrn("    sudo dnf install direnv")
            log.wrn("")
            log.wrn("  Arch Linux:")
            log.wrn("    sudo pacman -S direnv")
            log.wrn("")
            log.wrn("  Nix:")
            log.wrn("    nix-env -i direnv")
        else:
            log.wrn("  See: https://direnv.net/docs/installation.html")

        log.wrn("")
        log.wrn("After installing, add the hook to your shell:")
        log.wrn('  bash:  eval "$(direnv hook bash)"  # add to ~/.bashrc')
        log.wrn('  zsh:   eval "$(direnv hook zsh)"   # add to ~/.zshrc')
        log.wrn(
            "  fish:  direnv hook fish | source   # add to ~/.config/fish/config.fish"
        )
        log.wrn("")
        log.wrn("See: https://direnv.net/docs/hook.html")
        log.wrn("")

        return False

    def _check_nix(self):
        """Check if nix is installed and provide installation instructions if not."""
        try:
            result = subprocess.run(
                ["nix", "--version"],
                capture_output=True,
                text=True,
                check=False,
            )
            if result.returncode == 0:
                version = result.stdout.strip()
                log.dbg(f"nix found: {version}")
                return True
        except FileNotFoundError:
            pass

        # nix not found - show installation instructions
        log.wrn("nix is not installed.")
        log.wrn("")
        log.wrn("nix is recommended for reproducible development environments.")
        log.wrn("The .envrc file uses nix flakes for environment setup.")
        log.wrn("")
        log.wrn("To install nix (recommended: Determinate Systems installer):")
        log.wrn("")
        log.wrn("  curl --proto '=https' --tlsv1.2 -sSf -L \\")
        log.wrn("    https://install.determinate.systems/nix | sh -s -- install")
        log.wrn("")
        log.wrn("Or official installer:")
        log.wrn("  sh <(curl -L https://nixos.org/nix/install) --daemon")
        log.wrn("")
        log.wrn("See: https://nixos.org/download.html")
        log.wrn("")

        return False

    def _do_install(self, args):
        """Install setup files to workspace root."""
        # Check for required tools
        has_direnv = self._check_direnv()
        has_nix = self._check_nix()

        if not has_direnv or not has_nix:
            log.inf("Continuing with setup anyway...")
            log.inf("")

        # Get workspace root (where west topdir points)
        workspace_root = Path(self.topdir)

        # Source directory is where this script lives (utils/west/)
        # We need to go up to the repo root (orb/public/)
        utils_dir = Path(__file__).parent.parent
        repo_root = utils_dir.parent

        copied_count = 0
        skipped_count = 0

        # --- AI instruction files ---
        ai_source = utils_dir / "ai" / "AGENT.md"
        ai_destinations = [
            "AGENT.md",
            "CLAUDE.md",
            ".github/copilot-instructions.md",
        ]

        if ai_source.exists():
            for dst_rel in ai_destinations:
                dst = workspace_root / dst_rel

                # Ensure parent directory exists
                if not args.dry_run:
                    dst.parent.mkdir(parents=True, exist_ok=True)

                copied = self._copy_file(ai_source, dst, args.dry_run, args.force)
                if copied:
                    copied_count += 1
                else:
                    skipped_count += 1
        else:
            log.wrn(f"AI source file not found: {ai_source}")

        # --- Environment file ---
        envrc_source = repo_root / ".envrc.example"
        envrc_dest = workspace_root / ".envrc"

        # Calculate relative path from workspace root to repo root
        repo_rel_path = repo_root.relative_to(workspace_root)

        if envrc_source.exists():
            copied = self._copy_envrc(
                envrc_source, envrc_dest, repo_rel_path, args.dry_run, args.force
            )
            if copied:
                copied_count += 1
            else:
                skipped_count += 1
        else:
            log.wrn(f"Environment file not found: {envrc_source}")

        # Summary
        if args.dry_run:
            log.inf(f"Dry run complete. Would copy {copied_count} file(s).")
        else:
            log.inf(f"Done. Copied {copied_count} file(s), skipped {skipped_count}.")

        # Remind user to allow direnv if it's installed
        if has_direnv and not args.dry_run and copied_count > 0:
            log.inf("")
            log.inf("Next step: Run 'direnv allow' to activate the environment.")

    def _do_clean(self, args):
        """Remove installed setup files from workspace root."""
        workspace_root = Path(self.topdir)

        removed_count = 0
        skipped_count = 0

        for filename in self._get_installed_files():
            filepath = workspace_root / filename
            if filepath.exists():
                if args.dry_run:
                    log.inf(f"Would remove: {filepath}")
                    removed_count += 1
                else:
                    filepath.unlink()
                    log.inf(f"Removed: {filepath}")
                    removed_count += 1
            else:
                log.dbg(f"Skipping (not found): {filepath}")
                skipped_count += 1

        # Summary
        if args.dry_run:
            log.inf(f"Dry run complete. Would remove {removed_count} file(s).")
        else:
            log.inf(f"Done. Removed {removed_count} file(s), skipped {skipped_count}.")

    def _copy_envrc(
        self, src: Path, dst: Path, repo_rel_path: Path, dry_run: bool, force: bool
    ) -> bool:
        """
        Copy .envrc file, transforming paths for the workspace root.

        Paths like './utils/env' become './<repo_rel_path>/utils/env'.
        """
        content = src.read_text()
        # Transform relative paths: ./utils/ -> ./<repo_rel_path>/utils/
        transformed = content.replace("./utils/", f"./{repo_rel_path}/utils/")

        if dst.exists() and not force:
            if dst.read_text() == transformed:
                log.dbg(f"Skipping (identical): {dst}")
                return False
            else:
                log.wrn(f"File exists and differs: {dst}")
                log.wrn("  Use --force to overwrite.")
                return False

        if dry_run:
            log.inf(f"Would copy (with path transform): {src} -> {dst}")
        else:
            dst.write_text(transformed)
            log.inf(f"Copied: {src.name} -> {dst} (paths transformed)")

        return True

    def _copy_file(self, src: Path, dst: Path, dry_run: bool, force: bool) -> bool:
        """
        Copy a file from src to dst.

        Returns True if file was (or would be) copied, False if skipped.
        """
        if dst.exists() and not force:
            # Check if files are identical
            if dst.read_bytes() == src.read_bytes():
                log.dbg(f"Skipping (identical): {dst}")
                return False
            else:
                log.wrn(f"File exists and differs: {dst}")
                log.wrn("  Use --force to overwrite.")
                return False

        if dry_run:
            log.inf(f"Would copy: {src} -> {dst}")
        else:
            shutil.copy2(src, dst)
            log.inf(f"Copied: {src.name} -> {dst}")

        return True
