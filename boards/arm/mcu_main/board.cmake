board_runner_args(pyocd "--target=stm32g474vetx")

include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
