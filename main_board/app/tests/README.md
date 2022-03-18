# Tests

Tests are run by compiling source for `native_posix_64`. It is thus recommended to run these tests in the Docker
container if your host is different.

From this directory, you can reach the docker image and run tests using the target `mcu-tests`:

```shell
make -C ../../../utils/docker/ mcu-tests REPO_DIR=/path/to/firmware MCU_DIR=orb/main_board/app BOARD=mcu_main_v30
```
