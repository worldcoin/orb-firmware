# Use DOCKER_TAG if passed in from command line
DOCKER_IMAGE_NAME := orb-mcu-firmware-builder$(if $(DOCKER_TAG),:$(DOCKER_TAG),:local)

# Get the project root directory
WEST_YAML := $(shell west manifest --path)
PROJ_DIR := $(dir $(WEST_YAML))

ifneq ($(filter help all build,$(MAKECMDGOALS)),)

.PHONY: help
help:
	@:$(info Optional env or cmd-line variables:)
	@:$(info )
	@:$(info BOARD         = board option to give to `west build -b`.)
	@:$(info DOCKER_TAG    = Docker image tag to use.)
	@:$(info )
	@:$(info TARGETS:)
	@:$(info )
	@:$(info all/build     : build the Docker image.)
	@:$(info shell         : drop into the Docker container.)
	@:$(info clean         : clean the Docker image.)
	@:$(info )
	@:$(info mcu-build     : build MCU code.)
	@:$(info mcu-flash     : flash MCU code (also builds if necessary).)
	@:$(info mcu-clean     : clean MCU code.)
	@:$(info mcu-tests     : build and run MCU tests.)
	@:$(info )

.PHONY: all build
all build: .docker_image_built

.docker_image_built: Dockerfile Makefile
	docker build . -t $(DOCKER_IMAGE_NAME)
	touch $@

else

# Reminder: if the user calls `make VAR_FOO=baz`, the value provided for the variable
# in the argument to `make` will override a `:=` or `=` assignment in the Makefile.
# So, the following variables are overridable.
# Use `=?` if you want environment variables and command-line arguments to take
# precedence over an assignment in a Makefile.

WEST_TOPDIR := $(shell west topdir)
BOARD       := pearl_main

DOCKER_CMD := \
  docker run -ti --rm --privileged \
    -u $(shell id -u):$(shell id -g) \
    -v /etc/group:/etc/group:ro \
    -v /etc/passwd:/etc/passwd:ro \
    -v '$(HOME)'/.ssh:'$(HOME)'/.ssh:ro \
    -v '$(HOME)'/.ccache:'$(HOME)'/.ccache \
    -v '$(HOME)'/.cache:'$(HOME)'/.cache \
    -v '$(WEST_TOPDIR):$(WEST_TOPDIR)' \
    -e WEST_TOPDIR='$(WEST_TOPDIR)' \
    -e HOME='$(HOME)' \
    -w '$(WEST_TOPDIR)' \
    $(DOCKER_IMAGE_NAME)

SIG_KEY_FILE := $(PROJ_DIR)/utils/ota/root-ec-p256.pem
ENC_KEY_FILE := $(PROJ_DIR)/utils/ota/enc-ec256-pub.pem

# Usage: $(call relpath,ROOT,CHILD)
#  If CHILD is the same as ROOT, returns '.'
#  If CHILD is under ROOT, removes the ROOT prefix leaving a relative path
#  Otherwise, returns CHILD unchanged
define relpath
$(strip $(if $(filter $(abspath $1),$(abspath $2)),., \
  $(patsubst $(abspath $1)/%,%,$(abspath $2))))
endef

PROJ_DIR_REL_TOPDIR := $(call relpath,$(WEST_TOPDIR),$(PROJ_DIR))

REL_BOARD_DIR := $(PROJ_DIR_REL_TOPDIR)/main_board
REL_BUILD_DIR := $(REL_BOARD_DIR)/build 

BUILD_CMD := \
       cd $(WEST_TOPDIR) && \
       west build $(REL_BOARD_DIR) \
       --build-dir $(REL_BUILD_DIR) \
       --board $(BOARD) \
       --pristine \
       -- \
       -DCMAKE_BUILD_TYPE="Release" \
       -DEXTRA_COMPILE_FLAGS=-Werror \
       -DCONFIG_MCUBOOT_SIGNATURE_KEY_FILE=\"$(SIG_KEY_FILE)\" \
       -DCONFIG_MCUBOOT_ENCRYPTION_KEY_FILE=\"$(ENC_KEY_FILE)\"

$(SIG_KEY_FILE) $(ENC_KEY_FILE) &:
	cd $(PROJ_DIR)/utils/ota && ./generate_dev_keys

.PHONY: shell
shell:
	$(DOCKER_CMD) /bin/bash

mcu-build: $(SIG_KEY_FILE) $(ENC_KEY_FILE) 
	$(DOCKER_CMD) /bin/bash -c -- '$(BUILD_CMD)'

mcu-flash:
	$(DOCKER_CMD) /bin/bash -c -- '$(BUILD_CMD) && su-exec root west flash'

mcu-clean:
	cd $(WEST_TOPDIR) && rm -rf $(REL_BUILD_DIR)

mcu-tests:
	$(DOCKER_CMD) /bin/bash -c -- \
		'source $(WEST_TOPDIR)/zephyr/zephyr-env.sh && cd $(PROJ_DIR)/main_board && twister -T . -vv -c -p native_posix_64'

.PHONY: format
format:
	$(DOCKER_CMD) /bin/bash -c -- \
		'cd orb && pre-commit run --all-files --config utils/format/pre-commit-config.yaml'

.PHONY: clean
clean:
	$(RM) .docker_image_built

endif # ($(MAKECMDGOALS),help)
