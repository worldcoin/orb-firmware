# New firmware version

Follow these steps to generate a new firmware version without manual prompts:

1. Checkout `main` branch and pull.
2. Determine new firmware version to release
   - Get version (semver) from `VERSION` file
   - Increment `PATCH`
3. Create branch `codex/mcu-fw-M.m.p`, with `M.m.p` being `MAJOR.MINOR.PATCH`
4. Set new version in `VERSION`
5. Create a new signed commit with the template:

   ```
   build(release): mcu fw version <M.m.p>

   <short summary of changes since last commit containing `build(release)`>
   ```

   - allow me to review the commit message

6. Push branch to remote `worldcoin`
