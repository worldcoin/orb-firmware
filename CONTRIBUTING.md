# Contributing

Most of our contributing guidelines are taken from the Zephyr project, so please refer to the [Zephyr contributing
guidelines](https://docs.zephyrproject.org/latest/contribute/guidelines.html) for more information and help.

## Development

### Code style

Code formatting is automated and **checked in CI** using the `pre-commit` tool, so it behooves you to install and
configure
it now. Install `pre-commit` using `pip3 install pre-commit` or it might already be installed in
the [conda environment](utils/env/environment.yml).

Configure `pre-commit` using the config in the repo:

```shell
cd "$REPO_DIR"/orb/public
pre-commit install -c utils/format/pre-commit-config.yaml --hook-type commit-msg
```

#### Check Formatting

Manually:

```shell
cd "$REPO_DIR"/orb/public && pre-commit run --all-files --config utils/format/pre-commit-config.yaml
```

Using Docker:

```shell
cd "$REPO_DIR"/orb/public/utils/docker
make format
```

## Contribution workflow

### New feature

Create a feature branch by prefixing with your name `name/`.

### Commit message guidelines

[`gitlint`](https://jorisroovers.com/gitlint/latest/) is used to enforce commit message style. Commit message guidelines
are directly from the Zephyr
project ([Zephyr guidelines](https://docs.zephyrproject.org/latest/contribute/guidelines.html#commit-message-guidelines)).

Install `gitlint` using `pip3 install gitlint`, or it might already be installed in
the [conda environment](utils/env/environment.yml).
