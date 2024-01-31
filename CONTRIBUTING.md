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

### Commit message

Follow [Zephyr guidelines](https://docs.zephyrproject.org/latest/contribute/guidelines.html#commit-message-guidelines).
