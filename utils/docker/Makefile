# Use DOCKER_TAG if passed in from command line
DOCKER_IMAGE_NAME := orb-mcu-firmware-builder$(if $(DOCKER_TAG),:$(DOCKER_TAG),:local)

# Get the project root directory
WEST_YAML := $(shell west manifest --path)
PROJ_DIR := $(dirname $(WEST_YAML))

ifneq ($(filter help all build,$(MAKECMDGOALS)),)

.PHONY: help
help:
	@:$(info Optional env or cmd-line variables:)
	@:$(info )
	@:$(info BOARD   		= board option to give to `west build -b`.)
	@:$(info DOCKER_TAG   	= Docker image tag to use.)
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
	@:$(info )
	@:$(info Examples:)
	@:$(info )
	@:$(info make main_mcu-build WEST_TOPDIR=/home/$USER/firmware)
	@:$(info make main_mcu-tests WEST_TOPDIR=/home/$USER/firmware)

.PHONY: all build
all build: .docker_image_built

.docker_image_built: Dockerfile Makefile
	docker build . -t $(DOCKER_IMAGE_NAME)
	touch $@

else

ifndef WEST_TOPDIR
WEST_TOPDIR := $(shell west topdir)
endif

ifdef BOARD
BOARD_CMD := -b $(BOARD)
endif

DOCKER_CMD := \
	docker run -ti --rm --privileged \
		-u $(shell id -u):$(shell id -g) \
		-v /etc/group:/etc/group:ro \
		-v /etc/passwd:/etc/passwd:ro \
		-v '$(HOME)'/.ssh:'$(HOME)'/.ssh:ro \
		-v '$(HOME)'/.ccache:'$(HOME)'/.ccache \
		-v '$(HOME)'/.cache:'$(HOME)'/.cache \
		-v '$(REPO_DIR):$(REPO_DIR)' \
		-e REPO_DIR='$(REPO_DIR)' \
		-e HOME='$(HOME)' \
		-w '$(REPO_DIR)' \
		$(DOCKER_IMAGE_NAME)

.PHONY: shell
shell:
	$(DOCKER_CMD) /bin/bash

mcu-build:
	$(DOCKER_CMD) /bin/bash -c -- \
		'cd $(PROJ_DIR)/main_board && west build $(BOARD_CMD)'

mcu-flash:
	$(DOCKER_CMD) /bin/bash -c -- \
		'cd $(PROJ_DIR)/main_board && west build $(BOARD_CMD) && su-exec root west flash'

mcu-clean:
	cd $(WEST_TOPDIR)/orb/main_board && rm -rf build

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
