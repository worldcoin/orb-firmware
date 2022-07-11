DOCKER_VER := "sha256:dcc0e3cdb924cfb34da681909190a43a05128859da3e1b8e71210f1e33e58437"
DOCKER_TAG ?= ghcr.io/worldcoin/mcu-zephyr-build@$(DOCKER_VER)

ifneq ($(filter help all build,$(MAKECMDGOALS)),)

.PHONY: help
help:
	@:$(info Needed env or cmd-line variables:)
	@:$(info )
	@:$(info REPO_DIR = absolute path to top-level west workspace.)
	@:$(info )
	@:$(info Optional env or cmd-line variables:)
	@:$(info )
	@:$(info BOARD   = board option to give to `west build -b`.)
	@:$(info )
	@:$(info TARGETS:)
	@:$(info )
	@:$(info all/build     : build the Docker image.)
	@:$(info shell         : drop into the Docker container.)
	@:$(info clean         : clean the Docker image.)
	@:$(info )
	@:$(info where mcu = [ main_board | sec_board ])
	@:$(info <mcu>-build     : build MCU code.)
	@:$(info <mcu>-flash     : flash MCU code (also builds if necessary).)
	@:$(info <mcu>-clean     : clean MCU code.)
	@:$(info <mcu>-tests     : build and run MCU tests.)
	@:$(info )
	@:$(info )
	@:$(info Examples:)
	@:$(info )
	@:$(info make main_mcu-build REPO_DIR=/home/pdietl/firmware)
	@:$(info make main_mcu-tests REPO_DIR=/home/pdietl/firmware)

.PHONY: all build
all build: .docker_image_built

.docker_image_built: Dockerfile Makefile
	docker build . -t $(DOCKER_TAG)
	touch $@

else

ifndef REPO_DIR
$(error REPO_DIR must be given in env or cmd line!)
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

%-build:
	$(DOCKER_CMD) /bin/bash -c -- \
		'cd $(REPO_DIR)/orb/$*/app && west build $(BOARD_CMD)'

%-flash:
	$(DOCKER_CMD) /bin/bash -c -- \
		'cd $(REPO_DIR)/orb/$*/app && west build $(BOARD_CMD) && su-exec root west flash'

%-clean:
	cd $(REPO_DIR)/orb/$*/app && rm -rf build

%-tests:
	$(DOCKER_CMD) /bin/bash -c -- \
		'source $(REPO_DIR)/zephyr/zephyr-env.sh && cd $(REPO_DIR)/orb/$*/app && twister -T . -vv -c'

.PHONY: format
format:
	$(DOCKER_CMD) /bin/bash -c -- \
		'cd orb && pre-commit run --all-files --config utils/format/pre-commit-config.yaml'

.PHONY: clean
clean:
	$(RM) .docker_image_built

endif # ($(MAKECMDGOALS),help)
