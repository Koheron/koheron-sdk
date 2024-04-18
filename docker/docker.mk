DOCKER :=
ifeq ($(BUILD_METHOD),docker)
	UID = $(shell id -u)
	GID = $(shell id -g)
	DOCKER = docker run --rm -v $(SDK_FULL_PATH):/home/containeruser/wkspace:Z -u $(UID):$(GID) -w /home/containeruser/wkspace gnu-gcc-9.5
endif