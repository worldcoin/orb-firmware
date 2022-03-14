board_runner_args(pyocd "--target=stm32g474vetx")
board_runner_args(jlink "--device=STM32G474VE" "--speed=4000" "--reset-after-load")

include(${ZEPHYR_BASE}/boards/common/pyocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
