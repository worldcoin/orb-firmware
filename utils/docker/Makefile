DOCKER_VER := 2
DOCKER_TAG ?= ghcr.io/worldcoin/mcu-zephyr-build:$(DOCKER_VER)

ifneq ($(filter help all build,$(MAKECMDGOALS)),)

.PHONY: help
help:
	@:$(info Needed env or cmd-line variables:)
	@:$(info )
	@:$(info REPO_DIR = absolute path to top-level west workspace.)
	@:$(info )
	@:$(info Optional env or cmd-line variables:)
	@:$(info )
	@:$(info MCU_DIR = path relative to $$REPO_DIR to the MCU you want to build (defaults to orb/main_board/app).)
	@:$(info BOARD   = board option to give to `west build -b`.)
	@:$(info )
	@:$(info TARGETS:)
	@:$(info )
	@:$(info all/build : build the Docker image.)
	@:$(info shell     : drop into the Docker container.)
	@:$(info clean     : clean the Docker image.)
	@:$(info )
	@:$(info mcu-build : build MCU code.)
	@:$(info mcu-flash : flash MCU code (also builds if necessary).)
	@:$(info mcu-clean : clean MCU code.)
	@:$(info )
	@:$(info Examples:)
	@:$(info )
	@:$(info make mcu-build REPO_DIR=/home/pdietl/firmware)
	@:$(info make mcu-build REPO_DIR=/home/pdietl/firmware MCU_DIR=orb/main_board/app BOARD=mcu_main_v30)

.PHONY: all build
all build: .docker_image_built

.docker_image_built: Dockerfile Makefile
	docker build . -t $(DOCKER_TAG)
	touch $@

else

ifndef REPO_DIR
$(error REPO_DIR must be given in env or cmd line!)
endif

ifndef MCU_DIR
MCU_DIR := $(REPO_DIR)/orb/main_board/app
else
override MCU_DIR := $(REPO_DIR)/$(MCU_DIR)
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
		$(DOCKER_TAG)

.PHONY: shell
shell:
	$(DOCKER_CMD) /bin/bash

.PHONY: mcu-build
mcu-build:
	$(DOCKER_CMD) /bin/bash -c -- \
		'cd $(MCU_DIR) && west build $(BOARD_CMD)'

.PHONY: mcu-flash
mcu-flash:
	$(DOCKER_CMD) /bin/bash -c -- \
		'cd $(MCU_DIR) && west build $(BOARD_CMD) && su-exec root west flash'

.PHONY: mcu-clean
mcu-clean:
	cd $(MCU_DIR) && rm -rf build

.PHONY: format
format:
	$(DOCKER_CMD) /bin/bash -c -- \
		'cd orb && pre-commit run --all-files --config utils/format/pre-commit-config.yaml'

.PHONY: clean
clean:
	$(RM) .docker_image_built

endif # ($(MAKECMDGOALS),help)
