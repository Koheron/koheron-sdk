###############################################################################
# HTTP API
###############################################################################

TMP_API_PATH := $(TMP)/api

.PHONY:
api: $(TMP_API_PATH)/wsgi.py \
        $(TMP_API_PATH)/app/__init__.py \
        $(TMP_API_PATH)/app/install_instrument.sh

$(TMP_API_PATH)/wsgi.py: $(OS_PATH)/api/wsgi.py
	mkdir -p $(@D)
	cp $< $@

$(TMP_API_PATH)/app/%: $(OS_PATH)/api/%
	mkdir -p $(@D)
	cp $< $@

# run `systemctl restart uwsgi` on the board
.PHONY:
api_sync: api
	rsync -avz -e "ssh -i /ssh-private-key" $(TMP_API_PATH)/. root@$(HOST):/usr/local/api/

.PHONY:
api_clean:
	rm -rf $(TMP_API_PATH)

###############################################################################
# WWW
###############################################################################

WWW_PATH:= $(OS_PATH)/www
TMP_WWW_PATH:= $(TMP)/www

.PHONY: www
www : $(TMP_WWW_PATH)/koheron.css \
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

.PHONY: www_sync
www_sync: www
	rsync -avz -e "ssh -i /ssh-private-key" $(TMP_WWW_PATH)/. root@$(HOST):/usr/local/www/

.PHONY: clean_www
clean_www:
	rm -rf $(TMP_WWW_PATH)

WWW_TS_FILES := $(WEB_PATH)/koheron.ts
WWW_TS_FILES += $(WWW_PATH)/instruments.ts
WWW_TS_FILES += $(WWW_PATH)/instruments_widget.ts

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
	curl http://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/js/bootstrap.min.js -o $@

$(TMP_WWW_PATH)/bootstrap.min.css:
	mkdir -p $(@D)
	curl http://maxcdn.bootstrapcdn.com/bootstrap/3.3.7/css/bootstrap.min.css -o $@

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
ROOT_TAR_URL := http://cdimage.ubuntu.com/ubuntu-base/releases/$(UBUNTU_VERSION)/release/$(ROOT_TAR)
ROOT_TAR_PATH := $(TMP)/$(ROOT_TAR)
OVERLAY_TAR := $(TMP_OS_PATH)/rootfs_overlay.tar

ABS_ROOT_TAR_PATH := $(abspath $(ROOT_TAR_PATH))
ABS_OVERLAY_TAR   := $(abspath $(OVERLAY_TAR))

OVERLAY_DIR := $(TMP_OS_PATH)/rootfs_overlay

$(ROOT_TAR_PATH):
	mkdir -p $(@D)
	curl -L $(ROOT_TAR_URL) -o $@
	@echo [$@] OK

###############################################################################
# ROOTFS OVERLAY
###############################################################################

# Stage WWW
$(OVERLAY_DIR)/usr/local/www/.stamp: $(WWW_ASSETS) $(TMP_OS_VERSION_FILE)
	mkdir -p $(@D)
	rsync -a $(TMP_WWW_PATH)/ $(OVERLAY_DIR)/usr/local/www/
	cp $(TMP_OS_VERSION_FILE) $(OVERLAY_DIR)/usr/local/www/version.json
	touch $@

# Stage API
$(OVERLAY_DIR)/usr/local/api/.stamp: $(API_FILES)
	mkdir -p $(@D)
	rsync -a $(TMP_API_PATH)/ $(OVERLAY_DIR)/usr/local/api/
	touch $@

# Koheron server bits
$(OVERLAY_DIR)/usr/local/koheron-server/koheron-server-init.py: $(OS_PATH)/scripts/koheron-server-init.py
	install -D -m 0755 $< $@

# Systemd units
$(OVERLAY_DIR)/etc/systemd/system/%: $(OS_PATH)/systemd/%
	install -D -m 0644 $< $@

# uwsgi
$(OVERLAY_DIR)/etc/uwsgi/uwsgi.ini: $(OS_PATH)/config/uwsgi.ini
	install -D -m 0644 $< $@

# grow-rootfs-once
$(OVERLAY_DIR)/usr/local/sbin/grow-rootfs-once: $(OS_PATH)/scripts/grow-rootfs-once
	install -D -m 0755 $< $@

# Instruments
$(OVERLAY_DIR)/usr/local/instruments/unzip_default_instrument.sh: $(OS_PATH)/scripts/unzip_default_instrument.sh
	install -D -m 0755 $< $@

$(OVERLAY_DIR)/usr/local/instruments/$(NAME).zip: $(TMP_PROJECT_PATH)/$(NAME).zip
	install -D -m 0644 $< $@

$(OVERLAY_DIR)/usr/local/instruments/default:
	mkdir -p $(@D)
	echo "$(NAME).zip" > $@

# nginx
$(OVERLAY_DIR)/etc/nginx/nginx.conf: $(OS_PATH)/config/nginx.conf
	install -D -m 0644 $< $@

$(OVERLAY_DIR)/etc/nginx/sites-available/koheron.conf: $(OS_PATH)/config/nginx-server.conf
	install -D -m 0644 $< $@

# Bundle overlay as a single tar the script can explode into /
OVERLAY_FILES := \
  $(OVERLAY_DIR)/usr/local/www/.stamp \
  $(OVERLAY_DIR)/usr/local/api/.stamp \
  $(OVERLAY_DIR)/usr/local/koheron-server/koheron-server-init.py \
  $(OVERLAY_DIR)/etc/systemd/system/unzip-default-instrument.service \
  $(OVERLAY_DIR)/etc/systemd/system/koheron-server.service \
  $(OVERLAY_DIR)/etc/systemd/system/koheron-server-init.service \
  $(OVERLAY_DIR)/etc/uwsgi/uwsgi.ini \
  $(OVERLAY_DIR)/etc/systemd/system/uwsgi.service \
  $(OVERLAY_DIR)/usr/local/sbin/grow-rootfs-once \
  $(OVERLAY_DIR)/etc/systemd/system/grow-rootfs-once.service \
  $(OVERLAY_DIR)/usr/local/instruments/unzip_default_instrument.sh \
  $(OVERLAY_DIR)/usr/local/instruments/$(NAME).zip \
  $(OVERLAY_DIR)/usr/local/instruments/default \
  $(OVERLAY_DIR)/etc/nginx/nginx.conf \
  $(OVERLAY_DIR)/etc/nginx/sites-available/koheron.conf \
  $(OVERLAY_DIR)/etc/systemd/system/nginx.service

$(OVERLAY_TAR): $(OVERLAY_FILES) | $(OVERLAY_DIR)/
	tar -C $(OVERLAY_DIR) -cf $@ .
	@echo [$@] OK