DOCKER_IMAGE ?= cross-armhf:24.04
DOCKER_WD    ?= /home/containeruser/wkspace
DOCKER_UID    = $(shell id -u)
DOCKER_GID    = $(shell id -g)

# Pass proxy vars if present; set HOME to a writable path (the mounted workspace)
DOCKER_ENV    = -e HOME=$(DOCKER_WD) \
                -e http_proxy -e https_proxy -e no_proxy \
                -e TZ=$(shell cat /etc/timezone 2>/dev/null || echo UTC)

# You use losetup/formatting; give the container access to /dev and needed caps
DOCKER_DEV    = --privileged -v /dev:/dev \
                --tmpfs /run --tmpfs /tmp

# Mount your source; :Z keeps SELinux happy on Fedora/RHEL hosts
DOCKER_VOL    = -v $(SDK_FULL_PATH):$(DOCKER_WD):Z

# Optional: preserve git identity inside the container
# DOCKER_VOL   += -v $(HOME)/.gitconfig:/etc/gitconfig:ro

# Optional: ccache (if you use it)
# DOCKER_VOL   += -v $(HOME)/.ccache:/home/containeruser/.ccache

DOCKER_FLAGS ?= --cpus=$(N_CPUS)

DOCKER ?= docker run --rm $(DOCKER_DEV) $(DOCKER_ENV) \
         -u $(DOCKER_UID):$(DOCKER_GID) -w $(DOCKER_WD) \
         $(DOCKER_VOL) $(DOCKER_FLAGS) $(DOCKER_IMAGE)

DOCKER_MAKE = $(DOCKER) env -u MAKEFLAGS MAKEFLAGS="-j$(N_CPUS) --output-sync=target" make

DOCKER_ENV  += -e HOST_UID=$(DOCKER_UID) -e HOST_GID=$(DOCKER_GID)

# root-runner: same flags, but run as root (no -u)
DOCKER_ROOT = docker run --rm -t --privileged -v /dev:/dev \
               --tmpfs /run --tmpfs /tmp \
               -e HOME=$(DOCKER_WD) -e http_proxy -e https_proxy -e no_proxy \
               -e TZ=$(shell cat /etc/timezone 2>/dev/null || echo UTC) \
               -w $(DOCKER_WD) -v $(SDK_FULL_PATH):$(DOCKER_WD):Z \
               --cpus=$(N_CPUS) $(DOCKER_FLAGS) $(DOCKER_IMAGE)