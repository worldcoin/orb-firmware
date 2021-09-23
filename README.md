# Proto2 Main MCU

## Getting started

### Download the source

1. Import the project using west. Specify a tag, branch or commit SHA using --mr to work with a specific version of the repository. We are going to use the main branch:
    ```shell
    # go to an empty directory or specify a target directory
    west init -m git@github.com:worldcoin/proto2-main-mcu.git [directory]
    ```

2. We will now get all the project repositories:
    ```shell
    west update
   ```
3. Export a Zephyr CMake package. This allows CMake to automatically load the boilerplate code required for building nRF Connect SDK applications:
    ```shell
    west zephyr-export
    ```

Your directory structure should now look similar to the one below:

```
proto2-main-mcu
 |___ .west
 |___ app_main
 |___ bootloader
 |___ modules
 |___ tools
 |___ utils
 |___ zephyr
```

### Compilation

Compile the app:

```shell
west build -b stm32g4_discovery -c orb
```

### Installation

Flash the target:

```shell
west flash
```

