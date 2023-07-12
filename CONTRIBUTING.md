# Contributing

## Development

### Code style

Code formatting is automated and checked in CI using the `pre-commit` tool, so it behooves you to install and configure
it now. Install `pre-commit` using `pip3 install pre-commit` or through
the [conda environment](utils/env/environment.yml).

Configure `pre-commit` using the config in the repo:

```shell
cd "$REPO_DIR"/orb && pre-commit install -c utils/format/pre-commit-config.yaml
```

#### Check Formatting

Manually:

```shell
cd "$REPO_DIR"/orb && pre-commit run --all-files --config utils/format/pre-commit-config.yaml
```

Using Docker:

```shell
cd "$REPO_DIR"/orb/utils/docker
make format
```

## Contribution workflow

### New feature

Create a feature branch by prefixing with your name `name/`.

### Pull request

Use the linear ID `O-xxx` into the Pull Request description:

```
Fixes <issue number>
```

### Release process

Follow these steps:

- Rename `main` branch to `prod`
- Bump versions in the [VERSION file](VERSION)
- Tag new versioned release using semver. As an example: `git tag -a v0.6.2 -m "Release v0.6.2"`. Make
  sure to push the tags (`git push --follow-tags`). This will trigger CI to create a new release on GitHub.
- Any modification to the release has to be done using a new Pull Request to `main`
- Pull those changes into the release branch (`cherry-pick`)
- Never merge release branch to `main`
