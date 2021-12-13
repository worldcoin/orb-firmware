DOCKER_VER := 2
DOCKER_TAG := ghcr.io/worldcoin/mcu-zephyr-build:$(DOCKER_VER)
REPO_DIR ?= $(HOME)/firmware

.PHONY: all build
all build: .docker_image_built

.docker_image_built: Dockerfile Makefile
	docker build . -t $(DOCKER_TAG)
	touch $@

.PHONY: shell
shell:
	docker run -ti --rm --privileged \
		-u$(shell id -u):$(shell id -g) \
		-v /etc/group:/etc/group:ro \
		-v /etc/passwd:/etc/passwd:ro \
		-v $(HOME)/.ssh:$(HOME)/.ssh:ro \
		-v $(HOME)/.ccache:$(HOME)/.ccache \
		-v $(REPO_DIR):$(REPO_DIR) \
		-e REPO_DIR=$(REPO_DIR) \
		-w $(REPO_DIR) \
		$(DOCKER_TAG) \
		/bin/bash

.PHONY: clean
clean:
	$(RM) .docker_image_built
