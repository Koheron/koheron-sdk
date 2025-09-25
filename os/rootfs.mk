###############################################################################
# HTTP API
###############################################################################

TMP_API_PATH := $(TMP)/api

API_FILES := \
  $(TMP_API_PATH)/wsgi.py \
  $(TMP_API_PATH)/app/__init__.py \
  $(TMP_API_PATH)/app/install_instrument.sh

.PHONY: api
api: $(API_FILES)

$(TMP_API_PATH)/wsgi.py: $(OS_PATH)/api/wsgi.py
	# create parents and copy
	install -D -m0644 $< $@

$(TMP_API_PATH)/app/%: $(OS_PATH)/api/%
	# create parents and copy
	install -D -m0644 $< $@

PASSWORD ?= changeme

.PHONY: api_sync
api_sync: $(API_FILES)
	sshpass -p "$(PASSWORD)" rsync -avz -e "ssh -i /ssh-private-key" "$(TMP_API_PATH)/." "root@$(HOST):/usr/local/api/"
	sshpass -p "$(PASSWORD)" ssh -i /ssh-private-key "root@$(HOST)" 'systemctl daemon-reload || true; systemctl reload-or-restart uwsgi || true'

.PHONY: api_clean
api_clean:
	rm -rf $(TMP_API_PATH)

###############################################################################
# WWW
###############################################################################

WWW_PATH:= $(OS_PATH)/www
TMP_WWW_PATH:= $(TMP)/www

WWW_ASSETS := \
  $(TMP_WWW_PATH)/koheron.css \
  $(TMP_WWW_PATH)/instruments.js \
  $(TMP_WWW_PATH)/index.html \
  $(TMP_WWW_PATH)/main.css \
  $(TMP_WWW_PATH)/bootstrap.min.js \
  $(TMP_WWW_PATH)/bootstrap.min.css \
  $(TMP_WWW_PATH)/jquery.min.js \
  $(TMP_WWW_PATH)/koheron.svg \
  $(TMP_WWW_PATH)/koheron_logo.svg \
  $(TMP_WWW_PATH)/kbird.ico \
  $(TMP_WWW_PATH)/lato-v11-latin-400.woff2 \
  $(TMP_WWW_PATH)/lato-v11-latin-700.woff2 \
  $(TMP_WWW_PATH)/lato-v11-latin-900.woff2 \
  $(TMP_WWW_PATH)/glyphicons-halflings-regular.woff2 \
  $(TMP_WWW_PATH)/navigation.html \
  $(TMP_WWW_PATH)/html-imports.min.js \
  $(TMP_WWW_PATH)/html-imports.min.js.map

.PHONY: www
www : $(WWW_ASSETS)

.PHONY: www_sync
www_sync: www
	sshpass -p "$(PASSWORD)" rsync -avz -e "ssh -i /ssh-private-key" "$(TMP_WWW_PATH)/." "root@$(HOST):/usr/local/www/"

.PHONY: clean_www
clean_www:
	rm -rf $(TMP_WWW_PATH)

WWW_TS_FILES := $(WEB_PATH)/koheron.ts
WWW_TS_FILES += $(WWW_PATH)/instruments.ts
WWW_TS_FILES += $(WWW_PATH)/instruments_widget.ts
WWW_TS_FILES += $(WWW_PATH)/koheron_server_log.ts
WWW_TS_FILES += $(WWW_PATH)/koheron_system.ts
WWW_TS_FILES += $(WWW_PATH)/system_info_widget.ts

$(TMP_WWW_PATH)/instruments.js: $(WWW_TS_FILES)
	mkdir -p $(@D)
	$(TSC) $^ --outFile $@

$(TMP_WWW_PATH)/koheron.css:
	mkdir -p $(@D)
	curl https://assets.koheron.com/css/main.css -o $@

$(TMP_WWW_PATH)/index.html: $(WWW_PATH)/index.html
	mkdir -p $(@D)
	cp $< $@

$(TMP_WWW_PATH)/navigation.html: $(WEB_PATH)/navigation.html
	mkdir -p $(@D)
	cp $< $@

$(TMP_WWW_PATH)/main.css: $(WEB_PATH)/main.css
	mkdir -p $(@D)
	cp $< $@

$(TMP_WWW_PATH)/bootstrap.min.js:
	mkdir -p $(@D)
	curl https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js -o $@

$(TMP_WWW_PATH)/bootstrap.min.css:
	mkdir -p $(@D)
	curl https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css -o $@

$(TMP_WWW_PATH)/jquery.min.js:
	mkdir -p $(@D)
	curl https://code.jquery.com/jquery-1.12.4.min.js -o $@

