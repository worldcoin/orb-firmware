# Proto2 Main MCU

## Getting started

### Download the source

Go to [the workspace repository](https://github.com/worldcoin/proto2-firmware) 
and follow the instructions in the README.md.

Once downloaded, West will checkout the repository in the `app_main` directory 
with a branch called `manifest-rev`. If you want to work on the repo, make sure 
to checkout the `main` branch and branch from there.

```shell
git remote add origin git@github.com:worldcoin/proto2-main-mcu.git
git fetch
git checkout main

# From there, create a new branch
```

### Compilation

Compile the app:

```shell
west build -b [stm32g484_eval | orb]
```

### Installation

Flash the target:

```shell
west flash
```

