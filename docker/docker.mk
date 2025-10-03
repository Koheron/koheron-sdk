DOCKER_IMAGE ?= cross-armhf:24.04
DOCKER_WD    ?= /home/containeruser/wkspace
DOCKER_UID    = $(shell id -u)
DOCKER_GID    = $(shell id -g)

# Pass proxy vars if present; set HOME to a writable path (the mounted workspace)
DOCKER_ENV    = -e HOME=$(DOCKER_WD) \
                -e http_proxy -e https_proxy -e no_proxy \
                -e TZ=$(shell cat /etc/timezone 2>/dev/null || echo UTC)

# You use losetup/formatting; give the container access to /dev and needed caps
# Mount /run and /tmp as exec so qemu works inside chroots
DOCKER_DEV    = --privileged -v /dev:/dev \
                --tmpfs /run:exec --tmpfs /tmp:exec

# Mount your source; :Z keeps SELinux happy on Fedora/RHEL hosts
DOCKER_VOL    = -v $(SDK_FULL_PATH):$(DOCKER_WD):Z

# Optional: preserve git identity inside the container
# DOCKER_VOL   += -v $(HOME)/.gitconfig:/etc/gitconfig:ro

# Optional: ccache (if you use it)
# DOCKER_VOL   += -v $(HOME)/.ccache:/home/containeruser/.ccache

DOCKER_FLAGS ?= --cpus=$(N_CPUS)

# Host cache directory (persistent across runs)
CCACHE_DIR ?= $(TMP_PROJECT_PATH)/.ccache

DOCKER_CCACHE_FLAGS = \
  -e CCACHE_DIR=$(DOCKER_WD)/.ccache \
  -e CCACHE_BASEDIR=$(DOCKER_WD) \
  -e CCACHE_COMPRESS=1 \
  -e CCACHE_MAXSIZE=10G \
  -e CCACHE_TEMPDIR=/tmp/ccache_tmp \
  -e CCACHE_NOHARDLINK=true \
  -e CCACHE_NOFILECLONE=true

DOCKER ?= docker run --rm $(DOCKER_DEV) $(DOCKER_ENV) \
         -u $(DOCKER_UID):$(DOCKER_GID) -w $(DOCKER_WD) \
         $(DOCKER_VOL) $(DOCKER_CCACHE_FLAGS) $(DOCKER_FLAGS) $(DOCKER_IMAGE)

DOCKER_MAKE = $(DOCKER) env -u MAKEFLAGS MAKEFLAGS="-j$(N_CPUS) --output-sync=target" make

# Expose host IDs so scripts can chown outputs created as root inside the container
DOCKER_ENV  += -e HOST_UID=$(DOCKER_UID) -e HOST_GID=$(DOCKER_GID)

# root-runner: same flags, but run as root (no -u)
# Reuse DOCKER_ENV (HOME/proxies/TZ + HOST_UID/GID), keep exec tmpfs
DOCKER_ROOT = docker run --rm -t $(DOCKER_DEV) \
               $(DOCKER_ENV) \
               -w $(DOCKER_WD) $(DOCKER_VOL) $(DOCKER_CCACHE_FLAGS) \
               --cpus=$(N_CPUS) $(DOCKER_FLAGS) $(DOCKER_IMAGE)

# Create and fix perms from inside the container as root
.PHONY: ccache-prepare
ccache-prepare:
	$(DOCKER_ROOT) bash -lc '\
	  install -d -m 0775 $(DOCKER_WD)/.ccache && \
	  chown -R $(DOCKER_UID):$(DOCKER_GID) $(DOCKER_WD)/.ccache && \
	  chmod -R u+rwX,g+rwX $(DOCKER_WD)/.ccache'

.PHONY: ccache-check
ccache-check:
	$(DOCKER) sh -lc 'id; ls -ld $$CCACHE_DIR; stat -c "%u:%g %a" $$CCACHE_DIR; \
	                  mkdir -p $$CCACHE_TEMPDIR; \
	                  test -w $$CCACHE_DIR || echo "not writable"; \
	                  echo ok'

.PHONY: clean_docker
clean_docker:
	@id=$$(docker images -q $(DOCKER_IMAGE) 2>/dev/null); \
	if [ -n "$$id" ]; then \
	  echo "Removing containers using $$id..."; \
	  docker ps -aq --filter ancestor=$$id | xargs -r docker rm -f; \
	  echo "Removing image $(DOCKER_IMAGE) ($$id)..."; \
	  docker rmi -f $$id; \
	else \
	  echo "Image $(DOCKER_IMAGE) not found."; \
	fi