$(TMP_WWW_PATH)/koheron.svg:
	mkdir -p $(@D)
	curl https://assets.koheron.com/images/logo/koheron.svg -o $@

$(TMP_WWW_PATH)/koheron_logo.svg:
	mkdir -p $(@D)
	curl https://assets.koheron.com/images/logo/koheron_logo.svg -o $@

$(TMP_WWW_PATH)/kbird.ico:
	mkdir -p $(@D)
	curl https://assets.koheron.com/images/logo/koheron.ico -o $@

$(TMP_WWW_PATH)/lato-v11-latin-400.woff2:
	mkdir -p $(@D)
	curl https://fonts.gstatic.com/s/lato/v13/1YwB1sO8YE1Lyjf12WNiUA.woff2 -o $@

$(TMP_WWW_PATH)/lato-v11-latin-700.woff2:
	mkdir -p $(@D)
	curl https://fonts.gstatic.com/s/lato/v13/H2DMvhDLycM56KNuAtbJYA.woff2 -o $@

$(TMP_WWW_PATH)/lato-v11-latin-900.woff2:
	mkdir -p $(@D)
	curl https://fonts.gstatic.com/s/lato/v13/tI4j516nok_GrVf4dhunkg.woff2 -o $@

$(TMP_WWW_PATH)/glyphicons-halflings-regular.woff2:
	mkdir -p $(@D)
	curl https://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/fonts/glyphicons-halflings-regular.woff2 -o $@

$(TMP_WWW_PATH)/html-imports.min.js:
	mkdir -p $(@D)
	curl https://raw.githubusercontent.com/webcomponents/html-imports/master/html-imports.min.js -o $@

$(TMP_WWW_PATH)/html-imports.min.js.map:
	mkdir -p $(@D)
	curl https://raw.githubusercontent.com/webcomponents/html-imports/master/html-imports.min.js.map -o $@

###############################################################################
# ROOTFS
###############################################################################

UBUNTU_VERSION ?= 24.04.3
ifeq ($(ZYNQ_TYPE),zynqmp)
  UBUNTU_ARCH := arm64
  QEMU_BIN    := /usr/bin/qemu-aarch64-static
else
  UBUNTU_ARCH := armhf
  QEMU_BIN    := /usr/bin/qemu-arm-static
endif

ROOT_TAR := ubuntu-base-$(UBUNTU_VERSION)-base-$(UBUNTU_ARCH).tar.gz
ROOT_TAR_URL := https://cdimage.ubuntu.com/ubuntu-base/releases/$(UBUNTU_VERSION)/release/$(ROOT_TAR)
ROOT_TAR_PATH := $(TMP)/$(ROOT_TAR)

BASE_ROOTFS_TAR := $(TMP)/ubuntu-base-$(UBUNTU_VERSION)-base-koheron-$(UBUNTU_ARCH).tgz

OVERLAY_TAR := $(TMP_OS_PATH)/rootfs_overlay.tar

ABS_ROOT_TAR_PATH := $(abspath $(ROOT_TAR_PATH))
ABS_OVERLAY_TAR   := $(abspath $(OVERLAY_TAR))

OVERLAY_DIR := $(TMP_OS_PATH)/rootfs_overlay

$(ROOT_TAR_PATH):
	mkdir -p $(@D)
	curl -L $(ROOT_TAR_URL) -o $@
	$(call ok,$@)

###############################################################################
# ROOTFS OVERLAY
###############################################################################

# Ensure overlay root directory exists (for order-only prereqs)
$(OVERLAY_DIR)/:
	mkdir -p $@

GIT_IN_REPO := $(shell command -v git >/dev/null 2>&1 && git rev-parse --is-inside-work-tree >/dev/null 2>&1 && echo yes || echo no)

GIT_COMMIT   := $(shell [ "$(GIT_IN_REPO)" = yes ] && git rev-parse --short=12 HEAD 2>/dev/null || echo unknown)
GIT_BRANCH   := $(shell [ "$(GIT_IN_REPO)" = yes ] && git rev-parse --abbrev-ref HEAD 2>/dev/null   || echo unknown)
GIT_TAG      := $(shell [ "$(GIT_IN_REPO)" = yes ] && git describe --tags --abbrev=0 2>/dev/null     || echo none)
GIT_DESCRIBE := $(shell [ "$(GIT_IN_REPO)" = yes ] && git describe --tags --always --dirty=-dirty 2>/dev/null || echo unknown)
# "dirty" = any tracked changes (ignores untracked for reproducibility; change to --untracked-files=normal if you want)
GIT_DIRTY    := $(shell \
  if [ "$(GIT_IN_REPO)" = yes ]; then \
    if [ -n "$$(git status --porcelain --untracked-files=no 2>/dev/null)" ]; then echo yes; else echo no; fi; \
  else echo n/a; fi)

