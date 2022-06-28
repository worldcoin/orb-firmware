if (CONFIG_WS2812_PWM_STM32)
    zephyr_library_amend()

    zephyr_library_sources_ifdef(CONFIG_WS2812_PWM_STM32 ws2812_pwm_stm32.c)
    zephyr_library_compile_options(-Wextra ${EXTRA_COMPILE_FLAGS})
endif()
