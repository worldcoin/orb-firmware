# ------------------------------------------------
# Application Makefile (based on gcc)
# ------------------------------------------------

TARGET = orb_app

# debug build?
DEBUG = 1
# optimization
OPT = -Og

# Build path
BUILD_DIR = build
SDK_ROOT = ../../../..

# C sources
C_SOURCES =  \
${SDK_ROOT}/orb/app/errors.c \
${SDK_ROOT}/orb/app/main.c \
${SDK_ROOT}/orb/app/freertos.c \
./platform/stm32f3xx_it.c \
./platform/stm32f3xx_hal_msp.c \
./platform/stm32f3xx_hal_timebase_tim.c \
./platform/system_stm32f3xx.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_adc.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_adc_ex.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_rcc.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_rcc_ex.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_gpio.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_dma.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_cortex.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_pwr.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_pwr_ex.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_flash.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_flash_ex.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_i2c.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_i2c_ex.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_exti.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_i2s.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_i2s_ex.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_spi.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_spi_ex.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_tim.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_tim_ex.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_tsc.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_uart.c \
${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Src/stm32f3xx_hal_uart_ex.c \
${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/croutine.c \
${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/event_groups.c \
${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/list.c \
${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/queue.c \
${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c \
${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/tasks.c \
${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/timers.c \
${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS/cmsis_os.c \
${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c \
${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c

# ASM sources
ASM_SOURCES =  \
./platform/startup_stm32f302xc.s


PREFIX = arm-none-eabi-
# The gcc compiler bin path can be either defined in make command via GCC_PATH variable (> make GCC_PATH=xxx)
# either it can be added to the PATH environment variable.
ifdef GCC_PATH
CC = $(GCC_PATH)/$(PREFIX)gcc
AS = $(GCC_PATH)/$(PREFIX)gcc -x assembler-with-cpp
CP = $(GCC_PATH)/$(PREFIX)objcopy
SZ = $(GCC_PATH)/$(PREFIX)size
else
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size
endif
HEX = $(CP) -O ihex
BIN = $(CP) -O binary -S

# cpu
CPU = -mcpu=cortex-m4
# fpu
FPU = -mfpu=fpv4-sp-d16
# float-abi
FLOAT-ABI = -mfloat-abi=hard
# mcu
MCU = $(CPU) -mthumb $(FPU) $(FLOAT-ABI)

# macros for gcc
# AS defines
AS_DEFS = 

# C defines
C_DEFS =  \
-DUSE_HAL_DRIVER \
-DSTM32F302xC

# AS includes
AS_INCLUDES =  \
-I${SDK_ROOT}/orb/app/include

# C includes
C_INCLUDES =  \
-I${SDK_ROOT}/orb/app/include \
-I./config \
-I${SDK_ROOT}/orb/common/include \
-I${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Inc \
-I${SDK_ROOT}/components/Drivers/STM32F3xx_HAL_Driver/Inc/Legacy \
-I${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/include \
-I${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS \
-I${SDK_ROOT}/components/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F \
-I${SDK_ROOT}/components/Drivers/CMSIS/Device/ST/STM32F3xx/Include \
-I${SDK_ROOT}/components/Drivers/CMSIS/Include

# compile gcc flags
ASFLAGS = $(MCU) $(AS_DEFS) $(AS_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

CFLAGS = $(MCU) $(C_DEFS) $(C_INCLUDES) $(OPT) -Wall -fdata-sections -ffunction-sections

ifeq ($(DEBUG), 1)
CFLAGS += -g -gdwarf-2
endif

# Generate dependency information
CFLAGS += -MMD -MP -MF"$(@:%.o=%.d)"

# link script
LDSCRIPT = STM32F302VBTx_FLASH.ld

# libraries
LIBS = -lc -lm -lnosys 
LIBDIR = 
LDFLAGS = $(MCU) -specs=nano.specs -T$(LDSCRIPT) $(LIBDIR) $(LIBS) -Wl,-Map=$(BUILD_DIR)/$(TARGET).map,--cref -Wl,--gc-sections

# default action: build all
all: $(BUILD_DIR)/$(TARGET).elf $(BUILD_DIR)/$(TARGET).hex $(BUILD_DIR)/$(TARGET).bin


# list of objects
OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))
# list of ASM program objects
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
vpath %.s $(sort $(dir $(ASM_SOURCES)))

$(BUILD_DIR)/%.o: %.c Makefile | $(BUILD_DIR)
	@echo "Compiling $<"
	@$(CC) -c $(CFLAGS) -Wa,-a,-ad,-alms=$(BUILD_DIR)/$(notdir $(<:.c=.lst)) $< -o $@

$(BUILD_DIR)/%.o: %.s Makefile | $(BUILD_DIR)
	@echo "Assembling $<"
	@$(AS) -c $(CFLAGS) $< -o $@

$(BUILD_DIR)/$(TARGET).elf: $(OBJECTS) Makefile
	@echo "Linking $@"
	@$(CC) $(OBJECTS) $(LDFLAGS) -o $@
	@$(SZ) $@

$(BUILD_DIR)/%.hex: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	@$(HEX) $< $@
	@echo "Hex file: $@"

$(BUILD_DIR)/%.bin: $(BUILD_DIR)/%.elf | $(BUILD_DIR)
	@$(BIN) $< $@
	@echo "Bin file: $@"

$(BUILD_DIR):
	mkdir $@		

# if running from Docker, make sure we always regenerated version.h
ifeq ($(DOCKER_RUN),1)

prepare_version:
	@echo Preparing version from Docker
	python $(SDK_ROOT)/utils/version/gen_version.py -f ./version.ini -i ./config/version.h

else

# Get the file in the branch referencing the commit SHA
current := $(shell cut -c6- $(SDK_ROOT)/.git/HEAD)
# version.h regenerated only if new commit or version.ini has changed.
./config/version.h: $(SDK_ROOT)/.git/${current} ./version.ini
	@echo Preparing version $(app_version_script)
	python $(SDK_ROOT)/utils/version/gen_version.py -f ./version.ini -i ./config/version.h

prepare_version: ./config/version.h

endif # DOCKER_RUN

app_debug: -DCONFIG_LOG_LEVEL=4
app_debug: prepare_version $(BUILD_DIR)/$(TARGET).hex

clean:
	-rm ./config/version.h
	-rm -fR $(BUILD_DIR)

# dependencies
-include $(wildcard $(BUILD_DIR)/*.d)