# ---------- Build ID: UTC timestamp + short commit + -dirty if needed ----------
BUILD_TIME_UTC := $(shell date -u +%Y%m%d.%H%M%S)
BUILD_REV      := $(GIT_COMMIT)
BUILD_DIRTY_SFX:= $(if $(filter yes,$(GIT_DIRTY)),-dirty,)
BUILD_ID       ?= $(BUILD_TIME_UTC).$(BUILD_REV)$(BUILD_DIRTY_SFX)

RELEASE_NAME := $(BOARD)-$(NAME)

# (keep your existing paths)
MANIFEST_TXT          := $(TMP_PROJECT_PATH)/manifest-$(RELEASE_NAME).txt

RELEASE_ZIP := $(TMP_PROJECT_PATH)/$(RELEASE_NAME).zip

# ---------- Manifest generation ----------
$(MANIFEST_TXT):
	@mkdir -p $(@D)
	@{ \
	  echo "release=$(RELEASE_NAME)"; \
	  echo "build_id=$(BUILD_ID)"; \
	  echo "project=$(NAME)"; \
	  echo "board=$(BOARD)"; \
	  echo "zynq=$(ZYNQ_TYPE)"; \
	  echo "kernel=$(LINUX_TAG)"; \
	  echo "u-boot=$(UBOOT_TAG)"; \
	  echo "device-tree=$(DTREE_TAG)"; \
	  echo "koheron_version=$(KOHERON_VERSION)"; \
	  echo "git_commit=$(GIT_COMMIT)"; \
	  echo "git_branch=$(GIT_BRANCH)"; \
	  echo "git_tag=$(GIT_TAG)"; \
	  echo "git_describe=$(GIT_DESCRIBE)"; \
	  echo "git_dirty=$(GIT_DIRTY)"; \
	  echo "generated_utc=$(shell date -u +%Y-%m-%dT%H:%M:%SZ)"; \
	} > $@

$(OVERLAY_DIR)/usr/local/share/koheron/manifest.txt: $(MANIFEST_TXT) | $(OVERLAY_DIR)/
	# ensure parents exist and copy
	install -D -m0644 $< $@

# ---------- /etc/koheron-release ----------
$(OVERLAY_DIR)/etc/koheron-release :
	@mkdir -p $(dir $@)
	@{ \
	  echo "NAME=Koheron"; \
	  echo "RELEASE=$(RELEASE_NAME)"; \
	  echo "BUILD_ID=$(BUILD_ID)"; \
	  echo "UBUNTU=$(UBUNTU_VERSION)"; \
	  echo "KOHERON_VERSION=$(KOHERON_VERSION)"; \
	  echo "GIT_COMMIT=$(GIT_COMMIT)"; \
	  echo "GIT_BRANCH=$(GIT_BRANCH)"; \
	  echo "GIT_TAG=$(GIT_TAG)"; \
	  echo "GIT_DESCRIBE=$(GIT_DESCRIBE)"; \
	  echo "GIT_DIRTY=$(GIT_DIRTY)"; \
	} > $@
	@chmod 0644 $@

# Stage WWW
$(OVERLAY_DIR)/usr/local/www/.stamp: $(WWW_ASSETS) | $(OVERLAY_DIR)/
	# ensure destination exists, then stage
	mkdir -p $(OVERLAY_DIR)/usr/local/www
	rsync -a $(TMP_WWW_PATH)/ $(OVERLAY_DIR)/usr/local/www/
	touch $@

# Stage API
$(OVERLAY_DIR)/usr/local/api/.stamp: $(API_FILES) | $(OVERLAY_DIR)/
	# ensure destination exists, then stage
	mkdir -p $(OVERLAY_DIR)/usr/local/api
	rsync -a $(TMP_API_PATH)/ $(OVERLAY_DIR)/usr/local/api/
	touch $@

# Koheron server bits
$(OVERLAY_DIR)/usr/local/koheron-server/koheron-server-init.py: $(OS_PATH)/scripts/koheron-server-init.py
	install -D -m0755 $< $@

# Systemd units
$(OVERLAY_DIR)/etc/systemd/system/%: $(OS_PATH)/systemd/%
	install -D -m0644 $< $@

