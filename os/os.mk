# Build:
# - First-stage boot loader
# - Device tree
# - U-boot
# - Linux kernel

-include $(OS_PATH)/board.mk
ifneq ("$(wildcard $(BOARD_PATH)/board.mk)","")
-include $(BOARD_PATH)/board.mk
endif
ZYNQ_TYPE ?= zynq
VIVADO_VER ?= $(VIVADO_VERSION)
include $(OS_PATH)/$(ZYNQ_TYPE).mk


PATCHES := $(BOARD_PATH)/patches
PROC := ps7_cortexa9_0
HSI := source $(VIVADO_PATH)/$(VIVADO_VER)/settings64.sh && hsi -nolog -nojournal -mode batch
BOOTGEN := source $(VIVADO_PATH)/$(VIVADO_VER)/settings64.sh && bootgen

BOARD := $(shell basename $(BOARD_PATH))

TMP_OS_PATH := $(TMP_PROJECT_PATH)/os

UBOOT_PATH := $(TMP_OS_PATH)/u-boot-xlnx-$(UBOOT_TAG)
LINUX_PATH := $(TMP_OS_PATH)/linux-xlnx-$(LINUX_TAG)
DTREE_PATH := $(TMP_OS_PATH)/device-tree-xlnx-$(DTREE_TAG)

UBOOT_TAR := $(TMP)/u-boot-xlnx-$(UBOOT_TAG).tar.gz
LINUX_TAR := $(TMP)/linux-xlnx-$(LINUX_TAG).tar.gz
DTREE_TAR := $(TMP)/device-tree-xlnx-$(DTREE_TAG).tar.gz

TMP_OS_VERSION_FILE := $(TMP_OS_PATH)/version.json

$(TMP_OS_VERSION_FILE): $(KOHERON_VERSION_FILE)
	echo '{ "version": "$(KOHERON_VERSION)" }' > $@

DTREE_SWITCH = $(TMP_OS_PATH)/devicetree.dtb
ifdef DTREE_OVERRIDE
DTREE_SWITCH = $(TMP_OS_PATH)/devicetree_$(DTREE_LOC) 
endif

.PHONY: os
os: $(INSTRUMENT_ZIP) www api $(TMP_OS_PATH)/boot.bin $(TMP_OS_PATH)/$(LINUX_IMAGE) $(DTREE_SWITCH) $(TMP_OS_VERSION_FILE)

# Build image (run as root)
.PHONY: image
image:
	bash $(OS_PATH)/scripts/ubuntu-$(MODE).sh $(TMP_PROJECT_PATH) $(OS_PATH) $(TMP_OS_PATH) $(NAME) $(TMP_OS_VERSION_FILE) $(ZYNQ_TYPE)

.PHONY: clean_os
clean_os:
	rm -rf $(TMP_OS_PATH)

###############################################################################
# First-stage boot loader
###############################################################################

