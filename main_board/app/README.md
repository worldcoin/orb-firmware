# Proto2 Main MCU application

## Getting started

### Download the source

First, follow the instructions in the [top-level README.md](https://github.com/worldcoin/proto2-mcu/tree/support_evt#readme).

Once downloaded, West will checkout this repository in the `orb` directory
with a branch called `manifest-rev`. If you want to work on the repo, make sure
to checkout the `main` branch and branch from there.

```shell
cd $REPO_DIR"/orb
git remote add origin git@github.com:worldcoin/proto2-mcu.git
git fetch
git checkout main

# From there, create a new branch
```

### Compiling and Flashing

You have two options: use the Makefile w/ Docker or do it manually.

> ⚠️ Make sure to have the [bootloader flashed](../../bootloader_main/README.md) before.

#### With Makefile

- Go to `${REPO_DIR}/orb/utils/docker`.
- Run `make help` to see all options

To Build: `make mcu-build`

To Flash: `make mcu-flash`

#### Manually

Make sure you are in `"$REPO_DIR"/orb/main_board/app/` directory.
Compile the app:

```shell
# 'west build' defaults to mcu_main_v31
west build -b [stm32g484_eval | mcu_main_v30 | mcu_main_v31]
```

Flash the app:

If not in the Docker container:

```shell
west flash
```

If in the Docker container:

```shell
# Note: sometimes you must have the debugger already plugged into your computer _before_ launching Docker in order for Docker to see it.
su-exec root west flash
```

## Misc Documentation

### Timer alloctions:

| Peripheral                     | Pin  | Timer | Timer Channel |
| ------------------------------ | ---- | ----- | ------------- |
| 740nm                          | PB0  | TIM1  | CH2N          |
| 850nm Left                     | PB14 | TIM15 | CH1           |
| 850nm Right                    | PB15 | TIM15 | CH2           |
| 940nm Left                     | PE2  | TIM3  | CH1           |
| 940nm Right                    | PE5  | TIM3  | CH4           |
| Main fan PWM                   | PB2  | TIM5  | CH1           |
| Main fan tach                  | PD7  | TIM2  | CH3           |
| Aux fan PWM                    | PA1  | TIM5  | CH2           |
| Aux fan tach                   | PA12 | TIM4  | CH2           |
| CAM 0 (IR eye camera) trigger  | PC8  | TIM8  | CH3           |
| CAM 2 (IR face camera) trigger | PC9  | TIM8  | CH4           |
| CAM 3 (IR face camera) trigger | PC6  | TIM8  | CH1           |
| User LEDs                      | PE3  | TIM20 | CH2           |