# uwsgi
$(OVERLAY_DIR)/etc/uwsgi/uwsgi.ini: $(OS_PATH)/config/uwsgi.ini
	install -D -m0644 $< $@

# grow-rootfs-once
$(OVERLAY_DIR)/usr/local/sbin/grow-rootfs-once: $(OS_PATH)/scripts/grow-rootfs-once
	install -D -m0755 $< $@

# Instruments
$(OVERLAY_DIR)/usr/local/instruments/unzip_default_instrument.sh: $(OS_PATH)/scripts/unzip_default_instrument.sh
	install -D -m0755 $< $@

# --- copy all (or selected) instruments into overlay; create a stamp ---
COPY_INSTRUMENTS ?=
COPY_INSTR_FILES := $(addprefix $(TMP)/$(BOARD)/instruments/,$(addsuffix .zip,$(COPY_INSTRUMENTS)))

$(OVERLAY_DIR)/usr/local/instruments/.stamp: $(COPY_INSTR_FILES)
	@mkdir -p $(OVERLAY_DIR)/usr/local/instruments
	@if [ -n "$^" ]; then cp -f $^ $(OVERLAY_DIR)/usr/local/instruments/; fi
	@touch $@

$(OVERLAY_DIR)/usr/local/instruments/$(NAME).zip: $(TMP_PROJECT_PATH)/$(NAME).zip
	install -D -m0644 $< $@

$(OVERLAY_DIR)/usr/local/instruments/default: | $(OVERLAY_DIR)/
	# ensure parent dir exists and write default name
	@mkdir -p $(dir $@)
	echo "$(NAME).zip" > $@

# nginx
$(OVERLAY_DIR)/etc/nginx/nginx.conf: $(OS_PATH)/config/nginx.conf
	install -D -m0644 $< $@

$(OVERLAY_DIR)/etc/nginx/sites-available/koheron.conf: $(OS_PATH)/config/nginx-server.conf
	install -D -m0644 $< $@

# Bundle overlay as a single tar the script can explode into /
OVERLAY_FILES := \
  $(OVERLAY_DIR)/usr/local/instruments/.stamp \
  $(OVERLAY_DIR)/usr/local/share/koheron/manifest.txt \
  $(OVERLAY_DIR)/etc/koheron-release \
  $(OVERLAY_DIR)/usr/local/www/.stamp \
  $(OVERLAY_DIR)/usr/local/api/.stamp \
  $(OVERLAY_DIR)/usr/local/koheron-server/koheron-server-init.py \
  $(OVERLAY_DIR)/etc/systemd/system/unzip-default-instrument.service \
  $(OVERLAY_DIR)/etc/systemd/system/koheron-server.service \
  $(OVERLAY_DIR)/etc/systemd/system/koheron-server-init.service \
  $(OVERLAY_DIR)/etc/uwsgi/uwsgi.ini \
  $(OVERLAY_DIR)/etc/systemd/system/uwsgi.service \
  $(OVERLAY_DIR)/etc/systemd/system/uwsgi.socket \
  $(OVERLAY_DIR)/usr/local/sbin/grow-rootfs-once \
  $(OVERLAY_DIR)/etc/systemd/system/grow-rootfs-once.service \
  $(OVERLAY_DIR)/usr/local/instruments/unzip_default_instrument.sh \
  $(OVERLAY_DIR)/usr/local/instruments/$(NAME).zip \
  $(OVERLAY_DIR)/usr/local/instruments/default \
  $(OVERLAY_DIR)/etc/nginx/nginx.conf \
  $(OVERLAY_DIR)/etc/nginx/sites-available/koheron.conf \
  $(OVERLAY_DIR)/etc/systemd/system/nginx.service

$(OVERLAY_TAR): $(OVERLAY_FILES) | $(OVERLAY_DIR)/
	# ensure destination dir for tar exists
	mkdir -p $(@D)
	tar -C $(OVERLAY_DIR) \
	    --owner=0 --group=0 --numeric-owner \
	    --mtime='UTC 1970-01-01' \
	    -cf $@ .
	$(call ok,$@)

.PHONY: clean_overlay_rootfs
clean_overlay_rootfs:
	@echo "Cleaning overlay artifacts…"
	@rm -rf $(OVERLAY_DIR) $(OVERLAY_TAR) \
	       $(OVERLAY_DIR)/usr/local/share/koheron/manifest.txt \
	       $(OVERLAY_DIR)/etc/koheron-release \
	       $(OVERLAY_DIR)/usr/local/www/.stamp \
	       $(OVERLAY_DIR)/usr/local/api/.stamp