# Additional files (including fsbl_hooks.c) can be added to the FSBL in $(BOARD_PATH)/patches/fsbl
FSBL_FILES := $(wildcard $(BOARD_PATH)/patches/fsbl/*.h $(BOARD_PATH)/patches/fsbl/*.c)

.PHONY: fsbl
fsbl: $(TMP_OS_PATH)/fsbl/executable.elf

$(TMP_OS_PATH)/fsbl/Makefile: $(TMP_FPGA_PATH)/$(NAME).hwdef
	mkdir -p $(@D)
	$(HSI) -source $(FPGA_PATH)/hsi/fsbl.tcl -tclargs $(NAME) $(PROC) $(TMP_OS_PATH)/hard $(@D) $< $(ZYNQ_TYPE)
	@echo [$@] OK

$(TMP_OS_PATH)/fsbl/executable.elf: $(TMP_OS_PATH)/fsbl/Makefile $(FSBL_FILES)
	cp -a $(BOARD_PATH)/patches/fsbl/. $(TMP_OS_PATH)/fsbl/ 2>/dev/null || true
	source $(VIVADO_PATH)/$(VIVADO_VER)/settings64.sh && make -C $(@D) all

.PHONY: clean_fsbl
clean_fsbl:
	rm -rf $(TMP_OS_PATH)/fsbl

###############################################################################
# U-Boot
###############################################################################

$(UBOOT_TAR):
	mkdir -p $(@D)
	curl -L $(UBOOT_URL) -o $@
	@echo [$@] OK

$(UBOOT_PATH): $(UBOOT_TAR)
	mkdir -p $@
	tar -zxf $< --strip-components=1 --directory=$@
	@echo [$@] OK

ifeq ("$(UBOOT_CONFIG)","")
UBOOT_CONFIG = zynq_$(BOARD)_defconfig
endif

$(TMP_OS_PATH)/u-boot.elf: $(UBOOT_PATH) $(shell find $(PATCHES) -type f)
	cp -a $(PATCHES)/${UBOOT_CONFIG} $(UBOOT_PATH)/ 2>/dev/null || true
	cp -a $(PATCHES)/u-boot/. $(UBOOT_PATH)/ 2>/dev/null || true
	mkdir -p $(@D)
	make -C $< mrproper
	make -C $< arch=arm $(UBOOT_CONFIG)
	make -C $< arch=arm CFLAGS="-O2 $(GCC_FLAGS)" \
	  CROSS_COMPILE=$(GCC_ARCH)- all
	cp $</u-boot $@
	@echo [$@] OK

###############################################################################
# boot.bin
###############################################################################

$(TMP_OS_PATH)/boot.bin: $(TMP_OS_PATH)/fsbl/executable.elf $(BITSTREAM) $(TMP_OS_PATH)/u-boot.elf
	echo "img:{[bootloader] $^}" > $(TMP_OS_PATH)/boot.bif
	$(BOOTGEN) -image $(TMP_OS_PATH)/boot.bif -arch $(ZYNQ_TYPE) -w -o i $@
	@echo [$@] OK

###############################################################################
# DEVICE TREE
###############################################################################

$(DTREE_TAR):
	mkdir -p $(@D)
	curl -L $(DTREE_URL) -o $@
	@echo [$@] OK

$(DTREE_PATH): $(DTREE_TAR)
	mkdir -p $@
	tar -zxf $< --strip-components=1 --directory=$@
	@echo [$@] OK

.PHONY: overlay_dtree
overlay_dtree: $(TMP_OS_PATH)/overlay/system-top.dts

.PHONY: devicetree
devicetree: $(TMP_OS_PATH)/devicetree/system-top.dts

$(TMP_OS_PATH)/overlay/system-top.dts: $(TMP_FPGA_PATH)/$(NAME).hwdef $(DTREE_PATH) $(PATCHES)/overlay.patch
	mkdir -p $(@D)
	$(HSI) -source $(FPGA_PATH)/hsi/devicetree.tcl -tclargs $(NAME) $(PROC) $(DTREE_PATH) $(VIVADO_VER) \
	  $(TMP_OS_PATH)/hard $(TMP_OS_PATH)/overlay $(TMP_FPGA_PATH)/$(NAME).hwdef true
	cp -R $(TMP_OS_PATH)/overlay $(TMP_OS_PATH)/overlay.orig
	patch -d $(TMP_OS_PATH) -p -0 < $(PATCHES)/overlay.patch 
	@echo [$@] OK

$(TMP_OS_PATH)/devicetree/system-top.dts: $(TMP_FPGA_PATH)/$(NAME).hwdef $(DTREE_PATH) $(PATCHES)/devicetree.patch
	mkdir -p $(@D)
	$(HSI) -source $(FPGA_PATH)/hsi/devicetree.tcl -tclargs $(NAME) $(PROC) $(DTREE_PATH) $(VIVADO_VER) \
	  $(TMP_OS_PATH)/hard $(TMP_OS_PATH)/devicetree $(TMP_FPGA_PATH)/$(NAME).hwdef false
	cp -R $(TMP_OS_PATH)/devicetree $(TMP_OS_PATH)/devicetree.orig
	patch -d $(TMP_OS_PATH) -p -0 < $(PATCHES)/devicetree.patch 
	@echo [$@] OK

.PHONY: clean_devicetree
clean_devicetree:
	rm -rf $(TMP_OS_PATH)/devicetree $(TMP_OS_PATH)/devicetree.orig

.PHONY: patch_devicetree
patch_devicetree:
	bash os/scripts/patch_devicetree.sh $(TMP_OS_PATH) $(BOARD_PATH)

###############################################################################
# LINUX
###############################################################################

$(LINUX_TAR):
	mkdir -p $(@D)
	curl -L $(LINUX_URL) -o $@
	@echo [$@] OK

$(LINUX_PATH): $(LINUX_TAR)
	mkdir -p $@
	tar -zxf $< --strip-components=1 --directory=$@
	@echo [$@] OK

$(TMP_OS_PATH)/$(LINUX_IMAGE): $(LINUX_PATH) $(OS_PATH)/xilinx_$(ZYNQ_TYPE)_defconfig
	make -C $< mrproper
	make -C $< ARCH=$(ARCH) xilinx_$(ZYNQ_TYPE)_defconfig
	make -C $< ARCH=$(ARCH) CFLAGS="-O2 $(GCC_FLAGS)" \
	  --jobs=$(N_CPUS) \
	  CROSS_COMPILE=$(GCC_ARCH)- UIMAGE_LOADADDR=0x8000 $(LINUX_IMAGE)
	cp $</arch/$(ARCH)/boot/$(LINUX_IMAGE) $@
	@echo [$@] OK




$(TMP_OS_PATH)/overlay.dtb: $(TMP_OS_PATH)/overlay/system-top.dts
	sed -i 's/.bin/$(NAME).bit.bin/g' $(TMP_OS_PATH)/overlay/pl.dtsi
	$(OS_PATH)/dtc-1.5.0/dtc -O dtb -o $@ \
	  -i $(TMP_OS_PATH)/overlay -b 0 -@ $(TMP_OS_PATH)/overlay/pl.dtsi
	@echo [$@] OK

$(TMP_OS_PATH)/devicetree.dtb: $(TMP_OS_PATH)/devicetree/system-top.dts
	$(OS_PATH)/dtc-1.5.0/dtc -I dts -O dtb -o $@ \
	  -i $(TMP_OS_PATH)/devicetree $(TMP_OS_PATH)/devicetree/system-top.dts
	@echo [$@] OK

$(TMP_OS_PATH)/devicetree_linux: $(TMP_OS_PATH)/$(LINUX_IMAGE)
	echo ${DTREE_OVERRIDE}
	cp -a $(PATCHES)/linux/. $(LINUX_PATH)/ 2>/dev/null || true
	make -C $(LINUX_PATH) ARCH=$(ARCH) CROSS_COMPILE=$(GCC_ARCH)- dtbs -j$(N_CPUS)
	cp $(LINUX_PATH)/${DTREE_OVERRIDE} $(TMP_OS_PATH)/devicetree.dtb
	@echo [$(TMP_OS_PATH)/devicetree.dtb] OK

$(TMP_OS_PATH)/devicetree_uboot: $(TMP_OS_PATH)/u-boot.elf
	echo ${DTREE_OVERRIDE}
	cp $(UBOOT_PATH)/${DTREE_OVERRIDE} $(TMP_OS_PATH)/devicetree.dtb
	@echo [$(TMP_OS_PATH)/devicetree.dtb] OK

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
	curl https://www.koheron.com/static/css/main.css -o $@

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
	curl https://www.koheron.com/static/images/website/koheron.svg -o $@

$(TMP_WWW_PATH)/koheron_logo.svg:
	mkdir -p $(@D)
	curl https://www.koheron.com/static/images/website/koheron_logo.svg -o $@

$(TMP_WWW_PATH)/kbird.ico:
	mkdir -p $(@D)
	curl https://www.koheron.com/static/images/website/kbird.ico -o $@

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
